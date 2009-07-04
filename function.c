#include "function.h"
#include "utils.h"

static void
PYM_JSFunctionDealloc(PYM_JSFunction *self)
{
  // TODO: What if there's still a reference to the callable in
  // JS-land?
  if (self->callable) {
    Py_DECREF(self->callable);
    self->callable = NULL;
  }
  PYM_JSObjectType.tp_dealloc((PyObject *) self);
}

static JSBool
dispatchJSFunctionToPython(JSContext *cx,
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

  int error = PYM_pyObjectToJsval(cx, result, rval);
  Py_DECREF(result);

  if (error) {
    // TODO: Get the actual exception.
    JS_ReportError(cx, "Python function failed.");
    return JS_FALSE;
  }
  return JS_TRUE;
}

PyTypeObject PYM_JSFunctionType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Function",         /*tp_name*/
  sizeof(PYM_JSFunction),      /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSFunctionDealloc,
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
  /*tp_flags*/
  Py_TPFLAGS_DEFAULT,
  /* tp_doc */
  "JavaScript Function.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,                           /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  0,                           /* tp_methods */
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

PYM_JSFunction *
PYM_newJSFunctionFromCallable(PYM_JSContextObject *context,
                              PyObject *callable,
                              const char *name)
{
  if (!PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "Callable must be callable");
    return NULL;
  }

  JSFunction *func = JS_NewFunction(context->cx,
                                    dispatchJSFunctionToPython, 0,
                                    0, NULL, name);

  if (func == NULL) {
    PyErr_SetString(PYM_error, "JS_DefineFunction() failed");
    return NULL;
  }

  JSObject *funcObj = JS_GetFunctionObject(func);

  if (funcObj == NULL) {
    PyErr_SetString(PYM_error, "JS_GetFunctionObject() failed");
    return NULL;
  }

  if (!JS_SetReservedSlot(context->cx, funcObj, 0,
                          PRIVATE_TO_JSVAL(callable))) {
    PyErr_SetString(PYM_error, "JS_SetReservedSlot() failed");
    return NULL;
  }

  PYM_JSFunction *object = PyObject_New(PYM_JSFunction,
                                        &PYM_JSFunctionType);
  if (object == NULL)
    return NULL;

  object->callable = NULL;
  if (PYM_newJSObject(context, funcObj,
                      (PYM_JSObject *) object) == NULL)
    // Note that our object's reference count will have already
    // been decremented by PYM_newJSObject().
    return NULL;

  object->callable = callable;
  Py_INCREF(callable);

  return object;
}
