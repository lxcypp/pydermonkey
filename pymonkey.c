#include "jsapi.h"
#include <Python/Python.h>

#define Py_RETURN_UNDEFINED  { Py_INCREF(PYM_undefined);        \
                               return PYM_undefined; }

typedef struct {
  PyObject_HEAD
} PYM_undefinedObject;

// TODO: We should make this behave as much like JavaScript's
// "undefined" value as possible; e.g., its string value should
// be "undefined", the singleton should be falsy, etc.
static PyTypeObject PYM_undefinedType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.undefined",        /*tp_name*/
  sizeof(PYM_undefinedObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  0,                           /*tp_dealloc*/
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
  "Pythonic equivalent of JavaScript's 'undefined' value",
};

static PyObject *PYM_undefined = (PyObject *) &PYM_undefinedType;

static JSClass PYM_jsGlobalClass = {
  "PymonkeyGlobal", JSCLASS_GLOBAL_FLAGS,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static PyObject *PYM_error;

static PyObject *
PYM_jsvalToPyObject(jsval value) {
  if (JSVAL_IS_INT(value))
    return PyInt_FromLong(JSVAL_TO_INT(value));

  if (JSVAL_IS_DOUBLE(value)) {
    jsdouble *doubleRef = JSVAL_TO_DOUBLE(value);
    return PyFloat_FromDouble(*doubleRef);
  }

  if (value == JSVAL_FALSE)
    Py_RETURN_FALSE;

  if (value == JSVAL_TRUE)
    Py_RETURN_TRUE;

  if (JSVAL_IS_NULL(value))
    Py_RETURN_NONE;

  if (JSVAL_IS_VOID(value))
    Py_RETURN_UNDEFINED;

  if (JSVAL_IS_STRING(value) && JS_CStringsAreUTF8()) {
    // TODO: What to do if C strings aren't UTF-8?  The jschar *
    // type isn't actually UTF-16, it's just "UTF-16-ish", so
    // there doesn't seem to be any other lossless way of
    // transferring the string other than perhaps by transmitting
    // its JSON representation.

    JSString *str = JSVAL_TO_STRING(value);
    const char *bytes = JS_GetStringBytes(str);
    const char *errors;
    return PyUnicode_DecodeUTF8(bytes, strlen(bytes), errors);
  }

  // TODO: Support more types.
  PyErr_SetString(PyExc_NotImplementedError,
                  "Data type conversion not implemented.");
}

typedef struct {
  PyObject_HEAD
  JSRuntime *rt;
} PYM_JSRuntimeObject;

static PyObject *
PYM_JSRuntimeNew(PyTypeObject *type, PyObject *args,
                 PyObject *kwds)
{
  PYM_JSRuntimeObject *self;

  self = (PYM_JSRuntimeObject *) type->tp_alloc(type, 0);
  if (self != NULL) {
    self->rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!self->rt) {
      PyErr_SetString(PYM_error, "JS_NewRuntime() failed");
      type->tp_dealloc((PyObject *) self);
      self = NULL;
    }
  }

  return (PyObject *) self;
}

static void
PYM_JSRuntimeDealloc(PYM_JSRuntimeObject *self)
{
  if (self->rt) {
    JS_DestroyRuntime(self->rt);
    self->rt = NULL;
  }

  self->ob_type->tp_free((PyObject *) self);
}

extern PyTypeObject PYM_JSContextType;

typedef struct {
  PyObject_HEAD
  PYM_JSRuntimeObject *runtime;
  JSContext *cx;
} PYM_JSContextObject;

static PyObject *
PYM_newContext(PYM_JSRuntimeObject *self, PyObject *args)
{
  PYM_JSContextObject *context = PyObject_New(PYM_JSContextObject,
                                              &PYM_JSContextType);

  context->runtime = self;
  Py_INCREF(self);

  context->cx = JS_NewContext(self->rt, 8192);
  if (context->cx == NULL) {
    PyErr_SetString(PYM_error, "JS_NewContext() failed");
    Py_DECREF(context);
    return NULL;
  }

  JS_SetOptions(context->cx, JSOPTION_VAROBJFIX);
  JS_SetVersion(context->cx, JSVERSION_LATEST);

  return (PyObject *) context;
}

static PyMethodDef PYM_JSRuntimeMethods[] = {
  {"new_context", (PyCFunction) PYM_newContext, METH_VARARGS,
   "Create a new JavaScript context."},
  {NULL, NULL, 0, NULL}
};

static PyTypeObject PYM_JSRuntimeType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Runtime",          /*tp_name*/
  sizeof(PYM_JSRuntimeObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSRuntimeDealloc,
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
  "JavaScript Runtime.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  PYM_JSRuntimeMethods,        /* tp_methods */
  0,                           /* tp_members */
  0,                           /* tp_getset */
  0,                           /* tp_base */
  0,                           /* tp_dict */
  0,                           /* tp_descr_get */
  0,                           /* tp_descr_set */
  0,                           /* tp_dictoffset */
  0,                           /* tp_init */
  0,                           /* tp_alloc */
  PYM_JSRuntimeNew,            /* tp_new */
};

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

static PyMethodDef PYM_JSContextMethods[] = {
  {"get_runtime", (PyCFunction) PYM_getRuntime, METH_VARARGS,
   "Get the JavaScript runtime associated with this context."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSContextType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Context",          /*tp_name*/
  sizeof(PYM_JSRuntimeObject), /*tp_basicsize*/
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

static PyObject *
PYM_evaluate(PyObject *self, PyObject *args)
{
  const char *source;
  int sourceLen;
  const char *filename;
  int lineNo;

  if (!PyArg_ParseTuple(args, "s#si", &source, &sourceLen,
                        &filename, &lineNo))
    return NULL;

  JSRuntime *rt = JS_NewRuntime(8L * 1024L * 1024L);
  if (rt == NULL) {
    PyErr_SetString(PYM_error, "JS_NewRuntime() failed");
    return NULL;
  }

  JSContext *cx = JS_NewContext(rt, 8192);
  if (cx == NULL) {
    PyErr_SetString(PYM_error, "JS_NewContext() failed");
    JS_DestroyRuntime(rt);
  }

  JS_SetOptions(cx, JSOPTION_VAROBJFIX);
  JS_SetVersion(cx, JSVERSION_LATEST);

  JS_BeginRequest(cx);

  JSObject *global = JS_NewObject(cx, &PYM_jsGlobalClass, NULL, NULL);
  if (global == NULL) {
    PyErr_SetString(PYM_error, "JS_NewObject() failed");
    JS_EndRequest(cx);
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    return NULL;
  }

  if (!JS_InitStandardClasses(cx, global)) {
    PyErr_SetString(PYM_error, "JS_InitStandardClasses() failed");
    JS_EndRequest(cx);
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    return NULL;
  }

  jsval rval;
  if (!JS_EvaluateScript(cx, global, source, sourceLen, filename,
                         lineNo, &rval)) {
    // TODO: Actually get the error that was raised.
    PyErr_SetString(PYM_error, "Script failed");
    JS_EndRequest(cx);
    JS_DestroyContext(cx);
    JS_DestroyRuntime(rt);
    return NULL;
  }

  PyObject *pyRval = PYM_jsvalToPyObject(rval);

  JS_EndRequest(cx);
  JS_DestroyContext(cx);
  JS_DestroyRuntime(rt);

  return pyRval;
}

static PyMethodDef PYM_methods[] = {
  {"evaluate", PYM_evaluate, METH_VARARGS,
   "Evaluate the given JavaScript code, using the given filename "
   "and line number information."},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initpymonkey(void)
{
  if (!JS_CStringsAreUTF8())
    JS_SetCStringsAreUTF8();

  PyObject *module;

  module = Py_InitModule("pymonkey", PYM_methods);
  if (module == NULL)
    return;

  if (PyType_Ready(&PYM_undefinedType) < 0)
    return;

  Py_INCREF(PYM_undefined);
  PyModule_AddObject(module, "undefined", PYM_undefined);

  PYM_error = PyErr_NewException("pymonkey.error", NULL, NULL);
  Py_INCREF(PYM_error);
  PyModule_AddObject(module, "error", PYM_error);

  if (!PyType_Ready(&PYM_JSRuntimeType) < 0)
    return;

  Py_INCREF(&PYM_JSRuntimeType);
  PyModule_AddObject(module, "Runtime", (PyObject *) &PYM_JSRuntimeType);

  if (!PyType_Ready(&PYM_JSContextType) < 0)
    return;

  Py_INCREF(&PYM_JSContextType);
  PyModule_AddObject(module, "Context", (PyObject *) &PYM_JSContextType);
}
