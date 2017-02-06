/* trdos.c: Routines for handling the TR-DOS filesystem
   Copyright (c) 2016 Sergio Baldoví

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

   E-mail: serbalgi@gmail.com

*/

#include <config.h>

#include <string.h>

#include <libspectrum.h>

#include "compat.h"
#include "trdos.h"

typedef struct trdos_spec_t {
  libspectrum_byte first_free_sector; /* 0 to 15 */
  libspectrum_byte first_free_track;  /* 0 to ? */
  libspectrum_byte disk_type;
  libspectrum_byte file_count;
  libspectrum_word free_sectors;
  libspectrum_byte id;
  char password[9];                   /* not null-terminated */
  libspectrum_byte deleted_files;
  char disk_label[8];                 /* not null-terminated */
} trdos_spec_t;

typedef struct trdos_dirent_t {
  char filename[8];                   /* not null-terminated */
  char file_extension;
  libspectrum_word param1;
  libspectrum_word param2;
  libspectrum_byte file_length;       /* in sectors */
  libspectrum_byte start_sector;      /* 0 to 15 */
  libspectrum_byte start_track;       /* 0 to ? */
} trdos_dirent_t;

typedef struct trdos_boot_info_t {
  int have_boot_file;
  int basic_files_count;
  char first_basic_file[8];           /* not null-terminated */
} trdos_boot_info_t;

static int
trdos_read_spec( trdos_spec_t *spec, const libspectrum_byte *src )
{
  if( *src ) return -1;

  spec->first_free_sector   = src[225];
  spec->first_free_track    = src[226];
  spec->disk_type           = src[227];
  spec->file_count          = src[228];
  spec->free_sectors        = src[229] + src[230] * 0x100;
  spec->id                  = src[231];
  if( spec->id != 16 ) return -1;

  memcpy( spec->password, src + 234, 9 );
  spec->deleted_files       = src[244];
  memcpy( spec->disk_label, src + 245, 8 );

  return 0;
}

static void
trdos_write_spec( libspectrum_byte *dest, const trdos_spec_t *spec )
{
  memset( dest, 0, 256 );
  dest[225] = spec->first_free_sector;
  dest[226] = spec->first_free_track;
  dest[227] = spec->disk_type;
  dest[228] = spec->file_count;
  dest[229] = spec->free_sectors & 0xff;
  dest[230] = spec->free_sectors >> 8;
  dest[231] = spec->id;
  memcpy( dest + 234, spec->password, 9 );
  dest[244] = spec->deleted_files;
  memcpy( dest + 245, spec->disk_label, 8 );
}

static int
trdos_read_dirent( trdos_dirent_t *entry, const libspectrum_byte *src )
{
  memcpy( entry->filename, src, 8 );
  entry->file_extension = src[8];
  entry->param1         = src[9]  + src[10] * 0x100;
  entry->param2         = src[11] + src[12] * 0x100;
  entry->file_length    = src[13];
  entry->start_sector   = src[14];
  entry->start_track    = src[15];

  return entry->filename[0]? 0 : 1;
}

static void
trdos_write_dirent( libspectrum_byte *dest, const trdos_dirent_t *entry )
{
  memcpy( dest, entry->filename, 8 );
  dest[8]  = entry->file_extension;
  dest[9]  = entry->param1 & 0xff;
  dest[10] = entry->param1 >> 8;
  dest[11] = entry->param2 & 0xff;
  dest[12] = entry->param2 >> 8;
  dest[13] = entry->file_length;
  dest[14] = entry->start_sector;
  dest[15] = entry->start_track;
}

static int
trdos_read_fat( trdos_boot_info_t *info, const libspectrum_byte *data )
{
  int i, error;
  trdos_dirent_t entry;

  info->have_boot_file = 0;
  info->basic_files_count = 0;

  /* FAT sectors */
    /* FAT entries */
  for( i = 0; i < 8 * 16; i++ ) {
    error = trdos_read_dirent( &entry, data + i * 16 );
    if( error ) return 0;

    /* Basic files */
    if( entry.filename[0] > 0x01 &&
        entry.file_extension == 'B' ) {

      /* Boot file */
      if( !info->have_boot_file &&
          !strncmp( (const char *)entry.filename, "boot    ", 8 ) ) {
        info->have_boot_file = 1;
      }

      /* First basic program */
      if( info->basic_files_count == 0 ) {
        memcpy( info->first_basic_file, entry.filename, 8 );
      }

      info->basic_files_count++;
    }
  }

  return 0;
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

void
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
