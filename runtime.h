#ifndef PYM_RUNTIME_H
#define PYM_RUNTIME_H

#include <jsapi.h>
#include <Python/Python.h>

typedef struct {
  PyObject_HEAD
  JSRuntime *rt;
} PYM_JSRuntimeObject;

extern PyTypeObject PYM_JSRuntimeType;

#endif
