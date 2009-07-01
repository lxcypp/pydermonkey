#include "utils.h"
#include "undefined.h"
#include "object.h"

PyObject *PYM_error;

static PyObject *
PYM_doubleToJsval(JSContext *cx,
                  double number,
                  jsval *rval)
{
  jsdouble *numberAsJsdouble = JS_NewDouble(cx, number);
  if (numberAsJsdouble == NULL) {
    PyErr_SetString(PYM_error, "JS_NewDouble() failed");
    return NULL;
  }
  *rval = DOUBLE_TO_JSVAL(numberAsJsdouble);
  Py_RETURN_NONE;
}

PyObject *
PYM_pyObjectToJsval(JSContext *cx,
                    PyObject *object,
                    jsval *rval)
{
#ifndef Py_UNICODE_WIDE
  if (PyUnicode_Check(object)) {
    Py_UNICODE *string = PyUnicode_AsUnicode(object);
    JSString *jsString = JS_NewUCStringCopyZ(cx,
                                             (const jschar *) string);
    if (jsString == NULL) {
      PyErr_SetString(PYM_error, "JS_NewUCStringCopyZ() failed");
      return NULL;
    }

    *rval = STRING_TO_JSVAL(jsString);
    Py_RETURN_NONE;
  }
#endif

  if (PyInt_Check(object)) {
    long number = PyInt_AS_LONG(object);
    if (INT_FITS_IN_JSVAL(number)) {
      *rval = INT_TO_JSVAL(number);
      Py_RETURN_NONE;
    } else
      return PYM_doubleToJsval(cx, number, rval);
  }

  if (PyFloat_Check(object))
    return PYM_doubleToJsval(cx, PyFloat_AS_DOUBLE(object), rval);

  // TODO: Support more types.
  PyErr_SetString(PyExc_NotImplementedError,
                  "Data type conversion not implemented.");
  return NULL;
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
    return (PyObject *) PYM_newJSObject(context, JSVAL_TO_OBJECT(value));

  // TODO: Support more types.
  PyErr_SetString(PyExc_NotImplementedError,
                  "Data type conversion not implemented.");
  return NULL;
}
