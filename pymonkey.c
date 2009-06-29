#include "undefined.h"
#include "runtime.h"
#include "context.h"
#include "object.h"
#include "utils.h"

#include <jsapi.h>

static JSClass PYM_jsGlobalClass = {
  "PymonkeyGlobal", JSCLASS_GLOBAL_FLAGS,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
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

  if (!PyType_Ready(&PYM_JSObjectType) < 0)
    return;

  Py_INCREF(&PYM_JSObjectType);
  PyModule_AddObject(module, "Object", (PyObject *) &PYM_JSObjectType);
}
