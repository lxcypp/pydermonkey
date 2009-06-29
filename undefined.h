#include <Python/Python.h>

#define Py_RETURN_UNDEFINED  { Py_INCREF(PYM_undefined);        \
                               return PYM_undefined; }

typedef struct {
  PyObject_HEAD
} PYM_undefinedObject;

extern PyTypeObject PYM_undefinedType;

extern PyObject *PYM_undefined;
