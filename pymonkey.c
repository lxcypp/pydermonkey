#include "jsapi.h"
#include <Python/Python.h>

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

  // TODO: Support more types.

  Py_INCREF(Py_None);
  return Py_None;
}

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

  JS_EndRequest(cx);
  JS_DestroyContext(cx);
  JS_DestroyRuntime(rt);

  return PYM_jsvalToPyObject(rval);
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
  PyObject *module;

  module = Py_InitModule("pymonkey", PYM_methods);
  if (module == NULL)
    return;

  PYM_error = PyErr_NewException("pymonkey.error", NULL, NULL);
  Py_INCREF(PYM_error);
  PyModule_AddObject(module, "error", PYM_error);
}
