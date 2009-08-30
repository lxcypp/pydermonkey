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
 * The Original Code is Pymonkey.
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

// This is the default JSOperationCallback for pymonkey-owned JS contexts,
// when they've defined one in Python.
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

// This is the default JSErrorReporter for pymonkey-owned JS contexts.
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
  Py_VISIT(self->runtime);
  return 0;
}

static int
PYM_clear(PYM_JSContextObject *self)
{
  Py_CLEAR(self->opCallback);
  Py_CLEAR(self->runtime);
  return 0;
}

static void
PYM_JSContextDealloc(PYM_JSContextObject *self)
{
  if (self->weakrefs)
    PyObject_ClearWeakRefs((PyObject *) self);
  PyObject_GC_UnTrack(self);

  if (self->cx) {
    JS_DestroyContext(self->cx);
    self->cx = NULL;
  }

  PYM_clear(self);
  PyObject_GC_Del(self);
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
          // TODO: We should clean up here.
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
        // TODO: We should clean up here.
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

static PyObject *
PYM_getObjectPrivate(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;

  if (!PyArg_ParseTuple(args, "O!", &PYM_JSObjectType, &object))
    return NULL;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  JSObject *obj = object->obj;

  if (JS_ObjectIsFunction(self->cx, obj)) {
    jsval functionHolder;
    if (!JS_GetReservedSlot(self->cx, obj, 0, &functionHolder)) {
      JS_ClearPendingException(self->cx);
      Py_RETURN_NONE;
    }
    if (!JSVAL_IS_OBJECT(functionHolder))
      Py_RETURN_NONE;
    obj = JSVAL_TO_OBJECT(functionHolder);
  }

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

  if (JS_ObjectIsFunction(self->cx, obj)) {
    jsval functionHolder;
    if (!JS_GetReservedSlot(self->cx, obj, 0, &functionHolder)) {
      JS_ClearPendingException(self->cx);
      Py_RETURN_NONE;
    }
    if (!JSVAL_IS_OBJECT(functionHolder))
      Py_RETURN_NONE;
    obj = JSVAL_TO_OBJECT(functionHolder);
  }

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
PYM_newObject(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PyObject *privateObj = NULL;

  if (!PyArg_ParseTuple(args, "|O", &privateObj))
    return NULL;

  JSObject *obj = PYM_JS_newObject(self->cx, privateObj);
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
  char *buffer = NULL;
  int size;

  if (!PyArg_ParseTuple(args, "O!es#", &PYM_JSObjectType, &object,
                        "utf-16", &buffer, &size))
    return NULL;

  PYM_UTF16String str(buffer, size);

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  JSBool hasProperty;
  JSBool result;
  result = JS_HasUCProperty(self->cx, object->obj, str.jsbuffer,
                            str.jslen, &hasProperty);

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
  char *buffer = NULL;
  int size;

  if (!PyArg_ParseTuple(args, "O!es#", &PYM_JSObjectType, &object,
                        "utf-16", &buffer, &size))
    return NULL;

  PYM_UTF16String str(buffer, size);

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  jsval val;
  JSBool result;
  Py_BEGIN_ALLOW_THREADS;
  result = JS_GetUCProperty(self->cx, object->obj, str.jsbuffer,
                            str.jslen, &val);
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
PYM_defineProperty(PYM_JSContextObject *self, PyObject *args)
{
  PYM_SANITY_CHECK(self->runtime);
  PYM_JSObject *object;
  PyObject *value;
  char *name = NULL;
  int namelen;

  if (!PyArg_ParseTuple(args, "O!es#O", &PYM_JSObjectType, &object,
                        "utf-16", &name, &namelen, &value))
    return NULL;

  PYM_UTF16String str(name, namelen);
  jsval jsValue;

  PYM_ENSURE_RUNTIME_MATCH(self->runtime, object->runtime);

  if (PYM_pyObjectToJsval(self, value, &jsValue) == -1)
    return NULL;

  if (!JS_DefineUCProperty(self->cx, object->obj, str.jsbuffer,
                           str.jslen, jsValue, NULL, NULL,
                           JSPROP_ENUMERATE)) {
    // TODO: There's probably an exception pending on self->cx,
    // what should we do about it?
    PyErr_SetString(PYM_error, "JS_DefineProperty() failed");
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
  {"define_property",
   (PyCFunction) PYM_defineProperty, METH_VARARGS,
   "Defines a property on an object."},
  {"get_property", (PyCFunction) PYM_getProperty, METH_VARARGS,
   "Gets the given property for the given JavaScript object."},
  {"has_property", (PyCFunction) PYM_hasProperty, METH_VARARGS,
   "Returns whether the given JavaScript object has the given property."},
  {"gc", (PyCFunction) PYM_gc, METH_VARARGS,
   "Performs garbage collection on the context's runtime."},
  {"set_operation_callback", (PyCFunction) PYM_setOperationCallback,
   METH_VARARGS,
   "Sets the operation callback for the context."},
  {"trigger_operation_callback", (PyCFunction) PYM_triggerOperationCallback,
   METH_VARARGS,
   "Triggers the operation callback for the context."},
  {"get_object_private", (PyCFunction) PYM_getObjectPrivate, METH_VARARGS,
   "Returns the private Python object stored in the JavaScript object."},
  {"clear_object_private", (PyCFunction) PYM_clearObjectPrivate, METH_VARARGS,
   "Clears any private Python object stored in the JavaScript object."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSContextType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Context",          /*tp_name*/
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

  context->weakrefs = NULL;
  context->opCallback = NULL;
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
