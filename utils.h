#ifndef PYM_UTILS_H
#define PYM_UTILS_H

#include "runtime.h"

#include <jsapi.h>
#include <Python/Python.h>

extern PyObject *PYM_error;

extern PyObject *
PYM_jsvalToPyObject(PYM_JSRuntimeObject *runtime, jsval value);

#endif
