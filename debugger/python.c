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

#include "memory_pages.h"
#include "utils.h"

static PyObject *pModule = NULL;

static PyObject*
module_read_memory( PyObject *self, PyObject *args )
{
  unsigned start, length;
  PyObject *bytes;
  char *buffer;
  size_t i;

  printf("In read_memory\n");

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

  printf("Finished read_memory\n");

  return bytes;
}

static PyMethodDef module_methods[] = {
  { "read_memory", module_read_memory, METH_VARARGS, "description" },
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

  printf( "Python init\n" );

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

  printf( "Python hook\n" );

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

    printf( "Just before Python call\n" );
    pReturn = PyObject_CallObject( pFunc, pArgs );
    printf( "Just back from Python call\n" );

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
  printf( "Python end\n" );
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
