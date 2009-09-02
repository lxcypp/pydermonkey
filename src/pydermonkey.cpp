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

#include "undefined.h"
#include "runtime.h"
#include "context.h"
#include "object.h"
#include "function.h"
#include "script.h"
#include "utils.h"

static PyObject *
PYM_getDebugInfo(PyObject *self, PyObject *args)
{
  PyObject *info = Py_BuildValue("{sI}",
                                 "runtime_count", PYM_getJSRuntimeCount());
  return info;
}

static PyMethodDef PYM_methods[] = {
  {"get_debug_info", PYM_getDebugInfo, METH_VARARGS,
   "Get debugging information about the module."},
  {NULL, NULL, 0, NULL}
};

PyMODINIT_FUNC
initpydermonkey(void)
{
  PyObject *module;

  module = Py_InitModule("pydermonkey", PYM_methods);
  if (module == NULL)
    return;

  if (PyType_Ready(&PYM_undefinedType) < 0)
    return;

  PYM_undefined = PyObject_New(PYM_undefinedObject, &PYM_undefinedType);
  if (PYM_undefined == NULL)
    return;
  Py_INCREF(PYM_undefined);
  PyModule_AddObject(module, "undefined", (PyObject *) PYM_undefined);

  PYM_error = PyErr_NewException("pydermonkey.error", NULL, NULL);
  Py_INCREF(PYM_error);
  PyModule_AddObject(module, "error", PYM_error);

  if (!PyType_Ready(&PYM_JSRuntimeType) < 0)
    return;

  Py_INCREF(&PYM_JSRuntimeType);
  PyModule_AddObject(module, "Runtime", (PyObject *) &PYM_JSRuntimeType);

  if (!PyType_Ready(&PYM_JSContextType) < 0)
    return;

  Py_INCREF(&PYM_JSContextType);
  PyModule_AddObject(module, "Context", (PyObject *) &PYM_JSContextType);

  if (!PyType_Ready(&PYM_JSObjectType) < 0)
    return;

  Py_INCREF(&PYM_JSObjectType);
  PyModule_AddObject(module, "Object", (PyObject *) &PYM_JSObjectType);

  PYM_JSFunctionType.tp_base = &PYM_JSObjectType;
  if (!PyType_Ready(&PYM_JSFunctionType) < 0)
    return;

  Py_INCREF(&PYM_JSFunctionType);
  PyModule_AddObject(module, "Function", (PyObject *) &PYM_JSFunctionType);

  PYM_JSScriptType.tp_base = &PYM_JSObjectType;
  if (!PyType_Ready(&PYM_JSScriptType) < 0)
    return;

  Py_INCREF(&PYM_JSScriptType);
  PyModule_AddObject(module, "Script", (PyObject *) &PYM_JSScriptType);

  if (PyModule_AddStringConstant(module, "__version__", PYM_VERSION) < 0)
    return;
}
