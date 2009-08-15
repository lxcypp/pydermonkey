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

#include "undefined.h"

static Py_ssize_t PYM_undefinedLength(PyObject *o) {
  return 0;
};

static PyMappingMethods PYM_undefinedAsMapping = {
  PYM_undefinedLength,         /*mp_length*/
  0,                           /*mp_subscript*/
  0                            /*mp_ass_subscript*/
};

static PyObject *PYM_undefinedRepr(PyObject *o) {
  return PyString_FromString("pymonkey.undefined");
}

// TODO: We should make this behave as much like JavaScript's
// "undefined" value as possible; e.g., its string value should
// be "undefined", the singleton should be falsy, etc.
PyTypeObject PYM_undefinedType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.undefined",        /*tp_name*/
  sizeof(PYM_undefinedObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  0,                           /*tp_dealloc*/
  0,                           /*tp_print*/
  0,                           /*tp_getattr*/
  0,                           /*tp_setattr*/
  0,                           /*tp_compare*/
  PYM_undefinedRepr,           /*tp_repr*/
  0,                           /*tp_as_number*/
  0,                           /*tp_as_sequence*/
  &PYM_undefinedAsMapping,     /*tp_as_mapping*/
  0,                           /*tp_hash */
  0,                           /*tp_call*/
  PYM_undefinedRepr,           /*tp_str*/
  0,                           /*tp_getattro*/
  0,                           /*tp_setattro*/
  0,                           /*tp_as_buffer*/
  Py_TPFLAGS_DEFAULT,          /*tp_flags*/
  /* tp_doc */
  "Pythonic equivalent of JavaScript's 'undefined' value",
};

PYM_undefinedObject *PYM_undefined;