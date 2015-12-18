/* scld.h: Routines for handling the Timex SCLD
   Copyright (c) 2002-2004 Fredrick Meunier, Witold Filipczyk

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

   E-mail: fredm@spamcop.net

*/

#ifndef FUSE_SCLD_H
#define FUSE_SCLD_H

#ifndef FUSE_MEMORY_H
#include "memory.h"
#endif				/* #ifndef FUSE_MEMORY_H */

#ifdef WORDS_BIGENDIAN

typedef struct
{
  unsigned altmembank : 1;  /* ALTMEMBANK : 0 = cartridge, 1 = exrom */
  unsigned intdisable : 1;  /* INTDISABLE */
  unsigned b5  : 1;  /* */
  unsigned b4  : 1;  /* */
  unsigned b3  : 1;  /* */
  unsigned hires  : 1;  /* SCLD HIRES mode */
  unsigned b1     : 1;  /* */
  unsigned altdfile : 1;  /* SCLD use ALTDFILE */
} scld_names;

typedef struct
{
  unsigned b7  : 1;  /* */
  unsigned b6  : 1;  /* */
  unsigned hirescol  : 3;  /* HIRESCOLMASK */
  unsigned scrnmode  : 3;  /* SCRNMODEMASK */
} scld_masks;

#else				/* #ifdef WORDS_BIGENDIAN */

typedef struct
{
  unsigned altdfile : 1;  /* SCLD use ALTDFILE */
  unsigned b1     : 1;  /* */
  unsigned hires  : 1;  /* SCLD HIRES mode */
  unsigned b3  : 1;  /* */
  unsigned b4  : 1;  /* */
  unsigned b5  : 1;  /* */
  unsigned intdisable : 1;  /* INTDISABLE */
  unsigned altmembank : 1;  /* ALTMEMBANK : 0 = cartridge, 1 = exrom */
} scld_names;

typedef struct
{
  unsigned scrnmode  : 3;  /* SCRNMODEMASK */
  unsigned hirescol  : 3;  /* HIRESCOLMASK */
  unsigned b6  : 1;  /* */
  unsigned b7  : 1;  /* */
} scld_masks;

#endif				/* #ifdef WORDS_BIGENDIAN */

typedef union
{
  libspectrum_byte byte;
  scld_masks mask;
  scld_names name;
} scld; 

extern scld scld_last_dec;           /* The last byte sent to Timex DEC port */

extern libspectrum_byte scld_last_hsr; /* Last byte sent to Timex HSR port */

/* Home map has pointers to the related entries in the RAM array so that the
   dck loading code can locate the associated pages when extracting data from
   its files */
extern memory_page * timex_home[MEMORY_PAGES_IN_64K];
extern memory_page timex_exrom[MEMORY_PAGES_IN_64K];
extern memory_page timex_dock[MEMORY_PAGES_IN_64K];

void scld_init( void );

void scld_dec_write( libspectrum_word port, libspectrum_byte b );
void scld_hsr_write( libspectrum_word port, libspectrum_byte b );

void scld_memory_map( void );
/* Initialise the memory map to point to the home bank */
void scld_memory_map_home( void );

void scld_home_map_16k( libspectrum_word address, memory_page source[],
                        int page_num );

/* Set contention for SCLD, contended in home, Dock and Exrom in the 0x4000 -
   0x7FFF range */
void scld_set_exrom_dock_contention( void );

#endif                  /* #ifndef FUSE_SCLD_H */
