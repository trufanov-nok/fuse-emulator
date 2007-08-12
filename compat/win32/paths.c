/* paths.c: Path-related compatability routines
   Copyright (c) 1999-2007 Philip Kendall

   $Id$

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

#include <stdlib.h>

#include "compat.h"

const char*
compat_get_temp_path( void )
{
  const char *dir;

  /* Something close to this algorithm specified at
     http://msdn.microsoft.com/library/default.asp?url=/library/en-us/fileio/base/gettemppath.asp
  */
  dir = getenv( "TMP" ); if( dir ) return dir;
  dir = getenv( "TEMP" ); if( dir ) return dir;
  return ".";
}

const char*
compat_get_home_path( void )
{
  const char *dir;
  dir = getenv( "USERPROFILE" ); if( dir ) return dir;
  dir = getenv( "WINDIR" ); if( dir ) return dir;
  return ".";
}