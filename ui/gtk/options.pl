#!/usr/bin/perl -w

# options.pl: generate options dialog boxes
# $Id$

# Copyright (c) 2002-2013 Philip Kendall

# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License along
# with this program; if not, write to the Free Software Foundation, Inc.,
# 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

# Author contact information:

# E-mail: philip-fuse@shadowmagic.org.uk

use strict;

use Fuse;
use Fuse::Dialog;

die "No data file specified" unless @ARGV;

my @dialogs = Fuse::Dialog::read( shift @ARGV );

my %combo_sets;
my %combo_default;

print Fuse::GPL( 'options.c: options dialog boxes',
                 '2001-2013 Philip Kendall' ) . << "CODE";

/* This file is autogenerated from options.dat by options.pl.
   Do not edit unless you know what you\'re doing! */

#include <config.h>

#ifdef UI_GTK                /* Use this file if we're using GTK+ */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gtk/gtk.h>

#include "compat.h"
#include "display.h"
#include "fuse.h"
#include "options.h"
#include "options_internals.h"
#include "periph.h"
#include "settings.h"
#include "ui/gtk/gtkcompat.h"
#include "ui/gtk/gtkinternals.h"
#include "utils.h"

static int
option_enumerate_combo( const char * const *options, char *value, guint count,
                        int def )
{
  guint i;
  if( value != NULL ) {
    for( i = 0; i < count; i++) {
      if( !strcmp( value, options[ i ] ) )
        return i;
    }
  }
  return def;
}

CODE

foreach( @dialogs ) {

    foreach my $widget ( @{ $_->{widgets} } ) {

	foreach my $type ( $widget->{type} ) {

	    my $text = $widget->{text}; $text =~ tr/()//d;

	    if( $type eq "Combo" ) {
		my $n = 0;

		foreach( split( /\|/, $widget->{data1} ) ) {
		    if( /^\*/ ) {
			$combo_default{$widget->{value}} = $n;
		    }
		    $n++;
		}
		$n = 0;
		$widget->{data1} =~ s/^\*//;
		$widget->{data1} =~ s/\|\*/|/;
		if( not exists( $combo_sets{$widget->{data1}} ) ) {
		    $combo_sets{$widget->{data1}} = "$_->{name}_$widget->{value}_combo";

		    print << "CODE";

static const char * const $_->{name}_$widget->{value}_combo[] = {
CODE
		    foreach( split( /\|/, $widget->{data1} ) ) {
			print << "CODE";
  "$_",
CODE
			$n++;
		    }
		    print << "CODE";
};

static const guint $_->{name}_$widget->{value}_combo_count = $n;

CODE
		} else {
		    print << "CODE";
\#define $_->{name}_$widget->{value}_combo $combo_sets{$widget->{data1}}
\#define $_->{name}_$widget->{value}_combo_count $combo_sets{$widget->{data1}}_count

CODE
		}
		print << "CODE";
int
option_enumerate_$_->{name}_$widget->{value}( void )
{
  return option_enumerate_combo( $_->{name}_$widget->{value}_combo,
                                 settings_current.$widget->{value},
                                 $_->{name}_$widget->{value}_combo_count,
                                 $combo_default{$widget->{value}} );
}

CODE
	    }
	}
    }

    print << "CODE";
static void menu_options_$_->{name}_done( GtkWidget *widget,
					  gpointer user_data );

void
menu_options_$_->{name}( GtkWidget *widget GCC_UNUSED,
			 gpointer data GCC_UNUSED )
{
  GtkWidget *content_area;
  menu_options_$_->{name}_t dialog;

  /* Firstly, stop emulation */
  fuse_emulation_pause();

  /* Create the necessary widgets */
  dialog.dialog = gtkstock_dialog_new( "Fuse - $_->{title}", NULL );
  content_area = gtk_dialog_get_content_area( GTK_DIALOG( dialog.dialog ) );

  /* Create the various widgets */
CODE

    foreach my $widget ( @{ $_->{widgets} } ) {

	foreach my $type ( $widget->{type} ) {

	    my $text = $widget->{text}; $text =~ tr/()//d;

	    if( $type eq "Checkbox" ) {

		print << "CODE";
  dialog.$widget->{value} =
    gtk_check_button_new_with_label( "$text" );
  gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( dialog.$widget->{value} ),
                                settings_current.$widget->{value} );
  gtk_container_add( GTK_CONTAINER( content_area ), dialog.$widget->{value} );

CODE
            } elsif( $type eq "Entry" ) {

                # FIXME: Make the entry widget resize sensibly

		print << "CODE";
  {
    GtkWidget *frame = gtk_frame_new( "$text" );
    GtkWidget *hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    GtkWidget *text = gtk_label_new( "$widget->{data2}" );
    gchar buffer[80];

    gtk_box_pack_start( GTK_BOX( content_area ), frame, TRUE, TRUE, 0 );

    gtk_container_set_border_width( GTK_CONTAINER( hbox ), 4 );
    gtk_container_add( GTK_CONTAINER( frame ), hbox );

    dialog.$widget->{value} = gtk_entry_new();
    gtk_entry_set_max_length( GTK_ENTRY( dialog.$widget->{value} ),
                              $widget->{data1} );
    snprintf( buffer, 80, "%d", settings_current.$widget->{value} );
    gtk_entry_set_text( GTK_ENTRY( dialog.$widget->{value} ), buffer );

    gtk_box_pack_start( GTK_BOX( hbox ), dialog.$widget->{value}, TRUE, TRUE, 0 );

    gtk_box_pack_start( GTK_BOX( hbox ), text, FALSE, FALSE, 5 );
  }

CODE
            } elsif( $type eq "Combo" ) {

		print << "CODE";
  {
    GtkWidget *hbox = gtk_box_new( GTK_ORIENTATION_HORIZONTAL, 0 );
    GtkWidget *combo = gtk_combo_box_text_new();
    GtkWidget *text = gtk_label_new( "$text" );
    guint i;

    gtk_box_pack_start( GTK_BOX( hbox ), text, FALSE, FALSE, 5 );
    text = gtk_label_new( " " );
    gtk_box_pack_start( GTK_BOX( hbox ), text, TRUE, FALSE, 5 );

    for( i = 0; i < $_->{name}_$widget->{value}_combo_count; i++ ) {
      gtk_combo_box_text_append_text( GTK_COMBO_BOX_TEXT( combo ), $_->{name}_$widget->{value}_combo[i] );
    }
    gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), $combo_default{$widget->{value}} );
    if( settings_current.$widget->{value} != NULL ) {
      for( i = 0; i < $_->{name}_$widget->{value}_combo_count; i++ ) {
        if( !strcmp( settings_current.$widget->{value}, $_->{name}_$widget->{value}_combo[i] ) ) {
          gtk_combo_box_set_active( GTK_COMBO_BOX( combo ), i );
        }
      }
    }

    dialog.$widget->{value} = combo;
    gtk_box_pack_start( GTK_BOX( hbox ), dialog.$widget->{value}, FALSE, FALSE, 5 );

    gtk_box_pack_start( GTK_BOX( content_area ), hbox, TRUE, TRUE, 0 );
  }

CODE
	    } else {
		die "Unknown type `$type'";
	    }
	}
    }

    print << "CODE";
  /* Create the OK and Cancel buttons */
  gtkstock_create_ok_cancel( dialog.dialog, NULL,
                             G_CALLBACK( menu_options_$_->{name}_done ),
                             (gpointer) &dialog, NULL, DEFAULT_DESTROY );

  /* Display the window */
  gtk_widget_show_all( dialog.dialog );

  /* Process events until the window is done with */
  gtk_main();

  /* And then carry on with emulation again */
  fuse_emulation_unpause();
}

static void
menu_options_$_->{name}_done( GtkWidget *widget GCC_UNUSED,
			      gpointer user_data )
{
  menu_options_$_->{name}_t *ptr = user_data;

CODE

    if( $_->{postcheck} ) {

      print << "CODE";
  /* Get a copy of current settings */
  settings_info original_settings;
  memset( &original_settings, 0, sizeof( settings_info ) );
  settings_copy( &original_settings, &settings_current );

CODE

    }

    foreach my $widget ( @{ $_->{widgets} } ) {

	if( $widget->{type} eq "Checkbox" ) {

	    print << "CODE";
  settings_current.$widget->{value} =
    gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON( ptr->$widget->{value} ) );

CODE
        } elsif( $widget->{type} eq "Entry" ) {

	    print << "CODE";
  settings_current.$widget->{value} =
    atoi( gtk_entry_get_text( GTK_ENTRY( ptr->$widget->{value} ) ) );

CODE
        } elsif( $widget->{type} eq "Combo" ) {

	    print << "CODE";
  libspectrum_free( settings_current.$widget->{value} );
  settings_current.$widget->{value} = utils_safe_strdup( $_->{name}_$widget->{value}_combo[
    gtk_combo_box_get_active( GTK_COMBO_BOX( ptr->$widget->{value} ) ) ] );

CODE
    	} else {
	    die "Unknown type `$widget->{type}'";
	}
    }

    if( $_->{postcheck} ) {

      print << "CODE";
  int needs_hard_reset = $_->{postcheck}();

  /* Confirm reset */
  if( needs_hard_reset && !gtkui_confirm("Some options need to reset the machine. Reset?" ) ) {

    /* Cancel new settings */
    settings_copy( &settings_current, &original_settings );
    settings_free( &original_settings );
    return;
  }

  settings_free( &original_settings );

CODE

    }

    print << "CODE";
  gtk_widget_destroy( ptr->dialog );

CODE

    print "  $_->{posthook}();\n\n" if $_->{posthook};

    print << "CODE";
  gtkstatusbar_set_visibility( settings_current.statusbar );
  display_refresh_all();

  gtk_main_quit();
}

CODE

}

print << "CODE"

#endif			/* #ifdef UI_GTK */
CODE
