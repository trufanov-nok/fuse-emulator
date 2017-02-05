/* disk.h: Routines for handling disk images
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

#ifndef FUSE_DISK_H
#define FUSE_DISK_H

typedef struct disk_position_context_t {
  libspectrum_byte *track;   /* current track data bytes */
  libspectrum_byte *clocks;  /* clock marks bits */
  libspectrum_byte *fm;      /* FM/MFM marks bits */
  libspectrum_byte *weak;    /* weak marks bits/weak data */
  int i;                     /* index for track and clocks */
} disk_position_context_t;

/* open a disk image file. if preindex = 1 and the image file not UDI then
   pre-index gap generated with index mark (0xfc)
   this time only .mgt(.dsk)/.img/.udi and CPC/extended CPC file format
   supported
*/
int disk_open( libspectrum_disk_t *d, const char *filename, int preindex,
               int disk_merge );

/* merge two one sided disk (d1, d2) to a two sided one (d),
   after merge closes d1 and d2
*/
int disk_merge_sides( libspectrum_disk_t *d, libspectrum_disk_t *d1,
                      libspectrum_disk_t *d2, int autofill );

/* write a disk image file (from the disk buffer). the d->type
   gives the format of file. if it DISK_TYPE_AUTO, disk_write
   try to guess from the file name (extension). if fail save as
   UDI.
*/
int disk_write( libspectrum_disk_t *d, const char *filename );

/* format disk to plus3 accept for formatting */
int disk_preformat( libspectrum_disk_t *d );

/* close a disk and free buffers */
void disk_close( libspectrum_disk_t *d );

#endif /* FUSE_DISK_H */
