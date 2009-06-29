#include "context.h"

static void
PYM_JSContextDealloc(PYM_JSContextObject *self)
{
  if (self->cx) {
    JS_DestroyContext(self->cx);
    self->cx = NULL;
  }

  Py_DECREF(self->runtime);
  self->runtime = NULL;

  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
PYM_getRuntime(PYM_JSContextObject *self, PyObject *args)
{
  Py_INCREF(self->runtime);
  return (PyObject *) self->runtime;
}

static PyMethodDef PYM_JSContextMethods[] = {
  {"get_runtime", (PyCFunction) PYM_getRuntime, METH_VARARGS,
   "Get the JavaScript runtime associated with this context."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSContextType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Context",          /*tp_name*/
  sizeof(PYM_JSRuntimeObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSContextDealloc,
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
  "JavaScript Context.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  PYM_JSContextMethods,        /* tp_methods */
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
