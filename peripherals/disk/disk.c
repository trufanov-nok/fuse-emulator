/* disk.c: Routines for handling disk images
   Copyright (c) 2007-2017 Gergely Szasz

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

   Philip: philip-fuse@shadowmagic.org.uk

*/

#include <config.h>

#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include <libspectrum.h>

#include "disk.h"
#include "settings.h"
#include "trdos.h"
#include "ui/ui.h"
#include "utils.h"

static libspectrum_disk_error_t
disk_open2( libspectrum_disk *d, const char *filename, int preindex )
{
  utils_file file;
  int error;
  libspectrum_disk dw;

#ifdef GEKKO		/* Wii doesn't have access() */
  d->wrprot = 0;
#else			/* #ifdef GEKKO */
  if( access( filename, W_OK ) == -1 )		/* file read only */
    d->wrprot = 1;
  else
    d->wrprot = 0;
#endif			/* #ifdef GEKKO */
  d->flag = preindex ? LIBSPECTRUM_DISK_FLAG_PREIDX : 0;
  if( utils_read_file( filename, &file ) )
    return d->status = LIBSPECTRUM_DISK_OPEN;
  d->filename = utils_safe_strdup( filename );
  error = libspectrum_disk_open( d, file.buffer, file.length );
  utils_close_file( &file );

  if( !error && ( d->type == LIBSPECTRUM_DISK_TRD ||
                  d->type == LIBSPECTRUM_DISK_SCL ) ) {

    if( settings_current.auto_load ) {
      dw = *d;    /* don't modify current position */
      trdos_insert_boot_loader( &dw );
    }
  }

  return d->status = error;
}

libspectrum_disk_error_t
disk_open( libspectrum_disk *d, const char *filename, int preindex,
           int merge_disks )
{
  char *filename2;
  char c = ' ';
  int l, g = 0, pos = 0;
  libspectrum_disk d1, d2;

  d->filename = NULL;
  if( filename == NULL || *filename == '\0' )
    return d->status = LIBSPECTRUM_DISK_OPEN;

  l = strlen( filename );

  if( !merge_disks || l < 7 )	/* if we do not want to open two separated disk image as one double sided disk */
    return disk_open2( d, filename, preindex );

  filename2 = (char *)filename + ( l - 1 );
  while( l ) {				/* [Ss]ide[ _][abAB12][ _.] */
    if( g == 0 && ( *filename2 == '.' || *filename2 == '_' ||
		    *filename2 == ' ' ) ) {
      g++;
    } else if( g == 1 && ( *filename2 == '1' || *filename2 == 'a' ||
			   *filename2 == 'A' ) ) {
      g++;
      pos = filename2 - filename;
      c = *filename2 + 1;		/* 1->2, a->b, A->B */
    } else if( g == 1 && ( *filename2 == '2' || *filename2 == 'b' ||
			   *filename2 == 'B' ) ) {
      g++;
      pos = filename2 - filename;
      c = *filename2 - 1;		/* 2->1, b->a, B->A */
    } else if( g == 2 && ( *filename2 == '_' || *filename2 == ' ' ) ) {
      g++;
    } else if( g == 3 && l >= 5 && ( !memcmp( filename2 - 3, "Side", 4 ) ||
				     !memcmp( filename2 - 3, "side", 4 ) ) ) {
      g++;
      break;
    } else {
      g = 0;
    }
    l--;
    filename2--;
  }
  if( g != 4 )
    return d->status = disk_open2( d, filename, preindex );
  d1.data = NULL; d1.flag = d->flag;
  d2.data = NULL; d2.flag = d->flag;
  filename2 = utils_safe_strdup( filename );
  *(filename2 + pos) = c;

  if( settings_current.disk_ask_merge &&
      !ui_query( "Try to merge 'B' side of this disk?" ) ) {
    libspectrum_free( filename2 );
    return d->status = disk_open2( d, filename, preindex );
  }

  if( disk_open2( &d2, filename2, preindex ) ) {
    return d->status = disk_open2( d, filename, preindex );
  }

  if( disk_open2( &d1, filename, preindex ) )
    return d->status = d1.status;

  if( libspectrum_disk_merge_sides( d, &d1, &d2, 0x00 ) ) {
    libspectrum_disk_close( &d2 );
    *d = d1;
  }
/*  fprintf( stderr, "`%s' and `%s' merged\n", filename, filename2 ); */
  libspectrum_free( filename2 );
  return d->status;
}

libspectrum_disk_error_t
disk_write( libspectrum_disk *d, const char *filename )
{
  libspectrum_byte *buffer = NULL;
  size_t length;
  FILE *file;
  libspectrum_disk dw;

  if( ( file = fopen( filename, "wb" ) ) == NULL )
    return d->status = LIBSPECTRUM_DISK_WRFILE;

  dw = *d;    /* don't modify current position */
  libspectrum_disk_write( &dw, &buffer, &length, filename );
  if( dw.status != LIBSPECTRUM_DISK_OK ) {
    if( buffer != NULL )
      libspectrum_free( buffer );
    fclose( file );
    return d->status = dw.status;
  }

  if( fwrite( buffer, length, 1, file ) != 1 ) {
    if( buffer != NULL )
      libspectrum_free( buffer );
    fclose( file );
    return d->status = LIBSPECTRUM_DISK_WRFILE;
  }

  libspectrum_free( buffer );

  if( fclose( file ) != 0 )
    return d->status = LIBSPECTRUM_DISK_WRFILE;

  return d->status = LIBSPECTRUM_DISK_OK;
}
