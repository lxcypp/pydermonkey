#ifndef PYM_OBJECT_H
#define PYM_OBJECT_H

#include "runtime.h"

#include <jsapi.h>
#include <Python/Python.h>

extern JSClass PYM_JS_ObjectClass;

typedef struct {
  PyObject_HEAD
  PYM_JSRuntimeObject *runtime;
  JSObject *obj;
} PYM_JSObject;

extern PyTypeObject PYM_JSObjectType;

#endif
