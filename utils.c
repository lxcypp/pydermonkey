#include "utils.h"
#include "undefined.h"

PyObject *PYM_error;

PyObject *
PYM_jsvalToPyObject(jsval value) {
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

  if (JSVAL_IS_STRING(value) && JS_CStringsAreUTF8()) {
    // TODO: What to do if C strings aren't UTF-8?  The jschar *
    // type isn't actually UTF-16, it's just "UTF-16-ish", so
    // there doesn't seem to be any other lossless way of
    // transferring the string other than perhaps by transmitting
    // its JSON representation.

    JSString *str = JSVAL_TO_STRING(value);
    const char *bytes = JS_GetStringBytes(str);
    const char *errors;
    return PyUnicode_DecodeUTF8(bytes, strlen(bytes), errors);
  }

  // TODO: Support more types.
  PyErr_SetString(PyExc_NotImplementedError,
                  "Data type conversion not implemented.");
}
