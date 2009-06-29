#ifndef PYM_CONTEXT_H
#define PYM_CONTEXT_H

#include "runtime.h"

#include <jsapi.h>
#include <Python/Python.h>

typedef struct {
  PyObject_HEAD
  PYM_JSRuntimeObject *runtime;
  JSContext *cx;
} PYM_JSContextObject;

extern PyTypeObject PYM_JSContextType;

#endif
