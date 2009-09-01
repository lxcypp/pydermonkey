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

#ifndef PYM_UTILS_H
#define PYM_UTILS_H

#include "context.h"

#include <jsapi.h>
#include <jsdhash.h>
#include <Python.h>

// Simple class that holds on to a UTF-16 string created by
// PyArg_ParseTuple() for as long as it's in scope.  It also
// provides easy BOM-stripping accessors for JS-land.
class PYM_UTF16String {
public:
  PYM_UTF16String(char *buffer, int size) : jsbuffer((jschar *) (buffer + 2)),
    jslen(size / 2 - 1), pybuffer(buffer), pysize(size) {
  }

  ~PYM_UTF16String() {
    PyMem_Free(pybuffer);
  }

  jschar *jsbuffer;
  size_t jslen;

protected:
  char *pybuffer;
  int pysize;
};

// Simple class that holds the Python global interpreter lock (GIL)
// for as long as it's in scope.
class PYM_PyAutoEnsureGIL {
public:
  PYM_PyAutoEnsureGIL() {
    state = PyGILState_Ensure();
  }

  ~PYM_PyAutoEnsureGIL() {
    PyGILState_Release(state);
  }

protected:
  PyGILState_STATE state;
};

typedef struct {
  JSDHashEntryStub base;
  void *value;
} PYM_HashEntry;

extern PyObject *PYM_error;

// Convert a PyObject to a jsval. Returns 0 on success,
// -1 on error. If an error occurs, a Python exception is
// set.
//
// The jsval is placed in rval.
extern int
PYM_pyObjectToJsval(PYM_JSContextObject *context,
                    PyObject *object,
                    jsval *rval);

// Convert a jsval to a PyObject, returning a new reference.
// If this function fails, it sets a Python exception and
// returns NULL.
extern PyObject *
PYM_jsvalToPyObject(PYM_JSContextObject *context, jsval value);

// Converts the currently-pending Python exception to a
// pending JS exception on the given JS context.
extern void
PYM_pythonExceptionToJs(PYM_JSContextObject *context);

// Converts the currently-pending exception on the given
// JS context into a pending Python exception.
void
PYM_jsExceptionToPython(PYM_JSContextObject *context);

#endif
