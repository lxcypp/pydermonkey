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
  if (PyString_Check(object) || PyUnicode_Check(object)) {
    PyObject *unicode;
    if (PyString_Check(object)) {
      unicode = PyUnicode_FromObject(object);
      if (unicode == NULL)
        return -1;
    } else {
      unicode = object;
      Py_INCREF(unicode);
    }

    PyObject *string = PyUnicode_AsUTF16String(unicode);
    Py_DECREF(unicode);
    if (string == NULL)
      return -1;

    char *buffer = PyString_AS_STRING(string);
    Py_ssize_t size = PyString_GET_SIZE(string);

    // Note that we're manipulating buffer and size here to get rid of
    // the BOM.
    JSString *jsString = JS_NewUCStringCopyN(context->cx,
                                             (const jschar *) (buffer + 2),
                                             (size / 2) - 1);
    Py_DECREF(string);
    if (jsString == NULL) {
      PyErr_SetString(PYM_error, "JS_NewUCStringCopyN() failed");
      return -1;
    }

    *rval = STRING_TO_JSVAL(jsString);
    return 0;
  }

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

  if (object == (PyObject *) PYM_undefined) {
    *rval = JSVAL_VOID;
    return 0;
  }

  // TODO: Support more types.
  //   * We could try converting PyLong objects to
  //     doubles, but this may result in an approximation; not sure if
  //     that's the right thing to do here, or if we should just raise
  //     a ValueError or an OverflowError.
  //   * Consider auto-converting buffer objects to strings.
  PyErr_Format(PyExc_NotImplementedError,
               "Data type conversion not implemented for type '%s'.",
               object->ob_type->tp_name);
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

  PyErr_SetString(PyExc_SystemError, "Unknown jsval type.");
  return NULL;
}

void
PYM_pythonExceptionToJs(PYM_JSContextObject *context)
{
  PyObject *type = PyErr_Occurred();

  if (type && !PyErr_GivenExceptionMatches(type, PYM_error))
    // A kind of exception that we don't want JS code to catch was
    // thrown; just return without setting a pending JS exception, and
    // SpiderMonkey will unroll the stack for us.
    return;

  PyObject *value;
  PyObject *traceback;
  bool success = false;

  PyErr_Fetch(&type, &value, &traceback);

  if (value &&
      (PyTuple_Check(value) || PyObject_HasAttrString(value, "args"))) {
    PyObject *args = NULL;

    if (PyTuple_Check(value)) {
      args = value;
      Py_INCREF(args);
    } else {
      // This reference is new.

      // TODO: What if this returns NULL and a new exception has been
      // raised?
      args = PyObject_GetAttrString(value, "args");
    }

    if (args && PyTuple_Check(args) && PyTuple_Size(args) > 0) {
      // This reference is borrowed.
      PyObject *message = PyTuple_GetItem(args, 0);
      jsval val;
      if (message && PYM_pyObjectToJsval(context, message, &val) == 0) {
        JS_SetPendingException(context->cx, val);
        success = true;
      }
    }

    Py_XDECREF(args);
  }

  if (!success)
    JS_ReportError(context->cx,
                   "Python exception occurred, but exception "
                   "couldn't be converted");

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
    JS_ClearPendingException(context->cx);

    PyObject *pyStr = NULL;
    PyObject *tuple = NULL;
    PyObject *obj = PYM_jsvalToPyObject(context, val);
    if (obj == NULL) {
      PyErr_Clear();
      obj = Py_None;
      Py_INCREF(obj);
    }

    JSString *str = NULL;

    Py_BEGIN_ALLOW_THREADS;
    str = JS_ValueToString(context->cx, val);
    Py_END_ALLOW_THREADS;

    if (str != NULL)
      pyStr = PYM_jsvalToPyObject(context, STRING_TO_JSVAL(str));
    else {
      JS_ClearPendingException(context->cx);
      pyStr = PyString_FromString("<string conversion failed>");
    }

    if (pyStr) {
      tuple = Py_BuildValue("(OO)", obj, pyStr);
      if (tuple)
        PyErr_SetObject(PYM_error, tuple);
    }
    Py_DECREF(obj);
    Py_XDECREF(pyStr);
    Py_XDECREF(tuple);
  } else
    PyErr_SetString(PYM_error, "JS_GetPendingException() failed");
}
