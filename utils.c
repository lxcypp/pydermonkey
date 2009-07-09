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

#include "utils.h"
#include "undefined.h"
#include "object.h"

PyObject *PYM_error;

static int
PYM_doubleToJsval(PYM_JSContextObject *context,
                  double number,
                  jsval *rval)
{
  jsdouble *numberAsJsdouble = JS_NewDouble(context->cx, number);
  if (numberAsJsdouble == NULL) {
    PyErr_SetString(PYM_error, "JS_NewDouble() failed");
    return -1;
  }
  *rval = DOUBLE_TO_JSVAL(numberAsJsdouble);
  return 0;
}

int
PYM_pyObjectToJsval(PYM_JSContextObject *context,
                    PyObject *object,
                    jsval *rval)
{
#ifndef Py_UNICODE_WIDE
  if (PyUnicode_Check(object)) {
    Py_UNICODE *string = PyUnicode_AsUnicode(object);
    JSString *jsString = JS_NewUCStringCopyZ(context->cx,
                                             (const jschar *) string);
    if (jsString == NULL) {
      PyErr_SetString(PYM_error, "JS_NewUCStringCopyZ() failed");
      return -1;
    }

    *rval = STRING_TO_JSVAL(jsString);
    return 0;
  }
#endif

  if (PyInt_Check(object)) {
    long number = PyInt_AS_LONG(object);
    if (INT_FITS_IN_JSVAL(number)) {
      *rval = INT_TO_JSVAL(number);
      return 0;
    } else
      return PYM_doubleToJsval(context, number, rval);
  }

  if (PyFloat_Check(object))
    return PYM_doubleToJsval(context, PyFloat_AS_DOUBLE(object), rval);

  if (PyObject_TypeCheck(object, &PYM_JSObjectType)) {
    PYM_JSObject *jsObject = (PYM_JSObject *) object;
    JSRuntime *rt = JS_GetRuntime(context->cx);
    if (rt != jsObject->runtime->rt) {
      PyErr_SetString(PyExc_ValueError,
                      "JS object and JS context are from different "
                      "JS runtimes");
      return -1;
    }
    *rval = OBJECT_TO_JSVAL(jsObject->obj);
    return 0;
  }

  if (object == Py_True) {
    *rval = JSVAL_TRUE;
    return 0;
  }

  if (object == Py_False) {
    *rval = JSVAL_FALSE;
    return 0;
  }

  if (object == Py_None) {
    *rval = JSVAL_NULL;
    return 0;
  }

  // TODO: Support more types.
  PyErr_SetString(PyExc_NotImplementedError,
                  "Data type conversion not implemented.");
  return -1;
}

PyObject *
PYM_jsvalToPyObject(PYM_JSContextObject *context,
                    jsval value) {
  if (JSVAL_IS_INT(value))
    return PyInt_FromLong(JSVAL_TO_INT(value));

  if (JSVAL_IS_DOUBLE(value)) {
    jsdouble *doubleRef = JSVAL_TO_DOUBLE(value);
    return PyFloat_FromDouble(*doubleRef);
  }

  if (value == JSVAL_FALSE)
    Py_RETURN_FALSE;

  if (value == JSVAL_TRUE)
    Py_RETURN_TRUE;

  if (JSVAL_IS_NULL(value))
    Py_RETURN_NONE;

  if (JSVAL_IS_VOID(value))
    Py_RETURN_UNDEFINED;

  if (JSVAL_IS_STRING(value)) {
    // Strings in JS are funky: think of them as 16-bit versions of
    // Python 2.x's 'str' type.  Whether or not they're valid UTF-16
    // is entirely up to the client code.

    // TODO: Instead of ignoring errors, consider actually treating
    // the string as a raw character buffer.
    JSString *str = JSVAL_TO_STRING(value);
    const char *chars = (const char *) JS_GetStringChars(str);
    size_t length = JS_GetStringLength(str);

    // We're multiplying length by two since Python wants the number
    // of bytes, not the number of 16-bit characters.
    return PyUnicode_DecodeUTF16(chars, length * 2, "ignore", NULL);
  }

  if (JSVAL_IS_OBJECT(value))
    return (PyObject *) PYM_newJSObject(context, JSVAL_TO_OBJECT(value),
                                        NULL);

  // TODO: Support more types.
  PyErr_SetString(PyExc_NotImplementedError,
                  "Data type conversion not implemented.");
  return NULL;
}

void
PYM_pythonExceptionToJs(PYM_JSContextObject *context)
{
  PyObject *type;
  PyObject *value;
  PyObject *traceback;

  PyErr_Fetch(&type, &value, &traceback);
  PyObject *str = PyObject_Unicode(value);
  if (str == NULL)
    JS_ReportError(context->cx, "Python exception occurred");
  else {
    jsval val;
    if (PYM_pyObjectToJsval(context, str, &val) == 0) {
      // TODO: Wrap Python traceback info in JS exception so the client
      // can examine it.
      JS_SetPendingException(context->cx, val);
    } else
      JS_ReportError(context->cx, "Python exception occurred");
    Py_DECREF(str);
  }

  Py_XDECREF(type);
  Py_XDECREF(value);
  Py_XDECREF(traceback);
}

void
PYM_jsExceptionToPython(PYM_JSContextObject *context)
{
  if (!JS_IsExceptionPending(context->cx) &&
      PyErr_Occurred())
    return;

  jsval val;
  if (JS_GetPendingException(context->cx, &val)) {
    JSString *str = JS_ValueToString(context->cx, val);
    if (str != NULL) {
      // TODO: Wrap the original JS exception so that the client can
      // examine it.
      const char *chars = JS_GetStringBytes(str);
      PyErr_SetString(PYM_error, chars);
    } else
      PyErr_SetString(PYM_error, "JS exception occurred");
  } else
    PyErr_SetString(PYM_error, "JS_GetPendingException() failed");
}
