/* ulaplus.h: Routines for handling the ULAplus
   Copyright (c) 2009 Gilberto Almeida
   Copyright (c) 2014 Fredrick Meunier, Segio Baldoví

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

#ifndef FUSE_ULAPLUS_H
#define FUSE_ULAPLUS_H

#define ULAPLUS_CLUT_MAX_COLOURS 64

extern int ulaplus_available;  /* Is ULAplus available for use? */
extern int ulaplus_palette_enabled;
extern libspectrum_byte ulaplus_palette[ ULAPLUS_CLUT_MAX_COLOURS ];

void ulaplus_register_startup( void );

/* Set the whole of the ULAplus palette */
void ulaplus_set_palette( libspectrum_byte *palette );

/* Get the R, G, B and grey components for a given ULAplus colour */
void ulaplus_parse_colour( int colour, int *red, int *green, int *blue,
                           int *grey );

#endif                  /* #ifndef FUSE_ULAPLUS_H */
