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

#include "object.h"
#include "function.h"
#include "runtime.h"
#include "utils.h"

JSObject *
PYM_JS_newObject(JSContext *cx, PyObject *pyObject)
{
  JSObject *obj = JS_NewObject(cx, &PYM_JS_ObjectClass, NULL, NULL);
  if (obj) {
    if (!JS_SetPrivate(cx, obj, pyObject))
      return NULL;
    Py_XINCREF(pyObject);
  }
  return obj;
}

JSBool
PYM_JS_setPrivatePyObject(JSContext *cx, JSObject *obj, PyObject *pyObject)
{
  PyObject *old;
  if (!PYM_JS_getPrivatePyObject(cx, obj, &old))
    return JS_FALSE;
  if (!JS_SetPrivate(cx, obj, pyObject))
    return JS_FALSE;
  Py_INCREF(pyObject);
  Py_XDECREF(old);
  return JS_TRUE;
}

JSBool
PYM_JS_getPrivatePyObject(JSContext *cx, JSObject *obj, PyObject **pyObject)
{
  JSClass *klass = JS_GET_CLASS(cx, obj);
  if (klass != &PYM_JS_ObjectClass) {
    JS_ReportError(cx, "Object is not an instance of PymonkeyObject");
    return JS_FALSE;
  }

  *pyObject = (PyObject *) JS_GetPrivate(cx, obj);
  return JS_TRUE;
}

static void
PYM_JS_finalizeObject(JSContext *cx, JSObject *obj)
{
  PYM_PyAutoEnsureGIL gil;
  PyObject *pyObject;
  // TODO: What if this fails?
  if (PYM_JS_getPrivatePyObject(cx, obj, &pyObject))
    Py_XDECREF(pyObject);
}

// This one-size-fits-all JSClass is used for any JS objects created
// in Python.  It can hold a reference to a Python object for as long as
// its parent JS object is accessible from JS-land. As soon as it's
// garbage collected by the JS interpreter, it releases its reference on
// the Python object.
JSClass PYM_JS_ObjectClass = {
  "PymonkeyObject", JSCLASS_GLOBAL_FLAGS | JSCLASS_HAS_PRIVATE,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, PYM_JS_finalizeObject,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static void
PYM_JSObjectDealloc(PYM_JSObject *self)
{
  if (self->obj) {
    JS_DHashTableOperate(&self->runtime->objects,
                         (void *) self->obj,
                         JS_DHASH_REMOVE);

    // JS_RemoveRoot() always returns JS_TRUE, so don't
    // bother checking its return value.
    JS_RemoveRootRT(self->runtime->rt, &self->obj);
    self->obj = NULL;
  }

  if (self->runtime) {
    Py_DECREF(self->runtime);
    self->runtime = NULL;
  }

  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
PYM_getRuntime(PYM_JSObject *self, PyObject *args)
{
  Py_INCREF(self->runtime);
  return (PyObject *) self->runtime;
}

static PyMethodDef PYM_JSObjectMethods[] = {
  {"get_runtime", (PyCFunction) PYM_getRuntime, METH_VARARGS,
   "Get the JavaScript runtime associated with this object."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSObjectType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Object",           /*tp_name*/
  sizeof(PYM_JSObject),        /*tp_basicsize*/
  0,                           /*tp_itemsize*/
                               /*tp_dealloc*/
  (destructor) PYM_JSObjectDealloc,
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
  Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
                               /* tp_doc */
  "JavaScript Object.",
  0,                           /* tp_traverse */
  0,                           /* tp_clear */
  0,                           /* tp_richcompare */
  0,                           /* tp_weaklistoffset */
  0,                           /* tp_iter */
  0,                           /* tp_iternext */
  PYM_JSObjectMethods,         /* tp_methods */
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

PYM_JSObject *PYM_findJSObject(PYM_JSContextObject *context, JSObject *obj)
{
  PYM_HashEntry *cached = (PYM_HashEntry *) JS_DHashTableOperate(
    &context->runtime->objects,
    (void *) obj,
    JS_DHASH_LOOKUP
    );

  if (JS_DHASH_ENTRY_IS_BUSY((JSDHashEntryHdr *) cached)) {
    Py_INCREF((PyObject *) cached->value);
    return (PYM_JSObject *) cached->value;
  }

  return NULL;
}

PYM_JSObject *PYM_newJSObject(PYM_JSContextObject *context,
                              JSObject *obj,
                              PYM_JSObject *subclass)
{
  PYM_JSObject *cachedObject = PYM_findJSObject(context, obj);
  if (cachedObject)
    return cachedObject;

  PYM_JSObject *object;

  if (subclass)
    object = subclass;
  else {
    if (JS_ObjectIsFunction(context->cx, obj)) {
      PYM_JSFunction *func = PyObject_New(PYM_JSFunction,
                                          &PYM_JSFunctionType);
      object = (PYM_JSObject *) func;
    } else
      object = PyObject_New(PYM_JSObject,
                            &PYM_JSObjectType);
  }

  if (object == NULL)
    return NULL;

  object->runtime = NULL;
  object->obj = NULL;

  PYM_HashEntry *cached = (PYM_HashEntry *) JS_DHashTableOperate(
    &context->runtime->objects,
    (void *) obj,
    JS_DHASH_ADD
    );

  if (cached == NULL) {
    Py_DECREF(object);
    PyErr_SetString(PYM_error, "JS_DHashTableOperate() failed");
    return NULL;
  }

  cached->base.key = (void *) obj;
  cached->value = object;

  object->runtime = context->runtime;
  Py_INCREF(object->runtime);

  object->obj = obj;

  JS_AddNamedRootRT(object->runtime->rt, &object->obj,
                    "Pymonkey-Generated Object");

  return object;
}
