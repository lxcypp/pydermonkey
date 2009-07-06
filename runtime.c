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

#include "runtime.h"
#include "context.h"
#include "utils.h"

static PyObject *
PYM_JSRuntimeNew(PyTypeObject *type, PyObject *args,
                 PyObject *kwds)
{
  PYM_JSRuntimeObject *self;

  self = (PYM_JSRuntimeObject *) type->tp_alloc(type, 0);
  if (self != NULL) {
    self->rt = NULL;
    self->objects.ops = NULL;

    if (!JS_DHashTableInit(&self->objects,
                           JS_DHashGetStubOps(),
                           NULL,
                           sizeof(PYM_HashEntry),
                           JS_DHASH_DEFAULT_CAPACITY(100))) {
      PyErr_SetString(PYM_error, "JS_DHashTableInit() failed");
      type->tp_dealloc((PyObject *) self);
      self = NULL;
    }

    if (self != NULL) {
      self->rt = JS_NewRuntime(8L * 1024L * 1024L);
      if (!self->rt) {
        PyErr_SetString(PYM_error, "JS_NewRuntime() failed");
        type->tp_dealloc((PyObject *) self);
        self = NULL;
      }
    }
  }

  return (PyObject *) self;
}

static void
PYM_JSRuntimeDealloc(PYM_JSRuntimeObject *self)
{
  if (self->objects.ops) {
    JS_DHashTableFinish(&self->objects);
    self->objects.ops = NULL;
  }

  if (self->rt) {
    JS_DestroyRuntime(self->rt);
    self->rt = NULL;
  }

  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
PYM_newContext(PYM_JSRuntimeObject *self, PyObject *args)
{
  JSContext *cx = JS_NewContext(self->rt, 8192);
  if (cx == NULL) {
    PyErr_SetString(PYM_error, "JS_NewContext() failed");
    return NULL;
  }

  JS_SetOptions(cx, JSOPTION_VAROBJFIX | JSOPTION_DONT_REPORT_UNCAUGHT |
                JSOPTION_ATLINE | JSOPTION_STRICT);
  JS_SetVersion(cx, JSVERSION_LATEST);

  PyObject *retval = (PyObject *) PYM_newJSContextObject(self, cx);

  if (retval == NULL)
    JS_DestroyContext(cx);

  return retval;
}

static PyMethodDef PYM_JSRuntimeMethods[] = {
  {"new_context", (PyCFunction) PYM_newContext, METH_VARARGS,
   "Create a new JavaScript context."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSRuntimeType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Runtime",          /*tp_name*/
  sizeof(PYM_JSRuntimeObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSRuntimeDealloc,
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
  Py_TPFLAGS_DEFAULT,          /*tp_flags*/
  /* tp_doc */
  "JavaScript Runtime.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  PYM_JSRuntimeMethods,        /* tp_methods */
  0,                           /* tp_members */
  0,                           /* tp_getset */
  0,                           /* tp_base */
  0,                           /* tp_dict */
  0,                           /* tp_descr_get */
  0,                           /* tp_descr_set */
  0,                           /* tp_dictoffset */
  0,                           /* tp_init */
  0,                           /* tp_alloc */
  PYM_JSRuntimeNew,            /* tp_new */
};
