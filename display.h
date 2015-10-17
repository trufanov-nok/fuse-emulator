/* display.h: Routines for printing the Spectrum's screen
   Copyright (c) 1999-2015 Philip Kendall

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

#ifndef FUSE_DISPLAY_H
#define FUSE_DISPLAY_H

#include <stddef.h>

#include <libspectrum.h>

/* The width and height of the Speccy's screen */
#define DISPLAY_WIDTH_COLS  32
#define DISPLAY_HEIGHT_ROWS 24

/* The width and height of the Speccy's screen */
/* Each main screen column can produce 16 pixels in hires mode */
#define DISPLAY_WIDTH         ( DISPLAY_WIDTH_COLS * 16 )
/* Each main screen row can produce only 8 pixels in any mode */
#define DISPLAY_HEIGHT        ( DISPLAY_HEIGHT_ROWS * 8 )

/* The width and height of the (emulated) border */
#define DISPLAY_BORDER_WIDTH_COLS  4
#define DISPLAY_BORDER_HEIGHT_COLS 3

/* The width and height of the (emulated) border */
/* Each main screen column can produce 16 pixels in hires mode */
#define DISPLAY_BORDER_WIDTH  ( DISPLAY_BORDER_WIDTH_COLS * 16 )
/* Aspect corrected border width */
#define DISPLAY_BORDER_ASPECT_WIDTH  ( DISPLAY_BORDER_WIDTH_COLS * 8 )
/* Each main screen row can produce only 8 pixels in any mode */
#define DISPLAY_BORDER_HEIGHT ( DISPLAY_BORDER_HEIGHT_COLS * 8 )

/* The width and height of the window we'll be displaying */
#define DISPLAY_SCREEN_WIDTH  ( DISPLAY_WIDTH  + 2 * DISPLAY_BORDER_WIDTH  )
#define DISPLAY_SCREEN_HEIGHT ( DISPLAY_HEIGHT + 2 * DISPLAY_BORDER_HEIGHT )

/* And the width in columns */
#define DISPLAY_SCREEN_WIDTH_COLS ( DISPLAY_WIDTH_COLS + 2 * DISPLAY_BORDER_WIDTH_COLS )

/* The aspect ratio corrected display width */
#define DISPLAY_ASPECT_WIDTH  ( DISPLAY_SCREEN_WIDTH / 2 )

extern int display_ui_initialised;

/* The various display modes of the emulated machines */
typedef enum display_chunk_type {
  LOWRES_TWO_COLOUR,	 /* Low-res chunk with two colours */
  LOWRES_SIXTEEN_COLOUR, /* Low-res chunk with 16 colours */
  HIRES_TWO_COLOUR,	 /* Hi-res chunk with two colours */
  DIRTY,	         /* Guaranteed to be dirty regardless of next scan */
} display_chunk_type;

typedef struct
{
  unsigned ink   : 8; /* colour index in ULAplus palette */
  unsigned paper : 8; /* colour index in ULAplus palette */
  unsigned data  : 8; /* 8 pixels packed into a byte */
} low_res_2_col_type; 

typedef struct
{
  unsigned data4 : 8; /* 2 16 colour pixels packed as nibbles in a byte */
  unsigned data3 : 8; /* 2 16 colour pixels packed as nibbles in a byte */
  unsigned data2 : 8; /* 2 16 colour pixels packed as nibbles in a byte */
  unsigned data1 : 8; /* 2 16 colour pixels packed as nibbles in a byte */
} low_res_16_col_type; 

typedef struct
{
  unsigned ink   : 8; /* colour index in ULAplus palette */
  unsigned paper : 8; /* colour index in ULAplus palette */
  unsigned data2 : 8; /* 8 pixels packed into a byte */
  unsigned data  : 8; /* 8 pixels packed into a byte */
} hi_res_2_col_type; 

typedef union
{
  libspectrum_dword dword;
  low_res_2_col_type lr_2c; 
  low_res_16_col_type lr_16c; 
  hi_res_2_col_type hr_2c; 
} display_chunk_data; 

typedef struct
{
  display_chunk_type type;
  display_chunk_data data;
} display_chunk; 

extern display_chunk
display_last_screen[ DISPLAY_SCREEN_WIDTH_COLS * DISPLAY_SCREEN_HEIGHT ];

/* Offsets as to where the data and the attributes for each pixel
   line start */
extern libspectrum_word display_line_start[ DISPLAY_HEIGHT ];
extern libspectrum_word display_attr_start[ DISPLAY_HEIGHT ];

int display_init(int *argc, char ***argv);
void display_line(void);

typedef void (*display_dirty_fn)( libspectrum_word offset );
/* Function to use to mark as 'dirty' the pixels which have been changed by a
   write to 'offset' within the RAM page containing the screen */
extern display_dirty_fn display_dirty;
void display_dirty_timex( libspectrum_word offset );
void display_dirty_pentagon_16_col( libspectrum_word offset );
void display_dirty_sinclair( libspectrum_word offset );

typedef void (*display_write_if_dirty_fn)( int x, int y );
/* Function to write a dirty 8x1 chunk of pixels to the display */
extern display_write_if_dirty_fn display_write_if_dirty;
void display_write_if_dirty_timex( int x, int y );
void display_write_if_dirty_pentagon_16_col( int x, int y );
void display_write_if_dirty_sinclair( int x, int y );

typedef void (*display_dirty_flashing_fn)(void);
/* Function to dirty the pixels which are changed by virtue of having a flash
   attribute */
extern display_dirty_flashing_fn display_dirty_flashing;
void display_dirty_flashing_timex(void);
void display_dirty_flashing_pentagon_16_col(void);
void display_dirty_flashing_sinclair(void);

void display_parse_attr( libspectrum_byte attr, libspectrum_byte *ink,
			 libspectrum_byte *paper );

void display_set_lores_border( libspectrum_byte spectrum_colour );
void display_set_hires_border( libspectrum_byte spectrum_colour );
int display_dirty_border(void);

int display_frame(void);
void display_refresh_main_screen(void);
void display_refresh_all(void);

#define display_get_addr( x, y ) \
  display_mode.name.altdfile ? display_line_start[(y)]+(x)+ALTDFILE_OFFSET : \
  display_line_start[(y)]+(x)
int display_getpixel( int x, int y );

void display_update_critical( int x, int y );

/* Timex-style hi-colour and hi-res handling */
#define STANDARD        0x00 /* standard Spectrum */
#define ALTDFILE        0x01 /* the same in nature as above, but using second
                                display file */
#define EXTCOLOUR       0x02 /* extended colours (data taken from first screen,
                                attributes 1x8 taken from second display. */
#define EXTCOLALTD      0x03 /* similar to above, but data is taken from second
                                screen */
#define HIRESATTR       0x04 /* hires mode, data in odd columns is taken from
                                first screen in standard way, data in even
                                columns is made from attributes data (8x8) */
#define HIRESATTRALTD   0x05 /* similar to above, but data taken from second
                                display */
#define HIRES           0x06 /* true hires mode, odd columns from first screen,
                                even columns from second screen.  columns
                                numbered from 1. */
#define HIRESDOUBLECOL  0x07 /* data taken only from second screen, columns are
                                doubled */
#define HIRESCOLMASK    0x38

#define WHITEBLACK      0x00
#define YELLOWBLUE      0x01
#define CYANRED         0x02
#define GREENMAGENTA    0x03
#define MAGENTAGREEN    0x04
#define REDCYAN         0x05
#define BLUEYELLOW      0x06
#define BLACKWHITE      0x07

#define ALTDFILE_OFFSET 0x2000

#ifdef WORDS_BIGENDIAN

typedef struct
{
  unsigned b7       : 1;  /* */
  unsigned b6       : 1;  /* */
  unsigned b5       : 1;  /* */
  unsigned b4       : 1;  /* */
  unsigned b3       : 1;  /* */
  unsigned hires    : 1;  /* Timex-style HIRES mode */
  unsigned b1       : 1;  /* */
  unsigned altdfile : 1;  /* Use Timex-style alternate ALTDFILE */
} display_flag_names;

typedef struct
{
  unsigned b7       : 1;  /* */
  unsigned b6       : 1;  /* */
  unsigned hirescol : 3;  /* HIRESCOLMASK */
  unsigned scrnmode : 3;  /* SCRNMODEMASK */
} display_flag_masks;

#else				/* #ifdef WORDS_BIGENDIAN */

typedef struct
{
  unsigned altdfile : 1;  /* Use Timex-style alternate ALTDFILE */
  unsigned b1       : 1;  /* */
  unsigned hires    : 1;  /* Timex-style HIRES mode */
  unsigned b3       : 1;  /* */
  unsigned b4       : 1;  /* */
  unsigned b5       : 1;  /* */
  unsigned b6       : 1;  /* */
  unsigned b7       : 1;  /* */
} display_flag_names;

typedef struct
{
  unsigned scrnmode : 3;  /* SCRNMODEMASK */
  unsigned hirescol : 3;  /* HIRESCOLMASK */
  unsigned b6       : 1;  /* */
  unsigned b7       : 1;  /* */
} display_flag_masks;

#endif				/* #ifdef WORDS_BIGENDIAN */

typedef union
{
  libspectrum_byte byte;
  display_flag_masks mask;
  display_flag_names name;
} display_flag; 

extern display_flag display_mode;    /* The current display mode */

extern libspectrum_byte hires_get_attr( void );

extern libspectrum_byte hires_convert_display_flag( libspectrum_byte attr );

extern void display_videomode_update( display_flag new_display_mode );

/* The various display hardware mode types of the emulated machines */
typedef enum display_hardware_mode_type {
  SINCLAIR,	 /* Low-res with two colours only */
  TIMEX,         /* Low-res with 2 colours or hi-res with 2 colours */
  PENTAGON1024,  /* Low-res with 16 colours */
} display_hardware_mode_type;

extern void display_set_mode( const display_hardware_mode_type mode );

#endif			/* #ifndef FUSE_DISPLAY_H */
