/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 1.1/GPL 2.0/LGPL 2.1
 *
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Original Code is Pydermonkey.
 *
 * The Initial Developer of the Original Code is Mozilla.
 * Portions created by the Initial Developer are Copyright (C) 2007
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *   Atul Varma <atul@mozilla.com>
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 or later (the "GPL"), or
 * the GNU Lesser General Public License Version 2.1 or later (the "LGPL"),
 * in which case the provisions of the GPL or the LGPL are applicable instead
 * of those above. If you wish to allow use of your version of this file only
 * under the terms of either the GPL or the LGPL, and not to allow others to
 * use your version of this file under the terms of the MPL, indicate your
 * decision by deleting the provisions above and replace them with the notice
 * and other provisions required by the GPL or the LGPL. If you do not delete
 * the provisions above, a recipient may use your version of this file under
 * the terms of any one of the MPL, the GPL or the LGPL.
 *
 * ***** END LICENSE BLOCK ***** */

#include "context.h"
#include "object.h"
#include "function.h"
#include "script.h"
#include "utils.h"

#include "jsdbgapi.h"
#include "jsscript.h"

// This is the default throw hook for pydermonkey-owned JS contexts,
// when they've defined one in Python.
static JSTrapStatus
PYM_throwHook(JSContext *cx, JSScript *script, jsbytecode *pc, jsval *rval,
              void *closure)
{
  PYM_PyAutoEnsureGIL gil;
  PYM_JSContextObject *context = (PYM_JSContextObject *)
    JS_GetContextPrivate(cx);

  PyObject *callable = context->throwHook;
  PyObject *args = PyTuple_Pack(1, (PyObject *) context);
  if (args == NULL) {
    JS_ReportOutOfMemory(cx);
    return JSTRAP_CONTINUE;
  }
  PyObject *result = PyObject_Call(callable, args, NULL);
  Py_DECREF(args);
  if (result == NULL) {
    PYM_pythonExceptionToJs(context);
    return JSTRAP_CONTINUE;
  }

  Py_DECREF(result);
  return JSTRAP_CONTINUE;
}

// This is the default JSOperationCallback for pydermonkey-owned JS
// contexts, when they've defined one in Python.
static JSBool
PYM_operationCallback(JSContext *cx)
{
  PYM_PyAutoEnsureGIL gil;
  PYM_JSContextObject *context = (PYM_JSContextObject *)
    JS_GetContextPrivate(cx);

  PyObject *callable = context->opCallback;
  PyObject *args = PyTuple_Pack(1, (PyObject *) context);
  if (args == NULL) {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }
  PyObject *result = PyObject_Call(callable, args, NULL);
  Py_DECREF(args);
  if (result == NULL) {
    PYM_pythonExceptionToJs(context);
    return JS_FALSE;
  }

  Py_DECREF(result);
  return JS_TRUE;
}

// This is the default JSErrorReporter for pydermonkey-owned JS contexts.
static void
PYM_reportError(JSContext *cx, const char *message, JSErrorReport *report)
{
  PYM_PyAutoEnsureGIL gil;

  // Convert JS warnings into Python warnings.
  if (JSREPORT_IS_WARNING(report->flags)) {
    if (report->filename)
      PyErr_WarnExplicit(NULL, message, report->filename, report->lineno,
                         NULL, NULL);
    else
      PyErr_Warn(NULL, message);
  } else
    // TODO: Not sure if this will ever get called, but we should know
    // if it is.
    PyErr_Warn(NULL, "A JS error was reported.");
}

static int
PYM_traverse(PYM_JSContextObject *self, visitproc visit, void *arg)
{
  Py_VISIT(self->opCallback);
  Py_VISIT(self->throwHook);
  Py_VISIT(self->runtime);
  return 0;
}

static int
PYM_clear(PYM_JSContextObject *self)
{
  // Dropping our owned reference to the runtime means that it
  // could disappear, so we're going to have to destroy our own
  // context before we do that.
  if (self->cx) {
    JS_DestroyContext(self->cx);
    self->cx = NULL;
  }

  Py_CLEAR(self->opCallback);
  Py_CLEAR(self->throwHook);
  Py_CLEAR(self->runtime);
  return 0;
}

static void
PYM_JSContextDealloc(PYM_JSContextObject *self)
{
  if (self->weakrefs)
    PyObject_ClearWeakRefs((PyObject *) self);
  PyObject_GC_UnTrack(self);
  PYM_clear(self);
  PyObject_GC_Del(self);
}

static int
PYM_pyObjectToPropertyJsval(PYM_JSContextObject *context,
                            PyObject *object,
                            jsval *rval)
{
  if ((PyInt_Check(object) && !INT_FITS_IN_JSVAL(PyInt_AS_LONG(object))) ||
      PyLong_Check(object)) {
    PyErr_SetString(PyExc_ValueError,
                    "Integer property value out of range.");
    return -1;
  }

  if (PYM_pyObjectToJsval(context, object, rval) == -1)
    return -1;

  if (!(JSVAL_IS_STRING(*rval) || JSVAL_IS_INT(*rval))) {
    PyErr_SetString(PyExc_TypeError,
                    "Property must be a string or integer.");
    return -1;
  }

  return 0;
}

static PyObject *
PYM_getRuntime(PYM_JSContextObject *self, PyObject *args)
{
  Py_INCREF(self->runtime);
  return (PyObject *) self->runtime;
}

static PyObject *
PYM_getStack(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);

  JSStackFrame *iteratorp = NULL;
  JSStackFrame *frame;
  PyObject *top = NULL;
  // This is always a borrowed reference, i.e. we never incref/decref it.
  PyObject *last = NULL;

  while ((frame = JS_FrameIterator(self->cx, &iteratorp)) != NULL) {
    bool success = true;
    JSScript *script = JS_GetFrameScript(self->cx, frame);
    unsigned int pc = 0;
    unsigned int lineno = 0;
    PyObject *pyScript = NULL;
    PyObject *pyFunc = NULL;

    // TODO: Ideally, we'd always convert the script to an object and
    // set it as an attribute of the function, but this can result in
    // strange segfaults, perhaps because JS functions destroy their
    // scripts on finalization while creating an object from a script
    // makes it subject to GC.  So to be safe, we'll only provide the
    // script object if one already exists.
    if (script) {
      jsbytecode *pcByte = JS_GetFramePC(self->cx, frame);
      pc = pcByte - script->code;
      lineno = JS_PCToLineNumber(self->cx, script, pcByte);

      if (JS_GetScriptObject(script)) {
        pyScript = (PyObject *) PYM_newJSScript(self, script);
        if (pyScript == NULL) {
          Py_XDECREF(top);
          return NULL;
        }
      }
    }

    if (pyScript == NULL) {
      pyScript = Py_None;
      Py_INCREF(pyScript);
    }

    JSObject *funObj = JS_GetFrameFunctionObject(self->cx, frame);
    if (funObj) {
      pyFunc = (PyObject *) PYM_newJSObject(self, funObj, NULL);
      if (pyFunc == NULL) {
        Py_XDECREF(top);
        Py_DECREF(pyScript);
        return NULL;
      }
    } else {
      pyFunc = Py_None;
      Py_INCREF(pyFunc);
    }

    PyObject *frameDict = Py_BuildValue(
      "{sOsIsIsOsO}",
      "script", pyScript,
      "pc", pc,
      "lineno", lineno,
      "caller", Py_None,
      "function", pyFunc
      );

    Py_DECREF(pyScript);
    Py_DECREF(pyFunc);

    if (frameDict) {
      if (last) {
        if (PyDict_SetItemString(last, "caller", frameDict) == 0) {
          last = frameDict;
          Py_DECREF(frameDict);
          frameDict = NULL;
        } else
          success = false;
      } else {
        top = frameDict;
        last = frameDict;
      }
    } else
      success = false;

    if (!success) {
      Py_XDECREF(top);
      Py_XDECREF(frameDict);
      return NULL;
    }
  }

  if (top)
    return top;
  Py_RETURN_NONE;
}

static int
PYM_maybeGetFunctionHolder(PYM_JSContextObject *context,
                           PYM_JSObject *object,
                           JSObject **result)
{
  if (PyType_IsSubtype(object->ob_type, &PYM_JSFunctionType)) {
    PYM_JSFunction *func = (PYM_JSFunction *) object;
    if (func->isPython) {
      jsval holder;
      if (!JS_GetReservedSlot(context->cx, object->obj, 0, &holder)) {
        PYM_jsExceptionToPython(context);
        return -1;
      }
      *result = JSVAL_TO_OBJECT(holder);
      return 0;
    }
  }

  return 0;
}

static PyObject *
PYM_isExceptionPending(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);

  if (JS_IsExceptionPending(self->cx))
    Py_RETURN_TRUE;
  Py_RETURN_FALSE;
}

static PyObject *
PYM_getPendingException(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);

  if (!JS_IsExceptionPending(self->cx))
    Py_RETURN_NONE;

  jsval exception;

  if (!JS_GetPendingException(self->cx, &exception)) {
    PyErr_SetString(PYM_error, "JS_GetPendingException() failed");
    return NULL;
  }

  return PYM_jsvalToPyObject(self, exception);
}

static PyObject *
PYM_getObjectPrivate(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  JSObject *obj = object->obj;

  if (PYM_maybeGetFunctionHolder(self, object, &obj) != 0)
    return NULL;

  JSClass *klass = JS_GET_CLASS(self->cx, obj);
  if (klass != &PYM_JS_ObjectClass)
    Py_RETURN_NONE;

  PyObject *pyObject;

  if (!PYM_JS_getPrivatePyObject(self->cx, obj, &pyObject)) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  if (pyObject == NULL)
    pyObject = Py_None;

  Py_INCREF(pyObject);
  return pyObject;
}

static PyObject *
PYM_clearObjectPrivate(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  JSObject *obj = object->obj;

  if (PYM_maybeGetFunctionHolder(self, object, &obj) != 0)
    return NULL;

  JSClass *klass = JS_GET_CLASS(self->cx, obj);
  if (klass != &PYM_JS_ObjectClass)
    Py_RETURN_NONE;

  if (!PYM_JS_setPrivatePyObject(self->cx, obj, Py_None)) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PYM_newArrayObject(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);

  JSObject *array = JS_NewArrayObject(self->cx, 0, NULL);
  if (array == NULL) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  return PYM_jsvalToPyObject(self, OBJECT_TO_JSVAL(array));
}

static PyObject *
PYM_newObject(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PyObject *privateObj = NULL;
  PYM_JSObject *proto = NULL;

  if (!PyArg_ParseTuple(args, "|OO!", &privateObj,
                        &PYM_JSObjectType, &proto))
    return NULL;

  JSObject *jsProto = NULL;

  if (proto) {
    PYM_ENSURE_RUNTIME_MATCH(self->runtime, proto->runtime);
    jsProto = proto->obj;
  }

  JSObject *obj = PYM_JS_newObject(self->cx, privateObj, jsProto, NULL);
  if (obj == NULL) {
    PyErr_SetString(PYM_error, "PYM_JS_newObject() failed");
    return NULL;
  }

  // If this fails, we don't need to worry about cleaning up
  // obj because it'll get cleaned up at the next GC.
  return (PyObject *) PYM_newJSObject(self, obj, NULL);
}

static PyObject *
PYM_hasProperty(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;
  PyObject *property;

  if (!PyArg_ParseTuple(args, "O!O", &PYM_JSObjectType, &object,
                        &property))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  jsval propertyVal;
  if (PYM_pyObjectToPropertyJsval(self, property, &propertyVal) == -1)
    return NULL;

  JSBool hasProperty;
  JSBool result;
  if (JSVAL_IS_INT(propertyVal)) {
    result = JS_HasElement(self->cx, object->obj,
                           JSVAL_TO_INT(propertyVal),
                           &hasProperty);
  } else {
    JSString *str = JSVAL_TO_STRING(propertyVal);
    result = JS_HasUCProperty(self->cx, object->obj,
                              JS_GetStringChars(str),
                              JS_GetStringLength(str),
                              &hasProperty);
  }

  if (!result) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  if (hasProperty)
    Py_RETURN_TRUE;
  Py_RETURN_FALSE;
}

static PyObject *
PYM_getProperty(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;
  PyObject *property;

  if (!PyArg_ParseTuple(args, "O!O", &PYM_JSObjectType, &object,
                        &property))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  jsval propertyVal;
  if (PYM_pyObjectToPropertyJsval(self, property, &propertyVal) == -1)
    return NULL;

  jsval val;
  JSBool result;

  Py_BEGIN_ALLOW_THREADS;
  if (JSVAL_IS_INT(propertyVal)) {
    result = JS_GetElement(self->cx, object->obj,
                           JSVAL_TO_INT(propertyVal), &val);
  } else {
    JSString *str = JSVAL_TO_STRING(propertyVal);
    result = JS_GetUCProperty(self->cx, object->obj,
                              JS_GetStringChars(str),
                              JS_GetStringLength(str),
                              &val);
  }
  Py_END_ALLOW_THREADS;

  if (!result) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  return PYM_jsvalToPyObject(self, val);
}

static PyObject *
PYM_gc(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  JS_GC(self->cx);
  Py_RETURN_NONE;
}

static PyObject *
PYM_initStandardClasses(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  if (!JS_InitStandardClasses(self->cx, object->obj)) {
    PyErr_SetString(PYM_error, "JS_InitStandardClasses() failed");
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PYM_compileScript(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  char *source = NULL;
  int sourceLen;
  const char *filename;
  int lineNo;

  if (!PyArg_ParseTuple(args, "es#si", "utf-16", &source, &sourceLen,
                        &filename, &lineNo))
    return NULL;

  PYM_UTF16String str(source, sourceLen);

  JSScript *script;
  script = JS_CompileUCScript(self->cx, NULL, str.jsbuffer,
                              str.jslen, filename, lineNo);

  if (script == NULL) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  // TODO: If this somehow fails, we may have a memory leak if a
  // script object wasn't created for the JSScript.
  return (PyObject *) PYM_newJSScript(self, script);
}

static PyObject *
PYM_executeScript(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;
  PYM_JSScript *script;

  if (!PyArg_ParseTuple(args, "O!O!", &PYM_JSObjectType, &object,
                        &PYM_JSScriptType, &script))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);
  PYM_ENSURE_RUNTIME_MATCH(self->runtime, script->base.runtime);

  jsval rval;
  JSBool result;
  Py_BEGIN_ALLOW_THREADS;
  result = JS_ExecuteScript(self->cx, object->obj, script->script, &rval);
  Py_END_ALLOW_THREADS;

  if (!result) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  PyObject *pyRval = PYM_jsvalToPyObject(self, rval);
  return pyRval;
}

static PyObject *
PYM_evaluateScript(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;
  char *source = NULL;
  int sourceLen;
  const char *filename;
  int lineNo;

  if (!PyArg_ParseTuple(args, "O!es#si", &PYM_JSObjectType, &object,
                        "utf-16", &source, &sourceLen, &filename, &lineNo))
    return NULL;

  PYM_UTF16String str(source, sourceLen);

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  // Instead of calling JS_EvaluateUCScript(), we're going to first
  // compile the script and then execute it. This is because the
  // former function calls JS_DestroyScript() on the script it's
  // created, which prevents e.g. the script object from being
  // extracted during execution and outliving the script's execution.

  JSScript *script;
  script = JS_CompileUCScript(self->cx, NULL, str.jsbuffer,
                              str.jslen, filename, lineNo);

  if (script == NULL) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  // TODO: If this somehow fails, we may have a memory leak if a
  // script object wasn't created for the JSScript.
  PYM_JSScript *pyScript = PYM_newJSScript(self, script);

  if (pyScript == NULL) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  jsval rval;
  JSBool result;
  Py_BEGIN_ALLOW_THREADS;
  result = JS_ExecuteScript(self->cx, object->obj, pyScript->script, &rval);
  Py_END_ALLOW_THREADS;

  Py_DECREF((PyObject *) pyScript);
  if (!result) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  PyObject *pyRval = PYM_jsvalToPyObject(self, rval);
  return pyRval;
}

static PyObject *
PYM_isArrayObject(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  if (JS_IsArrayObject(self->cx, object->obj))
    Py_RETURN_TRUE;
  Py_RETURN_FALSE;
}

static PyObject *
PYM_enumerate(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  JSIdArray *idArray = JS_Enumerate(self->cx, object->obj);
  if (idArray == NULL) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  PyObject *tuple = PyTuple_New(idArray->length);
  if (tuple == NULL) {
    JS_DestroyIdArray(self->cx, idArray);
    return NULL;
  }

  for (int i = 0; i < idArray->length; i++) {
    jsval val;
    if (!JS_IdToValue(self->cx, idArray->vector[i], &val)) {
      Py_DECREF(tuple);
      JS_DestroyIdArray(self->cx, idArray);
      PYM_jsExceptionToPython(self);
      return NULL;
    }

    PyObject *item = PYM_jsvalToPyObject(self, val);
    if (item == NULL) {
      Py_DECREF(tuple);
      JS_DestroyIdArray(self->cx, idArray);
      return NULL;
    }

    // Note that this function "steals" a reference to item, so
    // we don't need to decrement its reference count.
    PyTuple_SET_ITEM(tuple, i, item);
  }

  JS_DestroyIdArray(self->cx, idArray);
  return tuple;
}

static PyObject *
PYM_defineProperty(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;
  PyObject *property;
  PyObject *value;

  if (!PyArg_ParseTuple(args, "O!OO", &PYM_JSObjectType, &object,
                        &property, &value))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  JSAutoLocalRootScope localRootScope(self->cx);
  jsval propertyVal;
  if (PYM_pyObjectToPropertyJsval(self, property, &propertyVal) == -1)
    return NULL;

  jsval jsValue;
  if (PYM_pyObjectToJsval(self, value, &jsValue) == -1)
    return NULL;

  JSBool result;
  if (JSVAL_IS_INT(propertyVal)) {
    result = JS_DefineElement(self->cx, object->obj,
                              JSVAL_TO_INT(propertyVal),
                              jsValue, NULL, NULL, JSPROP_ENUMERATE);
  } else {
    JSString *str = JSVAL_TO_STRING(propertyVal);
    result = JS_DefineUCProperty(self->cx, object->obj,
                                 JS_GetStringChars(str),
                                 JS_GetStringLength(str),
                                 jsValue, NULL, NULL,
                                 JSPROP_ENUMERATE);
  }

  if (!result) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  Py_RETURN_NONE;
}

static PyObject *
PYM_callFunction(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *obj;
  PYM_JSFunction *fun;
  PyObject *funcArgs;

  if (!PyArg_ParseTuple(args, "O!O!O!", &PYM_JSObjectType, &obj,
                        &PYM_JSFunctionType, &fun,
                        &PyTuple_Type, &funcArgs))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, obj->runtime);
  PYM_ENSURE_RUNTIME_MATCH(self->runtime, fun->base.runtime);

  uintN argc = PyTuple_Size(funcArgs);

  jsval *argv = (jsval *) PyMem_Malloc(sizeof(jsval) * argc);
  if (argv == NULL)
    return PyErr_NoMemory();

  jsval *currArg = argv;

  for (unsigned int i = 0; i < argc; i++) {
    PyObject *item = PyTuple_GET_ITEM(funcArgs, i);
    if (item == NULL ||
        PYM_pyObjectToJsval(self, item, currArg) == -1) {
      PyMem_Free(argv);
      return NULL;
    }
    currArg++;
  }

  jsval rval;
  JSBool result;
  Py_BEGIN_ALLOW_THREADS;
  result = JS_CallFunctionValue(self->cx, obj->obj,
                                OBJECT_TO_JSVAL(fun->base.obj),
                                argc, argv, &rval);
  Py_END_ALLOW_THREADS;

  PyMem_Free(argv);

  if (!result) {
    PYM_jsExceptionToPython(self);
    return NULL;
  }

  return PYM_jsvalToPyObject(self, rval);
}

static PyObject *
PYM_newFunction(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PyObject *callable;
  const char *name;

  if (!PyArg_ParseTuple(args, "Os", &callable, &name))
    return NULL;

  return (PyObject *) PYM_newJSFunctionFromCallable(self, callable, name);
}

static PyObject *
PYM_setThrowHook(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PyObject *callable;

  if (!PyArg_ParseTuple(args, "O", &callable))
    return NULL;

  if (!PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "Callable must be callable");
    return NULL;
  }

  self->hooks.throwHook = PYM_throwHook;
  JS_SetContextDebugHooks(self->cx, &self->hooks);

  Py_INCREF(callable);
  if (self->throwHook)
    Py_DECREF(self->throwHook);
  self->throwHook = callable;

  Py_RETURN_NONE;
}

static PyObject *
PYM_setOperationCallback(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PyObject *callable;

  if (!PyArg_ParseTuple(args, "O", &callable))
    return NULL;

  if (!PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "Callable must be callable");
    return NULL;
  }

  JS_SetOperationCallback(self->cx, PYM_operationCallback);

  Py_INCREF(callable);
  if (self->opCallback)
    Py_DECREF(self->opCallback);
  self->opCallback = callable;

  Py_RETURN_NONE;
}

static PyObject *
PYM_triggerOperationCallback(PYM_JSContextObject *self, PyObject *args)
{
  JS_TriggerOperationCallback(self->cx);
  Py_RETURN_NONE;
}

static PyMethodDef PYM_JSContextMethods[] = {
  {"get_runtime", (PyCFunction) PYM_getRuntime, METH_VARARGS,
   "Get the JavaScript runtime associated with this context."},
  {"get_stack", (PyCFunction) PYM_getStack, METH_VARARGS,
   "Get the current stack for the context."},
  {"new_array_object", (PyCFunction) PYM_newArrayObject, METH_VARARGS,
   "Create a new JavaScript Array object."},
  {"new_object", (PyCFunction) PYM_newObject, METH_VARARGS,
   "Create a new JavaScript object."},
  {"init_standard_classes",
   (PyCFunction) PYM_initStandardClasses, METH_VARARGS,
   "Add standard classes and functions to the given object."},
  {"compile_script",
   (PyCFunction) PYM_compileScript, METH_VARARGS,
   "Compile the given JavaScript code using the given filename"
   "and line number information."},
  {"execute_script",
   (PyCFunction) PYM_executeScript, METH_VARARGS,
   "Execute the given JavaScript script object in the context of "
   "the given global object."},
  {"evaluate_script",
   (PyCFunction) PYM_evaluateScript, METH_VARARGS,
   "Evaluate the given JavaScript code in the context of the given "
   "global object, using the given filename"
   "and line number information."},
  {"call_function",
   (PyCFunction) PYM_callFunction, METH_VARARGS,
   "Calls a JS function."},
  {"new_function",
   (PyCFunction) PYM_newFunction, METH_VARARGS,
   "Creates a new function callable from JS."},
  {"is_array_object", (PyCFunction) PYM_isArrayObject, METH_VARARGS,
   "Returns whether or not the given JavaScript object is an array."},
  {"enumerate", (PyCFunction) PYM_enumerate, METH_VARARGS,
   "Returns a tuple of all a JavaScript object's enumerable properties."},
  {"define_property",
   (PyCFunction) PYM_defineProperty, METH_VARARGS,
   "Defines a property on a JavaScript object."},
  {"get_property", (PyCFunction) PYM_getProperty, METH_VARARGS,
   "Gets the given property for the given JavaScript object."},
  {"has_property", (PyCFunction) PYM_hasProperty, METH_VARARGS,
   "Returns whether the given JavaScript object has the given property."},
  {"gc", (PyCFunction) PYM_gc, METH_VARARGS,
   "Performs garbage collection on the context's runtime."},
  {"set_operation_callback", (PyCFunction) PYM_setOperationCallback,
   METH_VARARGS,
   "Sets the operation callback for the context."},
  {"set_throw_hook", (PyCFunction) PYM_setThrowHook, METH_VARARGS,
   "Sets the throw hook for the context."},
  {"trigger_operation_callback", (PyCFunction) PYM_triggerOperationCallback,
   METH_VARARGS,
   "Triggers the operation callback for the context."},
  {"get_object_private", (PyCFunction) PYM_getObjectPrivate, METH_VARARGS,
   "Returns the private Python object stored in the JavaScript object."},
  {"clear_object_private", (PyCFunction) PYM_clearObjectPrivate, METH_VARARGS,
   "Clears any private Python object stored in the JavaScript object."},
  {"is_exception_pending", (PyCFunction) PYM_isExceptionPending,
   METH_VARARGS,
   "Returns whether an exception is currently being propagated."},
  {"get_pending_exception", (PyCFunction) PYM_getPendingException,
   METH_VARARGS,
   "Returns the current exception being propagated."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSContextType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pydermonkey.Context",       /*tp_name*/
  sizeof(PYM_JSContextObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
                               /*tp_dealloc*/
  (destructor) PYM_JSContextDealloc,
  0,                           /*tp_print*/
  0,                           /*tp_getattr*/
  0,                           /*tp_setattr*/
  0,                           /*tp_compare*/
  0,                           /*tp_repr*/
  0,                           /*tp_as_number*/
  0,                           /*tp_as_sequence*/
  0,                           /*tp_as_mapping*/
  0,                           /*tp_hash */
  0,                           /*tp_call*/
  0,                           /*tp_str*/
  0,                           /*tp_getattro*/
  0,                           /*tp_setattro*/
  0,                           /*tp_as_buffer*/
                               /*tp_flags*/
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_HAVE_GC | Py_TPFLAGS_HAVE_WEAKREFS,
                               /* tp_doc */
  "JavaScript Context.",
  (traverseproc) PYM_traverse, /* tp_traverse */
  (inquiry) PYM_clear,         /* tp_clear */
  0,                           /* tp_richcompare */
                               /* tp_weaklistoffset */
  offsetof(PYM_JSContextObject, weakrefs),
  0,                           /* tp_iter */
  0,                           /* tp_iternext */
  PYM_JSContextMethods,        /* tp_methods */
  0,                           /* tp_members */
  0,                           /* tp_getset */
  0,                           /* tp_base */
  0,                           /* tp_dict */
  0,                           /* tp_descr_get */
  0,                           /* tp_descr_set */
  0,                           /* tp_dictoffset */
  0,                           /* tp_init */
  0,                           /* tp_alloc */
  0,                           /* tp_new */
};

extern PYM_JSContextObject *
PYM_newJSContextObject(PYM_JSRuntimeObject *runtime, JSContext *cx)
{
  PYM_JSContextObject *context = PyObject_GC_New(PYM_JSContextObject,
                                                 &PYM_JSContextType);
  if (context == NULL)
    return NULL;

  memset(&context->hooks, 0, sizeof(context->hooks));

  context->weakrefs = NULL;
  context->opCallback = NULL;
  context->throwHook = NULL;
  context->runtime = runtime;
  Py_INCREF(runtime);

  context->cx = cx;
  JS_SetContextPrivate(cx, context);
  JS_SetErrorReporter(cx, PYM_reportError);

#ifdef JS_GC_ZEAL
  // TODO: Consider exposing JS_SetGCZeal() to Python instead of
  // hard-coding it here.
  JS_SetGCZeal(cx, 2);
#endif

  PyObject_GC_Track((PyObject *) context);
  return context;
}
