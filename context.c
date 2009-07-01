#include "context.h"
#include "object.h"
#include "utils.h"

static void
PYM_JSContextDealloc(PYM_JSContextObject *self)
{
  if (self->cx) {
    JS_DestroyContext(self->cx);
    self->cx = NULL;
  }

  Py_DECREF(self->runtime);
  self->runtime = NULL;

  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
PYM_getRuntime(PYM_JSContextObject *self, PyObject *args)
{
  Py_INCREF(self->runtime);
  return (PyObject *) self->runtime;
}

static PyObject *
PYM_newObject(PYM_JSContextObject *self, PyObject *args)
{
  JSObject *obj = JS_NewObject(self->cx, &PYM_JS_ObjectClass, NULL, NULL);
  if (obj == NULL) {
    PyErr_SetString(PYM_error, "JS_NewObject() failed");
    return NULL;
  }

  // If this fails, we don't need to worry about cleaning up
  // obj because it'll get cleaned up at the next GC.
  return (PyObject *) PYM_newJSObject(self, obj);
}

static PyObject *
PYM_getProperty(PYM_JSContextObject *self, PyObject *args)
{
#ifndef Py_UNICODE_WIDE
  PYM_JSObject *object;
  Py_UNICODE *string;

  if (!PyArg_ParseTuple(args, "O!u", &PYM_JSObjectType, &object,
                        &string))
    return NULL;

  JSString *jsString = JS_NewUCStringCopyZ(self->cx,
                                           (const jschar *) string);
  if (jsString == NULL) {
    PyErr_SetString(PYM_error, "JS_NewStringCopyZ() failed");
    return NULL;
  }

  jsval val;
  if (!JS_GetUCProperty(self->cx, object->obj,
                        JS_GetStringChars(jsString),
                        JS_GetStringLength(jsString), &val)) {
    // TODO: Get the actual JS exception. Any exception that exists
    // here will probably still be pending on the JS context.
    PyErr_SetString(PYM_error, "Getting property failed.");
    return NULL;
  }

  return PYM_jsvalToPyObject(self, val);
#else
  PyErr_SetString(PyExc_NotImplementedError,
                  "This function is not yet implemented for wide "
                  "unicode builds of Python.");
  return NULL;
#endif
}

static PyObject *
PYM_initStandardClasses(PYM_JSContextObject *self, PyObject *args)
{
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  if (!JS_InitStandardClasses(self->cx, object->obj)) {
    PyErr_SetString(PYM_error, "JS_InitStandardClasses() failed");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PYM_evaluateScript(PYM_JSContextObject *self, PyObject *args)
{
  PYM_JSObject *object;
  const char *source;
  int sourceLen;
  const char *filename;
  int lineNo;

  if (!PyArg_ParseTuple(args, "O!s#si", &PYM_JSObjectType, &object,
                        &source, &sourceLen, &filename, &lineNo))
    return NULL;

  JS_BeginRequest(self->cx);

  jsval rval;
  if (!JS_EvaluateScript(self->cx, object->obj, source, sourceLen,
                         filename, lineNo, &rval)) {
    // TODO: Actually get the error that was raised.
    PyErr_SetString(PYM_error, "Script failed");
    JS_EndRequest(self->cx);
    return NULL;
  }

  PyObject *pyRval = PYM_jsvalToPyObject(self, rval);

  JS_EndRequest(self->cx);

  return pyRval;
}

static JSBool dispatchJSFunctionToPython(JSContext *cx,
                                         JSObject *obj,
                                         uintN argc,
                                         jsval *argv,
                                         jsval *rval)
{
  jsval callee = JS_ARGV_CALLEE(argv);
  jsval jsCallable;
  if (!JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(callee), 0, &jsCallable)) {
    JS_ReportError(cx, "JS_GetReservedSlot() failed.");
    return JS_FALSE;
  }
  PyObject *callable = (PyObject *) JSVAL_TO_PRIVATE(jsCallable);

  // TODO: Convert args and 'this' parameter.
  PyObject *args = PyTuple_New(0);
  if (args == NULL) {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  PyObject *result = PyObject_Call(callable, args, NULL);
  if (result == NULL) {
    // TODO: Get the actual exception.
    JS_ReportError(cx, "Python function failed.");
    return JS_FALSE;
  }

  PyObject *success = PYM_pyObjectToJsval(cx, result, rval);
  Py_DECREF(result);

  if (success == NULL) {
    // TODO: Get the actual exception.
    JS_ReportError(cx, "Python function failed.");
    return JS_FALSE;
  }
  return JS_TRUE;
}

static PyObject *
PYM_defineFunction(PYM_JSContextObject *self, PyObject *args)
{
  PYM_JSObject *object;
  PyObject *callable;
  const char *name;

  if (!PyArg_ParseTuple(args, "O!Os", &PYM_JSObjectType, &object,
                        &callable, &name))
    return NULL;

  if (!PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "Callable must be callable");
    return NULL;
  }

  // TODO: Support unicode naming.
  JSFunction *func = JS_DefineFunction(self->cx, object->obj, name,
                                       dispatchJSFunctionToPython,
                                       0, JSPROP_ENUMERATE);
  if (func == NULL) {
    PyErr_SetString(PYM_error, "JS_DefineFunction() failed");
    return NULL;
  }

  JSObject *funcObj = JS_GetFunctionObject(func);

  if (funcObj == NULL) {
    PyErr_SetString(PYM_error, "JS_GetFunctionObject() failed");
    return NULL;
  }

  if (!JS_SetReservedSlot(self->cx, funcObj, 0,
                          PRIVATE_TO_JSVAL(callable))) {
    PyErr_SetString(PYM_error, "JS_SetReservedSlot() failed");
    return NULL;
  }

  // TODO: When to decref?
  Py_INCREF(callable);

  Py_RETURN_NONE;
}

static PyMethodDef PYM_JSContextMethods[] = {
  {"get_runtime", (PyCFunction) PYM_getRuntime, METH_VARARGS,
   "Get the JavaScript runtime associated with this context."},
  {"new_object", (PyCFunction) PYM_newObject, METH_VARARGS,
   "Create a new JavaScript object."},
  {"init_standard_classes",
   (PyCFunction) PYM_initStandardClasses, METH_VARARGS,
   "Add standard classes and functions to the given object."},
  {"evaluate_script",
   (PyCFunction) PYM_evaluateScript, METH_VARARGS,
   "Evaluate the given JavaScript code in the context of the given "
   "global object, using the given filename"
   "and line number information."},
  {"define_function",
   (PyCFunction) PYM_defineFunction, METH_VARARGS,
   "Defines a function callable from JS."},
  {"get_property", (PyCFunction) PYM_getProperty, METH_VARARGS,
   "Gets the given property for the given JavaScript object."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSContextType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Context",          /*tp_name*/
  sizeof(PYM_JSContextObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSContextDealloc,
  0,                           /*tp_print*/
  0,                           /*tp_getattr*/
  0,                           /*tp_setattr*/
  0,                           /*tp_compare*/
  0,                           /*tp_repr*/
  0,                           /*tp_as_number*/
  0,                           /*tp_as_sequence*/
  0,                           /*tp_as_mapping*/
  0,                           /*tp_hash */
  0,                           /*tp_call*/
  0,                           /*tp_str*/
  0,                           /*tp_getattro*/
  0,                           /*tp_setattro*/
  0,                           /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,          /*tp_flags*/
  /* tp_doc */
  "JavaScript Context.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  PYM_JSContextMethods,        /* tp_methods */
  0,                           /* tp_members */
  0,                           /* tp_getset */
  0,                           /* tp_base */
  0,                           /* tp_dict */
  0,                           /* tp_descr_get */
  0,                           /* tp_descr_set */
  0,                           /* tp_dictoffset */
  0,                           /* tp_init */
  0,                           /* tp_alloc */
  0,                           /* tp_new */
};

extern PYM_JSContextObject *
PYM_newJSContextObject(PYM_JSRuntimeObject *runtime, JSContext *cx)
{
  PYM_JSContextObject *context = PyObject_New(PYM_JSContextObject,
                                              &PYM_JSContextType);
  if (context == NULL)
    return NULL;

  context->runtime = runtime;
  Py_INCREF(runtime);

  context->cx = cx;

  return context;
}
