#ifndef PYM_FUNCTION_H
#define PYM_FUNCTION_H

#include "object.h"
#include "context.h"

#include <jsapi.h>
#include <Python/Python.h>

typedef struct {
  PYM_JSObject base;
  PyObject *callable;
} PYM_JSFunction;

extern PyTypeObject PYM_JSFunctionType;

extern PYM_JSFunction *
PYM_newJSFunctionFromCallable(PYM_JSContextObject *context,
                              PyObject *callable,
                              const char *name);

#endif
