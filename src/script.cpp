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

#include "script.h"
#include "utils.h"

static void
PYM_JSScriptDealloc(PYM_JSScript *self)
{
  self->script = NULL;
  PYM_JSObjectType.tp_dealloc((PyObject *) self);
}

PyTypeObject PYM_JSScriptType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Script",           /*tp_name*/
  sizeof(PYM_JSScript),        /*tp_basicsize*/
  0,                           /*tp_itemsize*/
                               /*tp_dealloc*/
  (destructor) PYM_JSScriptDealloc,
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
  "JavaScript Script.",
  0,                           /* tp_traverse */
  0,                           /* tp_clear */
  0,                           /* tp_richcompare */
  0,                           /* tp_weaklistoffset */
  0,                           /* tp_iter */
  0,                           /* tp_iternext */
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

PYM_JSScript *
PYM_newJSScript(PYM_JSContextObject *context, JSScript *script)
{
  JSObject *scriptObj = JS_GetScriptObject(script);
  PYM_JSScript *object = NULL;

  if (scriptObj)
    object = (PYM_JSScript *) PYM_findJSObject(context, scriptObj);
  else {
    scriptObj = JS_NewScriptObject(context->cx, script);

    if (scriptObj == NULL) {
      PyErr_SetString(PYM_error, "JS_NewScriptObject() failed");
      return NULL;
    }
  }

  if (object == NULL) {
    object = PyObject_New(PYM_JSScript, &PYM_JSScriptType);
    if (object == NULL)
      return NULL;

    object->script = script;
    return (PYM_JSScript *) PYM_newJSObject(context, scriptObj,
                                            (PYM_JSObject *) object);
  }
  return object;
}
