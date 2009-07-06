#ifndef PYM_UTILS_H
#define PYM_UTILS_H

#include "context.h"

#include <jsapi.h>
#include <jsdhash.h>
#include <Python/Python.h>

typedef struct {
  JSDHashEntryStub base;
  void *value;
} PYM_HashEntry;

extern PyObject *PYM_error;

extern int
PYM_pyObjectToJsval(PYM_JSContextObject *context,
                    PyObject *object,
                    jsval *rval);

extern PyObject *
PYM_jsvalToPyObject(PYM_JSContextObject *context, jsval value);

extern void
PYM_pythonExceptionToJs(PYM_JSContextObject *context);

void
PYM_jsExceptionToPython(PYM_JSContextObject *context);

#endif
