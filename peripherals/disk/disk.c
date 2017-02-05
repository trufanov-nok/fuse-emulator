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

static void
position_context_save( const libspectrum_disk_t *d, disk_position_context_t *c )
{
  c->track  = d->track;
  c->clocks = d->clocks;
  c->fm     = d->fm;
  c->weak   = d->weak;
  c->i      = d->i;
}

static void
position_context_restore( libspectrum_disk_t *d,
                          const disk_position_context_t *c )
{
  d->track  = c->track;
  d->clocks = c->clocks;
  d->fm     = c->fm;
  d->weak   = c->weak;
  d->i      = c->i;
}

/* 1 RANDOMIZE USR 15619: REM : RUN "        " */
static libspectrum_byte beta128_boot_loader[] = {
  0x00, 0x01, 0x1c, 0x00, 0xf9, 0xc0, 0x31, 0x35, 0x36, 0x31, 0x39, 0x0e, 
  0x00, 0x00, 0x03, 0x3d, 0x00, 0x3a, 0xea, 0x3a, 0xf7, 0x22, 0x20, 0x20, 
  0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x22, 0x0d, 
};

static int
trdos_insert_basic_file( libspectrum_disk_t *d, trdos_spec_t *spec,
                         const libspectrum_byte *data, unsigned int size )
{
  unsigned int fat_sector, fat_entry, n_sec, n_bytes, n_copied;
  int i, t, s, slen, snum, del;
  size_t length;
  trdos_dirent_t entry;
  libspectrum_byte head[256];
  libspectrum_byte *buffer;
  libspectrum_byte trailing_data[] = { 0x80, 0xaa, 0x01, 0x00 }; /* line 1 */

  /* Check free FAT entries (we don't purge deleted files) */
  if( spec->file_count >= 128 )
    return LIBSPECTRUM_DISK_UNSUP;

  /* Check free sectors */
  n_sec = ( size + ARRAY_SIZE( trailing_data ) + 255 ) / 256;
  if( spec->free_sectors < n_sec )
    return LIBSPECTRUM_DISK_UNSUP;

  /* Write file data */
  n_copied = 0;
  s = spec->first_free_sector;
  t = spec->first_free_track;
  libspectrum_disk_set_track( d, 0, t );

  for( i = 0; i < n_sec; i++ ) {
    memset( head, 0, 256 );
    n_bytes = 0;

    /* Copy chunk of file body */
    if( n_copied < size ) {
      n_bytes = ( size - n_copied > 256 )? 256 : size - n_copied;
      memcpy( head, data + n_copied, n_bytes );
      n_copied += n_bytes;
    }

    /* Copy trailing parameters */
    if( n_copied >= size ) {
      while( n_copied - size < ARRAY_SIZE( trailing_data ) && n_bytes < 256 ) {
        head[ n_bytes ] = trailing_data[ n_copied - size ];
        n_copied++;
        n_bytes++;
      }
    }

    if( !libspectrum_disk_id_seek( d, s, &slen ) || slen != 0x01 )
      return LIBSPECTRUM_DISK_UNSUP;
    /* Write buffer to disk */
    libspectrum_disk_data_add( d, head, 256, 0, -1, 0, NULL );

    /* Next sector */
    s = ( s + 1 ) % 16;

    /* Next track */
    if( s == 0 ) {
      t = t + 1;
      if( t >= d->cylinders ) return LIBSPECTRUM_DISK_UNSUP;
      libspectrum_disk_set_track( d, 0, t );
    }
  }

  /* Write FAT entry */
  memcpy( entry.filename, "boot    ", 8 );
  entry.file_extension = 'B';
  entry.param1         = size; /* assumes variables = 0 */
  entry.param2         = size;
  entry.file_length    = n_sec;
  entry.start_sector   = spec->first_free_sector;
  entry.start_track    = spec->first_free_track;

  /* Copy sector to buffer, modify and write back to disk recalculating CRCs */
  fat_sector = spec->file_count / 16;
  snum = 1;
  del = 0;
  if( !libspectrum_disk_read_sectors( d, 0, 0, fat_sector, &snum, &del,
                                 &buffer, &length, NULL, NULL ) ||
      snum != 1 || slen != 256 ) {
    if( buffer != NULL )
      libspectrum_free( buffer );
    return LIBSPECTRUM_DISK_UNSUP;
  }

  fat_entry  = spec->file_count % 16;
  trdos_write_dirent( buffer + fat_entry * 16, &entry );

  if( !libspectrum_disk_id_seek( d, fat_sector, &slen ) || slen != 0x01 )
    return LIBSPECTRUM_DISK_UNSUP;
  /* Write buffer to disk */
  libspectrum_disk_data_add( d, buffer, 256, 0, -1, 0, NULL );
  libspectrum_free( buffer );

  /* Write specification sector */
  spec->file_count       += 1;
  spec->free_sectors     -= n_sec;
  spec->first_free_sector = s;
  spec->first_free_track  = t;
  trdos_write_spec( head, spec );

  if( !libspectrum_disk_id_seek( d, 9, &slen ) || slen != 0x01 )
    return LIBSPECTRUM_DISK_UNSUP;
  /* Write buffer to disk */
  libspectrum_disk_data_add( d, head, 256, 0, -1, 0, NULL );

  return LIBSPECTRUM_DISK_OK;
}

static void
trdos_insert_boot_loader( libspectrum_disk_t *d )
{
  trdos_spec_t spec;
  trdos_boot_info_t info;
  int slen, del;
  int snum;
  libspectrum_byte *buffer;
  size_t length;

  /* TR-DOS specification sector */
  if( !libspectrum_disk_seek( d, 0, 0, 9, &slen, &del, NULL ) &&
      !del && slen != 2 )
    return;

  if( trdos_read_spec( &spec, d->track + d->i ) )
    return;

  /* Check free FAT entries (we don't purge deleted files) */
  if( spec.file_count >= 128 )
    return;

  /* Check there is at least one free sector */
  if( spec.free_sectors == 0 )
    return;
  /* TODO: stealth mode? some boot loaders hide between sectors 10-16 */

  /* Read sectors (8) with FAT entries */
  snum = 8;
  del = 0;
  if( libspectrum_disk_read_sectors( d, 0, 0, 1, &snum, &del, &buffer,
                                     &length, NULL, NULL ) ||
      snum != 8 || length < 8 * 256 ) {
    if( buffer != NULL )
      libspectrum_free( buffer );
    return;
  }

  if( trdos_read_fat( &info, buffer ) ) {
    libspectrum_free( buffer );
    return;
  }
  libspectrum_free( buffer );

  /* Check actual boot file (nothing to do) */
  if( info.have_boot_file )
    return;

  /* Insert a simple boot loader that runs the first program */
  if( info.basic_files_count >= 1 ) {
    memcpy( beta128_boot_loader + 22, info.first_basic_file, 8 );

    trdos_insert_basic_file( d, &spec, beta128_boot_loader,
                             ARRAY_SIZE( beta128_boot_loader ) );
  }

  /* TODO: use also a boot loader that can handle multiple basic pograms */
}

static int
disk_open2( libspectrum_disk_t *d, const char *filename, int preindex )
{
  utils_file file;
  int error;

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
    disk_position_context_t context;

    if( settings_current.auto_load ) {
      position_context_save( d, &context );
      trdos_insert_boot_loader( d );
      position_context_restore( d, &context );
    }
  }

  return d->status = error;
}

/*--------------------- other fuctions -----------------------*/

/* create a two sided disk (d) from two one sided (d1 and d2) */
int
disk_merge_sides( libspectrum_disk_t *d, libspectrum_disk_t *d1,
                  libspectrum_disk_t *d2, int autofill )
{
  return libspectrum_disk_merge_sides( d, d1, d2, autofill );
}

int
disk_open( libspectrum_disk_t *d, const char *filename, int preindex,
           int merge_disks )
{
  char *filename2;
  char c = ' ';
  int l, g = 0, pos = 0;
  libspectrum_disk_t d1, d2;

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

  if( disk_merge_sides( d, &d1, &d2, 0x00 ) ) {
    disk_close( &d2 );
    *d = d1;
  }
/*  fprintf( stderr, "`%s' and `%s' merged\n", filename, filename2 ); */
  libspectrum_free( filename2 );
  return d->status;
}

/*--------------------- start of write section ----------------*/

int
disk_write( libspectrum_disk_t *d, const char *filename )
{
  libspectrum_byte *buffer = NULL;
  size_t length;
  FILE *file;
  libspectrum_disk_t dw;

  if( ( file = fopen( filename, "wb" ) ) == NULL )
    return d->status = LIBSPECTRUM_DISK_WRFILE;

  dw = *d;
  libspectrum_disk_write( &dw, &buffer, &length, filename );
  if( d->status != LIBSPECTRUM_DISK_OK ) {
    if( buffer != NULL )
      libspectrum_free( buffer );
    fclose( file );
    return d->status;
  }
  if( fwrite( buffer, length, 1, file ) != 1 ) {
    if( buffer != NULL )
      libspectrum_free( buffer );
    fclose( file );
    return d->status = LIBSPECTRUM_DISK_WRFILE;
  }

  libspectrum_free( buffer );

  if( fclose( file ) == -1 )
    return d->status = LIBSPECTRUM_DISK_WRFILE;

  return d->status = LIBSPECTRUM_DISK_OK;
}
