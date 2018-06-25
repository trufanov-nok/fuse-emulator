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

#include <Python.h>
#include <stdio.h>

static PyObject *pModule = NULL;

void
debugger_python_init( void )
{
  PyObject *pName;

  printf("Python init\n");

  Py_Initialize();

  pName = PyUnicode_FromString("debugger");
  pModule = PyImport_Import(pName);
  Py_DECREF(pName);

  if( !pModule ) {
    printf("Couldn't load Python module\n");
  }
}

void
debugger_python_hook( void )
{
  PyObject *pFunc;

  printf("Python hook\n");

  if( !pModule ) {
    printf("Python module not loaded\n");
    return;
  }

  pFunc = PyObject_GetAttrString(pModule, "dostuff");

  if( pFunc && PyCallable_Check(pFunc) ) {
    PyObject *pArgs, *pValue;

    pArgs = PyTuple_New(0);
    printf("Just before Python call\n");
    pValue = PyObject_CallObject(pFunc, pArgs);
    printf("Just back from Python call\n");
    Py_DECREF(pArgs);
    Py_XDECREF(pValue);
  } else {
    printf("Couldn't find function\n");
  }

  Py_XDECREF(pFunc);
}

void
debugger_python_end( void )
{
  Py_XDECREF(pModule);
  Py_FinalizeEx();
  printf("Python end\n");
}
