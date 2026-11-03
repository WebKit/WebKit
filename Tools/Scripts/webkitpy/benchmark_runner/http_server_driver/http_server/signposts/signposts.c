#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include <os/log.h>
#include <os/signpost.h>
#include <stdio.h>

#define SIGNPOST_NAME "JSGlobalObject"

static os_log_t handle;
static os_signpost_id_t id;

static PyObject *interval_begin(PyObject *self, PyObject *args) {
    const char* s;
    if (!PyArg_ParseTuple(args, "s", &s))
        return NULL;

    os_signpost_interval_begin(handle, id, SIGNPOST_NAME, "%s", s);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *interval_end(PyObject *self, PyObject *args) {
    const char* s;
    if (!PyArg_ParseTuple(args, "s", &s))
        return NULL;

    os_signpost_interval_end(handle, id, SIGNPOST_NAME, "%s", s);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyObject *event_emit(PyObject *self, PyObject *args) {
    const char* s;
    if (!PyArg_ParseTuple(args, "s", &s))
        return NULL;

    os_signpost_event_emit(handle, id, SIGNPOST_NAME, "%s", s);

    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef module_methods[] = {
    { "interval_begin", interval_begin, METH_VARARGS, "Emit interval begin signpost" },
    { "interval_end", interval_end, METH_VARARGS, "Emit interval end signpost" },
    { "event_emit", event_emit, METH_VARARGS, "Emit event signpost" },
    { NULL, NULL, 0, NULL }
};

static PyModuleDef module = {
    PyModuleDef_HEAD_INIT,
    "signposts",
    NULL,
    -1,
    module_methods
};

PyMODINIT_FUNC
PyInit_signposts(void) {
    PyObject *obj = PyModule_Create(&module);

    handle = os_log_create("UnknownBrowser", "Signposts");
    id = os_signpost_id_generate(handle);

    return obj;
}
