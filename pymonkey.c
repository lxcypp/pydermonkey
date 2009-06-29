#include "undefined.h"
#include "runtime.h"
#include "context.h"
#include "object.h"
#include "utils.h"

static PyMethodDef PYM_methods[] = {
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initpymonkey(void)
{
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
