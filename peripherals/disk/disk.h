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

/* open a disk image file. if preindex = 1 and the image file is not UDI,
   then a pre-index gap is generated with index mark (0xfc)
*/
libspectrum_disk_error_t
disk_open( libspectrum_disk *d, const char *filename, int preindex,
           int disk_merge );

/* write a disk image file (from the disk buffer). d->type determines the
   format. In the case of LIBSPECTRUM_DISK_TYPE_NONE, the format is guessed
   from the filename extension. The fallback format is UDI.
*/
libspectrum_disk_error_t
disk_write( libspectrum_disk *d, const char *filename );

#endif /* FUSE_DISK_H */
