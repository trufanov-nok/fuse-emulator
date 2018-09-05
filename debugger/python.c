/* python.c: Debugger Python integration
   Copyright (c) 2018 Philip Kendall

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License along
   with this program; if not, write to the Free Software Foundation, Inc.,
   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

   Author contact information:

   E-mail: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#ifdef USE_PYTHON

#include <stdio.h>

#include <libspectrum.h>
#include <Python.h>
#include <structmember.h>

#include "debugger.h"
#include "memory_pages.h"
#include "utils.h"
#include "z80/z80.h"

static PyObject *pModule = NULL;

static PyObject*
module_run( PyObject *self, PyObject *args )
{
  if( !PyArg_ParseTuple( args, ":run" ) )
    return NULL;

  debugger_run();

  Py_RETURN_NONE;
}

static PyObject*
module_read_memory( PyObject *self, PyObject *args )
{
  unsigned start, length;
  PyObject *bytes;
  char *buffer;
  size_t i;

  if( !PyArg_ParseTuple( args, "II:read_memory", &start, &length ) )
    return NULL;

  buffer = libspectrum_new( char, length );

  for( i = 0; i < length; i++ )
    buffer[i] = readbyte_internal( start + i );

  bytes = PyByteArray_FromStringAndSize( buffer, length );
  if( !bytes ) {
    libspectrum_free( buffer );
    return NULL;
  }

  return bytes;
}

static PyObject*
module_write_memory( PyObject *self, PyObject *args )
{
  unsigned start;
  Py_buffer buffer;
  char *memory;
  Py_ssize_t i;

  if( !PyArg_ParseTuple( args, "Iy*:write_memory", &start, &buffer ) )
    return NULL;

  /* We can handle only simple buffers */
  if( buffer.shape ) {
    PyBuffer_Release(&buffer);
    return NULL;
  }

  memory = buffer.buf;

  for( i = 0; i < buffer.len; i++ )
    writebyte_internal( start + i, memory[i] );

  PyBuffer_Release(&buffer);

  Py_RETURN_NONE;
}

static PyObject*
module_get_z80( PyObject *self, PyObject *args )
{
  PyObject *z80_obj;

  if( !PyArg_ParseTuple( args, ":get_z80" ) )
    return NULL;

  z80_obj = Py_BuildValue( "{s:i,s:i,s:i,s:i,s:i,s:i,s:i,s:i}",
      "af", z80.af.w,
      "bc", z80.bc.w,
      "de", z80.de.w,
      "hl", z80.hl.w,
      "af'", z80.af_.w,
      "bc'", z80.bc_.w,
      "de'", z80.de_.w,
      "hl'", z80.hl_.w );

  return z80_obj;
}

typedef struct {
  PyObject_HEAD
  libspectrum_word af;
  libspectrum_word bc;
  libspectrum_word de;
  libspectrum_word hl;
  libspectrum_word af_;
  libspectrum_word bc_;
  libspectrum_word de_;
  libspectrum_word hl_;
} z80_object;

static PyObject*
z80_object_new( PyTypeObject *type, PyObject *args, PyObject *kwargs )
{
  z80_object *self;
  self = (z80_object*)type->tp_alloc( type, 0 );
  if( self != NULL ) {
    self->af = z80.af.w;
    self->bc = z80.bc.w;
    self->de = z80.de.w;
    self->hl = z80.hl.w;
    self->af_ = z80.af_.w;
    self->bc_ = z80.bc_.w;
    self->de_ = z80.de_.w;
    self->hl_ = z80.hl_.w;
  }
  return (PyObject*)self;
}

static void
z80_object_dealloc( z80_object *self )
{
  Py_TYPE(self)->tp_free( (PyObject*)self );
}

static PyMemberDef z80_object_members[] = {
  { "af", T_INT, offsetof(z80_object, af), 0, "AF register" },
  { "bc", T_INT, offsetof(z80_object, bc), 0, "BC register" },
  { "de", T_INT, offsetof(z80_object, de), 0, "DE register" },
  { "hl", T_INT, offsetof(z80_object, hl), 0, "HL register" },
  { "af_", T_INT, offsetof(z80_object, af_), 0, "AF' register" },
  { "bc_", T_INT, offsetof(z80_object, bc_), 0, "BC' register" },
  { "de_", T_INT, offsetof(z80_object, de_), 0, "DE' register" },
  { "hl_", T_INT, offsetof(z80_object, hl_), 0, "HL' register" },
  { NULL }
};

static PyTypeObject z80_type = {
  PyVarObject_HEAD_INIT(NULL, 0)
  .tp_name = "fuse.Z80",
  .tp_doc = "Z80 state",
  .tp_basicsize = sizeof(z80_object),
  .tp_itemsize = 0,
  .tp_flags = Py_TPFLAGS_DEFAULT,
  .tp_new = z80_object_new,
  .tp_dealloc = (destructor)z80_object_dealloc,
  .tp_members = z80_object_members
};

static PyMethodDef module_methods[] = {
  { "run", module_run, METH_VARARGS, "description" },
  { "read_memory", module_read_memory, METH_VARARGS, "description" },
  { "write_memory", module_write_memory, METH_VARARGS, "description" },
  { "get_z80", module_get_z80, METH_VARARGS, "description" },
  { NULL, NULL, 0, NULL }
};

static PyModuleDef module = {
  PyModuleDef_HEAD_INIT, "fuse", NULL, -1, module_methods, NULL, NULL, NULL, NULL
};

static PyObject*
create_module( void )
{
  PyObject *m;

  if( PyType_Ready( &z80_type ) < 0 ) return NULL;

  m = PyModule_Create( &module );
  if( m == NULL ) return NULL;

  Py_INCREF( &z80_type );
  PyModule_AddObject( m, "Z80", (PyObject*)&z80_type );

  return m;
}

void
debugger_python_init( void )
{
  PyObject *pName;

  PyImport_AppendInittab( "fuse", &create_module );
  Py_Initialize();

  pName = PyUnicode_FromString( "debugger" );
  pModule = PyImport_Import( pName );
  Py_DECREF(pName);

  if( !pModule ) {
    printf( "Couldn't load Python module\n" );
  }
}

void
debugger_python_hook( size_t breakpoint_id )
{
  PyObject *pFunc;

  if( !pModule ) {
    printf( "Python module not loaded\n" );
    return;
  }

  pFunc = PyObject_GetAttrString( pModule, "breakpoint" );

  if( pFunc && PyCallable_Check( pFunc ) ) {
    PyObject *pArgs, *pArgument, *pReturn;

    pArgs = PyTuple_New( 1 );
    pArgument = PyLong_FromLong( breakpoint_id );
    PyTuple_SetItem( pArgs, 0, pArgument );

    pReturn = PyObject_CallObject( pFunc, pArgs );

    if( !pReturn ) PyErr_Print();

    Py_DECREF( pArgs );
    Py_XDECREF( pReturn );
  } else {
    printf( "Couldn't find function\n" );
  }

  Py_XDECREF( pFunc );
}

void
debugger_python_end( void )
{
  Py_XDECREF( pModule );
  Py_FinalizeEx();
}

#else /* #ifdef USE_PYTHON */

/* Stub versions of the functions if Python isn't available */

#include <stdlib.h>

void
debugger_python_init( void )
{
}

void
debugger_python_hook( size_t breakpoint_id )
{
}

void
debugger_python_end( void )
{
}

#endif
