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

static void
add_member( PyObject *dict, const char *key, libspectrum_dword value )
{
  PyObject *key_object, *value_object;

  key_object = PyUnicode_FromString(key);
  value_object = PyLong_FromUnsignedLong(value);

  PyDict_SetItem( dict, key_object, value_object );
}

static PyObject*
module_get_z80( PyObject *self, PyObject *args )
{
  PyObject *z80_obj;

  if( !PyArg_ParseTuple( args, ":get_z80" ) )
    return NULL;

  z80_obj = PyDict_New();

  add_member( z80_obj, "af", z80.af.w );
  add_member( z80_obj, "bc", z80.bc.w );
  add_member( z80_obj, "de", z80.de.w );
  add_member( z80_obj, "hl", z80.hl.w );

  add_member( z80_obj, "af'", z80.af_.w );
  add_member( z80_obj, "bc'", z80.bc_.w );
  add_member( z80_obj, "de'", z80.de_.w );
  add_member( z80_obj, "hl'", z80.hl_.w );

  return z80_obj;
}

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
  return PyModule_Create( &module );
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
