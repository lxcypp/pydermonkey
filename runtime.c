#include "runtime.h"
#include "context.h"
#include "utils.h"

static PyObject *
PYM_JSRuntimeNew(PyTypeObject *type, PyObject *args,
                 PyObject *kwds)
{
  PYM_JSRuntimeObject *self;

  self = (PYM_JSRuntimeObject *) type->tp_alloc(type, 0);
  if (self != NULL) {
    self->rt = JS_NewRuntime(8L * 1024L * 1024L);
    if (!self->rt) {
      PyErr_SetString(PYM_error, "JS_NewRuntime() failed");
      type->tp_dealloc((PyObject *) self);
      self = NULL;
    }
  }

  return (PyObject *) self;
}

static void
PYM_JSRuntimeDealloc(PYM_JSRuntimeObject *self)
{
  if (self->rt) {
    JS_DestroyRuntime(self->rt);
    self->rt = NULL;
  }

  self->ob_type->tp_free((PyObject *) self);
}

static PyObject *
PYM_newContext(PYM_JSRuntimeObject *self, PyObject *args)
{
  PYM_JSContextObject *context = PyObject_New(PYM_JSContextObject,
                                              &PYM_JSContextType);

  context->runtime = self;
  Py_INCREF(self);

  context->cx = JS_NewContext(self->rt, 8192);
  if (context->cx == NULL) {
    PyErr_SetString(PYM_error, "JS_NewContext() failed");
    Py_DECREF(context);
    return NULL;
  }

  JS_SetOptions(context->cx, JSOPTION_VAROBJFIX);
  JS_SetVersion(context->cx, JSVERSION_LATEST);

  return (PyObject *) context;
}

static PyMethodDef PYM_JSRuntimeMethods[] = {
  {"new_context", (PyCFunction) PYM_newContext, METH_VARARGS,
   "Create a new JavaScript context."},
  {NULL, NULL, 0, NULL}
};

PyTypeObject PYM_JSRuntimeType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Runtime",          /*tp_name*/
  sizeof(PYM_JSRuntimeObject), /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSRuntimeDealloc,
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
  "JavaScript Runtime.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  0,		               /* tp_weaklistoffset */
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  PYM_JSRuntimeMethods,        /* tp_methods */
  0,                           /* tp_members */
  0,                           /* tp_getset */
  0,                           /* tp_base */
  0,                           /* tp_dict */
  0,                           /* tp_descr_get */
  0,                           /* tp_descr_set */
  0,                           /* tp_dictoffset */
  0,                           /* tp_init */
  0,                           /* tp_alloc */
  PYM_JSRuntimeNew,            /* tp_new */
};
