/* help.c: Help menu
   Copyright (c) 2001,2002 Philip Kendall

   $Id$

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

   Author contact information:

   E-mail: pak21-fuse@srcf.ucam.org
   Postal address: 15 Crescent Road, Wokingham, Berks, RG40 2DB, England

*/

#include <config.h>

#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>

#include "display.h"
#include "fuse.h"
#include "keyboard.h"
#include "ui/uidisplay.h"
#include "utils.h"
#include "widget.h"

#define ERROR_MESSAGE_MAX_LENGTH 1024

static int widget_help_keyboard( const char *filename );

int widget_help_draw( void* data )
{
  /* Draw the dialog box */
  widget_dialog_with_border( 1, 2, 30, 3 );

  widget_printstring( 11, 2, WIDGET_COLOUR_FOREGROUND, "Help" );
  widget_printstring( 2, 4, WIDGET_COLOUR_FOREGROUND,
		      "(K)eyboard picture..." );

  uidisplay_lines( DISPLAY_BORDER_HEIGHT + 16,
		   DISPLAY_BORDER_HEIGHT + 16 + 24 );

  return 0;
}

void widget_help_keyhandler( int key )
{
  switch( key ) {
    
  case KEYBOARD_1: /* 1 used as `Escape' generates `Edit', which is Caps + 1 */
    widget_return[ widget_level ].finished = WIDGET_FINISHED_CANCEL;
    break;

  case KEYBOARD_k:
    widget_help_keyboard( "keyboard.scr" );
    break;

  case KEYBOARD_Enter:
    widget_return[ widget_level ].finished = WIDGET_FINISHED_OK;
    break;

  }
}

static int widget_help_keyboard( const char *filename )
{
  int error, fd;
  BYTE *screen; size_t length;

  char error_message[ ERROR_MESSAGE_MAX_LENGTH ];

  fd = utils_find_lib( filename );
  if( fd == -1 ) {
    fprintf( stderr, "%s: couldn't find keyboard picture (`%s')\n",
	     fuse_progname, filename );
    return 1;
  }
  
  error = utils_read_fd( fd, filename, &screen, &length );
  if( error ) return error;

  if( length != 6912 ) {
    fprintf( stderr, "%s: keyboard picture (`%s') is not 6912 bytes long\n",
	     fuse_progname, filename );
    return 1;
  }

  widget_do( WIDGET_TYPE_PICTURE, screen );

  if( munmap( screen, length ) == -1 ) {
    snprintf( error_message, ERROR_MESSAGE_MAX_LENGTH,
	      "%s: Couldn't munmap keyboard picture (`%s')",
	      fuse_progname, filename );
    perror( error_message );
    return 1;
  }

  return 0;
}
