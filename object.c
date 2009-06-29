#include "object.h"
#include "runtime.h"
#include "utils.h"

JSClass PYM_JS_ObjectClass = {
  "PymonkeyObject", JSCLASS_GLOBAL_FLAGS,
  JS_PropertyStub, JS_PropertyStub, JS_PropertyStub, JS_PropertyStub,
  JS_EnumerateStub, JS_ResolveStub, JS_ConvertStub, JS_FinalizeStub,
  JSCLASS_NO_OPTIONAL_MEMBERS
};

static void
PYM_JSObjectDealloc(PYM_JSObject *self)
{
  if (self->weakrefList)
    PyObject_ClearWeakRefs((PyObject *) self);

  if (self->obj) {
    if (PyDict_DelItem(self->runtime->objects,
                       self->uniqueId) == -1)
      PySys_WriteStderr("WARNING: PyDict_DelItem() failed.\n");
    Py_DECREF(self->uniqueId);
    self->uniqueId = NULL;

    // JS_RemoveRoot() always returns JS_TRUE, so don't
    // bother checking its return value.
    JS_RemoveRootRT(self->runtime->rt, &self->obj);
    self->obj = NULL;
  }

  if (self->runtime) {
    Py_DECREF(self->runtime);
    self->runtime = NULL;
  }
}

PyTypeObject PYM_JSObjectType = {
  PyObject_HEAD_INIT(NULL)
  0,                           /*ob_size*/
  "pymonkey.Object",           /*tp_name*/
  sizeof(PYM_JSObject),        /*tp_basicsize*/
  0,                           /*tp_itemsize*/
  /*tp_dealloc*/
  (destructor) PYM_JSObjectDealloc,
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
  "JavaScript Object.",
  0,		               /* tp_traverse */
  0,		               /* tp_clear */
  0,		               /* tp_richcompare */
  /* tp_weaklistoffset */
  offsetof(PYM_JSObject, weakrefList),
  0,		               /* tp_iter */
  0,		               /* tp_iternext */
  0,                           /* tp_methods */
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

PYM_JSObject *PYM_newJSObject(PYM_JSContextObject *context,
                              JSObject *obj) {
  jsid uniqueId;
  if (!JS_GetObjectId(context->cx, obj, &uniqueId)) {
    PyErr_SetString(PYM_error, "JS_GetObjectId() failed");
    return NULL;
  }
  PyObject *pyUniqueId = PyLong_FromLong(uniqueId);
  if (pyUniqueId == NULL)
    return NULL;

  PYM_JSRuntimeObject *runtime = context->runtime;
  PyObject *cachedObject = PyDict_GetItem(runtime->objects,
                                          pyUniqueId);
  if (cachedObject) {
    cachedObject = PyWeakref_GetObject(cachedObject);
    Py_INCREF(cachedObject);
    Py_DECREF(pyUniqueId);
    return (PYM_JSObject *) cachedObject;
  }

  PYM_JSObject *object = PyObject_New(PYM_JSObject,
                                      &PYM_JSObjectType);
  if (object == NULL) {
    Py_DECREF(pyUniqueId);
    return NULL;
  }

  object->runtime = NULL;
  object->obj = NULL;
  object->uniqueId = NULL;
  object->weakrefList = NULL;

  PyObject *weakref = PyWeakref_NewRef((PyObject *) object, NULL);
  if (weakref == NULL) {
    Py_DECREF(object);
    Py_DECREF(pyUniqueId);
    return NULL;
  }

  if (PyDict_SetItem(runtime->objects, pyUniqueId, weakref) == -1) {
    Py_DECREF(weakref);
    Py_DECREF(object);
    Py_DECREF(pyUniqueId);
    return NULL;
  }

  object->runtime = context->runtime;
  Py_INCREF(object->runtime);

  object->obj = obj;
  object->uniqueId = pyUniqueId;

  JS_AddNamedRootRT(object->runtime->rt, &object->obj,
                    "Pymonkey-Generated Object");

  return object;
}
