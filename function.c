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

static void
PYM_JSFunctionDealloc(PYM_JSFunction *self)
{
  // TODO: What if there's still a reference to the callable in
  // JS-land?
  if (self->callable) {
    Py_DECREF(self->callable);
    self->callable = NULL;
  }
  PYM_JSObjectType.tp_dealloc((PyObject *) self);
}

static JSBool
dispatchJSFunctionToPython(JSContext *cx,
                           JSObject *obj,
                           uintN argc,
                           jsval *argv,
                           jsval *rval)
{
  jsval callee = JS_ARGV_CALLEE(argv);
  jsval jsCallable;
  if (!JS_GetReservedSlot(cx, JSVAL_TO_OBJECT(callee), 0, &jsCallable)) {
    JS_ReportError(cx, "JS_GetReservedSlot() failed.");
    return JS_FALSE;
  }
  PyObject *callable = (PyObject *) JSVAL_TO_PRIVATE(jsCallable);

  // TODO: Convert args and 'this' parameter.
  PyObject *args = PyTuple_New(1);
  if (args == NULL) {
    JS_ReportOutOfMemory(cx);
    return JS_FALSE;
  }

  PYM_JSContextObject *context = (PYM_JSContextObject *)
    JS_GetContextPrivate(cx);
  Py_INCREF(context);
  PyTuple_SET_ITEM(args, 0, (PyObject *) context);

  PyObject *result = PyObject_Call(callable, args, NULL);
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
                                    dispatchJSFunctionToPython, 0,
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

  if (!JS_SetReservedSlot(context->cx, funcObj, 0,
                          PRIVATE_TO_JSVAL(callable))) {
    PyErr_SetString(PYM_error, "JS_SetReservedSlot() failed");
    return NULL;
  }

  PYM_JSFunction *object = PyObject_New(PYM_JSFunction,
                                        &PYM_JSFunctionType);
  if (object == NULL)
    return NULL;

  object->callable = NULL;
  if (PYM_newJSObject(context, funcObj,
                      (PYM_JSObject *) object) == NULL)
    // Note that our object's reference count will have already
    // been decremented by PYM_newJSObject().
    return NULL;

  object->callable = callable;
  Py_INCREF(callable);

  return object;
}
