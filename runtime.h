#ifndef PYM_RUNTIME_H
#define PYM_RUNTIME_H

#include <jsapi.h>
#include <jsdhash.h>
#include <Python/Python.h>

typedef struct {
  PyObject_HEAD
  JSRuntime *rt;
  JSDHashTable objects;
} PYM_JSRuntimeObject;

extern PyTypeObject PYM_JSRuntimeType;

#endif
