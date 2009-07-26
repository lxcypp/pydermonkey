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

#include "function.h"
#include "utils.h"

static JSBool
PYM_getHeldFunction(JSContext *cx, JSObject *obj, PyObject **callable)
{
  jsval jsCallable;
  if (!JS_GetReservedSlot(cx, obj, 0, &jsCallable)) {
    JS_ReportError(cx, "JS_GetReservedSlot() failed.");
    return JS_FALSE;
  }
  *callable = (PyObject *) JSVAL_TO_PRIVATE(jsCallable);
  return JS_TRUE;
}

static void
PYM_finalizeFunctionHolder(JSContext *cx, JSObject *obj)
{
  PyObject *callable;
  if (PYM_getHeldFunction(cx, obj, &callable))
    Py_DECREF(callable);
}

static JSClass PYM_JS_FunctionHolderClass = {
  "PymonkeyFunctionHolder", JSCLASS_HAS_RESERVED_SLOTS(1),
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub,
  PYM_finalizeFunctionHolder,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static JSObject *
PYM_newFunctionHolder(JSContext *cx, PyObject *callable)
{
  JSObject *obj = JS_NewObject(cx, &PYM_JS_FunctionHolderClass, NULL, NULL);
  if (obj) {
    if (JS_SetReservedSlot(cx, obj, 0, PRIVATE_TO_JSVAL(callable)))
      Py_INCREF(callable);
    else {
      obj = NULL;
      PyErr_SetString(PYM_error, "JS_SetReservedSlot() failed");
    }
  } else {
    PyErr_SetString(PYM_error, "JS_NewObject() failed");
  }
  return obj;
}

static void
PYM_JSFunctionDealloc(PYM_JSFunction *self)
{
  PYM_JSObjectType.tp_dealloc((PyObject *) self);
}

static JSBool
PYM_dispatchJSFunctionToPython(JSContext *cx,
                               JSObject *obj,
                               uintN argc,
                               jsval *argv,
                               jsval *rval)
{
  jsval callee = JS_ARGV_CALLEE(argv);
  jsval functionHolder;
  if (!JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(callee), 0, &functionHolder)) {
    JS_ReportError(cx, "JS_GetReservedSlot() failed.");
    return JS_FALSE;
  }

  PyObject *callable;
  if (!PYM_getHeldFunction(cx, JSVAL_TO_OBJECT(functionHolder), &callable))
    return JS_FALSE;

  PYM_JSContextObject *context = (PYM_JSContextObject *)
    JS_GetContextPrivate(cx);

  jsval thisArg = OBJECT_TO_JSVAL(obj);
  PyObject *pyThisArg = PYM_jsvalToPyObject(context, thisArg);
  if (pyThisArg == NULL) {
    PYM_pythonExceptionToJs(context);
    return JS_FALSE;
  }

  PyObject *funcArgs = PyTuple_New(argc);
  if (funcArgs == NULL) {
    Py_DECREF(pyThisArg);
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  for (unsigned int i = 0; i < argc; i++) {
    PyObject *arg = PYM_jsvalToPyObject(context, argv[i]);
    if (arg == NULL || PyTuple_SetItem(funcArgs, i, arg)) {
      if (arg)
        Py_DECREF(arg);
      Py_DECREF(funcArgs);
      Py_DECREF(pyThisArg);
      PYM_pythonExceptionToJs(context);
      return JS_FALSE;
    }
  }

  PyObject *args = PyTuple_Pack(3,
                                (PyObject *) context,
                                pyThisArg,
                                funcArgs);
  Py_DECREF(pyThisArg);
  Py_DECREF(funcArgs);
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

  int error = PYM_pyObjectToJsval(context, result, rval);
  Py_DECREF(result);

  if (error) {
    PYM_pythonExceptionToJs(context);
    return JS_FALSE;
  }

  return JS_TRUE;
}

PyTypeObject PYM_JSFunctionType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Function",         /*tp_name*/
  sizeof(PYM_JSFunction),      /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSFunctionDealloc,
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
  Py_TPFLAGS_DEFAULT,
  /* tp_doc */
  "JavaScript Function.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,                           /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  0,                           /* tp_methods */
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

PYM_JSFunction *
PYM_newJSFunctionFromCallable(PYM_JSContextObject *context,
                              PyObject *callable,
                              const char *name)
{
  if (!PyCallable_Check(callable)) {
    PyErr_SetString(PyExc_TypeError, "Callable must be callable");
    return NULL;
  }

  JSFunction *func = JS_NewFunction(context->cx,
                                    PYM_dispatchJSFunctionToPython, 0,
                                    0, NULL, name);

  if (func == NULL) {
    PyErr_SetString(PYM_error, "JS_DefineFunction() failed");
    return NULL;
  }

  JSObject *funcObj = JS_GetFunctionObject(func);

  if (funcObj == NULL) {
    PyErr_SetString(PYM_error, "JS_GetFunctionObject() failed");
    return NULL;
  }

  PYM_JSFunction *object = PyObject_New(PYM_JSFunction,
                                        &PYM_JSFunctionType);
  if (object == NULL)
    return NULL;

  if (PYM_newJSObject(context, funcObj,
                      (PYM_JSObject *) object) == NULL)
    // Note that our object's reference count will have already
    // been decremented by PYM_newJSObject().
    return NULL;

  JSObject *functionHolder = PYM_newFunctionHolder(context->cx, callable);
  if (functionHolder == NULL) {
    Py_DECREF((PyObject *) object);
    return NULL;
  }

  if (!JS_SetReservedSlot(context->cx, funcObj, 0,
                          OBJECT_TO_JSVAL(functionHolder))) {
    Py_DECREF((PyObject *) object);
    PyErr_SetString(PYM_error, "JS_SetReservedSlot() failed");
    return NULL;
  }

  return object;
}
