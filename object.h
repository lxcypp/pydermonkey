#ifndef PYM_OBJECT_H
#define PYM_OBJECT_H

#include "context.h"

#include <jsapi.h>
#include <Python/Python.h>

extern JSClass PYM_JS_ObjectClass;

typedef struct {
  PyObject_HEAD
  PYM_JSRuntimeObject *runtime;
  JSObject *obj;
  PyObject *uniqueId;
  PyObject *weakrefList;
} PYM_JSObject;

extern PyTypeObject PYM_JSObjectType;

extern PYM_JSObject *
PYM_newJSObject(PYM_JSContextObject *context, JSObject *obj);

#endif
