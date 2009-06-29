#include "undefined.h"

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
  "Pythonic equivalent of JavaScript's 'undefined' value",
};

PyObject *PYM_undefined = (PyObject *) &PYM_undefinedType;
