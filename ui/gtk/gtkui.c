/* gtkui.c: GTK+ routines for dealing with the user interface
   Copyright (c) 2000-2002 Philip Kendall, Russell Marks

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

#ifdef UI_GTK		/* Use this file iff we're using GTK+ */

#include <stdio.h>
#include <string.h>

#include <gdk/gdkkeysyms.h>
#include <gdk/gdkx.h>
#include <gtk/gtk.h>

#include "display.h"
#include "fuse.h"
#include "gtkkeyboard.h"
#include "gtkui.h"
#include "machine.h"
#include "options.h"
#include "rzx.h"
#include "settings.h"
#include "snapshot.h"
#include "spectrum.h"
#include "tape.h"
#include "timer.h"
#include "ui/ui.h"
#include "ui/uidisplay.h"
#include "widget/widget.h"

/* The main Fuse window */
GtkWidget *gtkui_window;

/* The area into which the screen will be drawn */
GtkWidget *gtkui_drawing_area;

/* Popup menu widget(s), as invoked by F1 */
GtkWidget *gtkui_menu_popup;

static gboolean gtkui_make_menu(GtkAccelGroup **accel_group,
				GtkWidget **menu_bar,
				GtkItemFactoryEntry *menu_data,
				guint menu_data_size);

static gint gtkui_delete(GtkWidget *widget, GdkEvent *event,
			      gpointer data);
static void gtkui_open(GtkWidget *widget, gpointer data);
static void gtkui_save(GtkWidget *widget, gpointer data);

static void gtkui_rzx_start( GtkWidget *widget, gpointer data );
static void gtkui_rzx_stop( GtkWidget *widget, gpointer data );
static void gtkui_rzx_play( GtkWidget *widget, gpointer data );

static void gtkui_quit(GtkWidget *widget, gpointer data);
static void gtkui_reset(GtkWidget *widget, gpointer data);

static void gtkui_select(GtkWidget *widget, gpointer data);
static void gtkui_select_done( GtkWidget *widget, gpointer user_data );

static void gtkui_tape_open( GtkWidget *widget, gpointer data );
static void gtkui_tape_play( GtkWidget *widget, gpointer data );
static void gtkui_tape_rewind( GtkWidget *widget, gpointer data );
static void gtkui_tape_clear( GtkWidget *widget, gpointer data );
static void gtkui_tape_write( GtkWidget *widget, gpointer data );

static void gtkui_help_keyboard( GtkWidget *widget, gpointer data );

static char* gtkui_fileselector_get_filename( const char *title );
static void gtkui_fileselector_done( GtkButton *button, gpointer user_data );
static void gtkui_fileselector_cancel( GtkButton *button, gpointer user_data );

static GtkItemFactoryEntry gtkui_menu_data[] = {
  { "/File",		        NULL , NULL,                0, "<Branch>"    },
  { "/File/_Open Snapshot..." , "F3" , gtkui_open,          0, NULL          },
  { "/File/_Save Snapshot..." , "F2" , gtkui_save,          0, NULL          },
  { "/File/separator",          NULL , NULL,                0, "<Separator>" },
  { "/File/_Recording",		NULL , NULL,		    0, "<Branch>"    },
  { "/File/Recording/_Record...",NULL, gtkui_rzx_start,     0, NULL	     },
  { "/File/Recording/_Play...", NULL , gtkui_rzx_play,	    0, NULL          },
  { "/File/Recording/_Stop",    NULL , gtkui_rzx_stop,	    0, NULL          },
  { "/File/separator",          NULL , NULL,                0, "<Separator>" },
  { "/File/E_xit",	        "F10", gtkui_quit,          0, NULL          },
  { "/Options",		        NULL , NULL,                0, "<Branch>"    },
  { "/Options/_General...",     "F4" , gtkoptions_general,  0, NULL          },
  { "/Options/_Sound...",	NULL , gtkoptions_sound,    0, NULL          },
  { "/Machine",		        NULL , NULL,                0, "<Branch>"    },
  { "/Machine/_Reset",	        "F5" , gtkui_reset,         0, NULL          },
  { "/Machine/_Select...",      "F9" , gtkui_select,        0, NULL          },
  { "/Tape",                    NULL , NULL,                0, "<Branch>"    },
  { "/Tape/_Open...",	        "F7" , gtkui_tape_open,     0, NULL          },
  { "/Tape/_Play",	        "F8" , gtkui_tape_play,     0, NULL          },
  { "/Tape/_Rewind",		NULL , gtkui_tape_rewind,   0, NULL          },
  { "/Tape/_Clear",		NULL , gtkui_tape_clear,    0, NULL          },
  { "/Tape/_Write...",		"F6" , gtkui_tape_write,    0, NULL          },
  { "/Help",			NULL , NULL,		    0, "<Branch>"    },
  { "/Help/_Keyboard...",	NULL , gtkui_help_keyboard, 0, NULL	     },
};
static guint gtkui_menu_data_size =
  sizeof( gtkui_menu_data ) / sizeof( GtkItemFactoryEntry );
  
int ui_init(int *argc, char ***argv, int width, int height)
{
  GtkWidget *box,*menu_bar;
  GtkAccelGroup *accel_group;
  GdkGeometry geometry;

  gtk_init(argc,argv);

  gtkui_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  gtk_window_set_title( GTK_WINDOW(gtkui_window), "Fuse" );
  gtk_window_set_wmclass( GTK_WINDOW(gtkui_window), fuse_progname, "Fuse" );
  gtk_window_set_default_size( GTK_WINDOW(gtkui_window), width, height);
  gtk_signal_connect(GTK_OBJECT(gtkui_window), "delete_event",
		     GTK_SIGNAL_FUNC(gtkui_delete), NULL);
  gtk_signal_connect(GTK_OBJECT(gtkui_window), "key-press-event",
		     GTK_SIGNAL_FUNC(gtkkeyboard_keypress), NULL);
  gtk_widget_add_events( gtkui_window, GDK_KEY_RELEASE_MASK );
  gtk_signal_connect(GTK_OBJECT(gtkui_window), "key-release-event",
		     GTK_SIGNAL_FUNC(gtkkeyboard_keyrelease), NULL);

  /* If we lose the focus, disable all keys */
  gtk_signal_connect( GTK_OBJECT( gtkui_window ), "focus-out-event",
		      GTK_SIGNAL_FUNC( gtkkeyboard_release_all ), NULL );

  box = gtk_vbox_new( FALSE, 0 );
  gtk_container_add(GTK_CONTAINER(gtkui_window), box);
  gtk_widget_show(box);

  gtkui_make_menu(&accel_group, &menu_bar, gtkui_menu_data,
		  gtkui_menu_data_size);

  gtk_window_add_accel_group( GTK_WINDOW(gtkui_window), accel_group );
  gtk_box_pack_start( GTK_BOX(box), menu_bar, FALSE, FALSE, 0 );
  gtk_widget_show(menu_bar);
  
  gtkui_drawing_area = gtk_drawing_area_new();
  if(!gtkui_drawing_area) {
    fprintf(stderr,"%s: couldn't create drawing area at %s:%d\n",
	    fuse_progname,__FILE__,__LINE__);
    return 1;
  }
  gtk_drawing_area_size( GTK_DRAWING_AREA(gtkui_drawing_area),
			 width, height );
  gtk_box_pack_start( GTK_BOX(box), gtkui_drawing_area, FALSE, FALSE, 0 );

  geometry.min_width = width;
  geometry.min_height = height;
  geometry.max_width = 3*width;
  geometry.max_height = 3*height;
  geometry.base_width = 0;
  geometry.base_height = 0;
  geometry.width_inc = width;
  geometry.height_inc = height;
  geometry.min_aspect = geometry.max_aspect = ((float)width)/height;

  gtk_window_set_geometry_hints( GTK_WINDOW(gtkui_window), gtkui_drawing_area,
				 &geometry,
				 GDK_HINT_MIN_SIZE | GDK_HINT_MAX_SIZE |
				 GDK_HINT_BASE_SIZE | GDK_HINT_RESIZE_INC |
				 GDK_HINT_ASPECT );


  gtk_widget_show(gtkui_drawing_area);

  if(uidisplay_init(width,height)) return 1;

  gtk_widget_show(gtkui_window);

  return 0;
}

static gboolean gtkui_make_menu(GtkAccelGroup **accel_group,
				GtkWidget **menu_bar,
				GtkItemFactoryEntry *menu_data,
				guint menu_data_size)
{
  GtkItemFactory *item_factory;

  *accel_group = gtk_accel_group_new();
  item_factory = gtk_item_factory_new( GTK_TYPE_MENU_BAR, "<main>",
				       *accel_group );
  gtk_item_factory_create_items(item_factory, menu_data_size, menu_data, NULL);
  *menu_bar = gtk_item_factory_get_widget( item_factory, "<main>" );

  /* We have to recreate the menus for the popup, unfortunately... */
  item_factory = gtk_item_factory_new( GTK_TYPE_MENU, "<main>", NULL );
  gtk_item_factory_create_items(item_factory, menu_data_size, menu_data, NULL);
  gtkui_menu_popup = gtk_item_factory_get_widget( item_factory, "<main>" );

  return FALSE;
}

static void gtkui_popup_menu_pos( GtkMenu *menu, gint *xp, gint *yp,
				  GtkWidget *data)
{
  gdk_window_get_position( gtkui_window->window, xp, yp );
}

/* Popup main menu, as invoked by F1. */
void gtkui_popup_menu(void)
{
  gtk_menu_popup( GTK_MENU(gtkui_menu_popup), NULL, NULL,
		  (GtkMenuPositionFunc)gtkui_popup_menu_pos, NULL,
		  0, 0 );
}

int ui_event(void)
{
  while(gtk_events_pending())
    gtk_main_iteration();
  return 0;
}

int ui_end(void)
{
  int error;
  
  /* Don't display the window whilst doing all this! */
  gtk_widget_hide(gtkui_window);

  /* Tidy up the low-level stuff */
  error = uidisplay_end(); if(error) return error;

  /* Now free up the window itself */
/*    XDestroyWindow(display,mainWindow); */

  /* And disconnect from the X server */
/*    XCloseDisplay(display); */

  return 0;
}

/* The callbacks used by various routines */

/* Called by the main window on a "delete_event" */
static gint gtkui_delete(GtkWidget *widget, GdkEvent *event,
			 gpointer data)
{
  fuse_exiting=1;
  return TRUE;
}

/* Called by the menu when File/Open selected */
static void gtkui_open(GtkWidget *widget, gpointer data)
{
  char *filename;

  fuse_emulation_pause();

  filename = gtkui_fileselector_get_filename( "Fuse - Load Snapshot" );
  if( !filename ) { fuse_emulation_unpause(); return; }

  snapshot_read( filename );

  free( filename );

  display_refresh_all();

  fuse_emulation_unpause();
}

/* Called by the menu when File/Save selected */
static void gtkui_save(GtkWidget *widget, gpointer data)
{
  char *filename;

  fuse_emulation_pause();

  filename = gtkui_fileselector_get_filename( "Fuse - Save Snapshot" );
  if( !filename ) { fuse_emulation_unpause(); return; }

  snapshot_write( filename );

  free( filename );

  fuse_emulation_unpause();
}

/* Called when File/Recording/Start selected */
static void gtkui_rzx_start( GtkWidget *widget, gpointer data )
{
  char *filename;

  if( rzx_playback || rzx_recording ) return;

  fuse_emulation_pause();
  
  filename = gtkui_fileselector_get_filename( "Fuse - Start Recording" );
  if( !filename ) { fuse_emulation_unpause(); return; }

  rzx_start_recording( filename );

  free( filename );

  fuse_emulation_unpause();
}

/* Called when File/Recording/Stop selected */
static void gtkui_rzx_stop( GtkWidget *widget, gpointer data )
{
  if( rzx_recording ) rzx_stop_recording();
  if( rzx_playback  ) rzx_stop_playback();
}

/* Called when File/Recording/Play selected */
static void gtkui_rzx_play( GtkWidget *widget, gpointer data )
{
  char *filename;

  if( rzx_playback || rzx_recording ) return;

  fuse_emulation_pause();
  
  filename = gtkui_fileselector_get_filename( "Fuse - Play Recording" );
  if( !filename ) { fuse_emulation_unpause(); return; }

  rzx_start_playback( filename );

  free( filename );

  fuse_emulation_unpause();
}

/* Called by the menu when File/Exit selected */
static void gtkui_quit(GtkWidget *widget, gpointer data)
{
  fuse_exiting=1;
}

/* Called by the menu when Machine/Reset selected */
static void gtkui_reset(GtkWidget *widget, gpointer data)
{
  machine_current->reset();
}

typedef struct gtkui_select_info {

  GtkWidget *dialog;
  GtkWidget **buttons;

} gtkui_select_info;

/* Called by the menu when Machine/Select selected */
static void gtkui_select(GtkWidget *widget, gpointer data)
{
  gtkui_select_info dialog;
  GSList *button_group;

  GtkWidget *ok_button, *cancel_button;

  int i;
  
  /* Some space to store the radio buttons in */
  dialog.buttons = (GtkWidget**)malloc( machine_count * sizeof(GtkWidget* ) );
  if( dialog.buttons == NULL ) {
    ui_error( "out of memory at %s:%d\n", __FILE__, __LINE__ );
    return;
  }

  /* Stop emulation */
  fuse_emulation_pause();

  /* Create the necessary widgets */
  dialog.dialog = gtk_dialog_new();
  gtk_window_set_title( GTK_WINDOW( dialog.dialog ), "Fuse - Select Machine" );

  dialog.buttons[0] =
    gtk_radio_button_new_with_label( NULL, machine_types[0]->description );
  button_group =
    gtk_radio_button_group( GTK_RADIO_BUTTON( dialog.buttons[0] ) );

  for( i=1; i<machine_count; i++ ) {
    dialog.buttons[i] =
      gtk_radio_button_new_with_label(
        gtk_radio_button_group (GTK_RADIO_BUTTON (dialog.buttons[i-1] ) ),
	machine_types[i]->description
      );
  }

  for( i=0; i<machine_count; i++ ) {
    gtk_toggle_button_set_active( GTK_TOGGLE_BUTTON( dialog.buttons[i] ),
				  machine_current == machine_types[i] );
    gtk_container_add( GTK_CONTAINER( GTK_DIALOG( dialog.dialog )->vbox ),
		       dialog.buttons[i] );
  }

  /* Create and add the actions buttons to the dialog box */
  ok_button = gtk_button_new_with_label( "OK" );
  cancel_button = gtk_button_new_with_label( "Cancel" );

  gtk_container_add( GTK_CONTAINER( GTK_DIALOG( dialog.dialog )->action_area ),
		     ok_button );
  gtk_container_add( GTK_CONTAINER( GTK_DIALOG( dialog.dialog )->action_area ),
		     cancel_button );

  /* Add the necessary callbacks */
  gtk_signal_connect( GTK_OBJECT( ok_button ), "clicked",
		      GTK_SIGNAL_FUNC( gtkui_select_done ),
		      (gpointer) &dialog );
  gtk_signal_connect_object( GTK_OBJECT( cancel_button ), "clicked",
			     GTK_SIGNAL_FUNC( gtkui_destroy_widget_and_quit ),
			     GTK_OBJECT( dialog.dialog ) );
  gtk_signal_connect( GTK_OBJECT( dialog.dialog ), "delete_event",
		      GTK_SIGNAL_FUNC( gtkui_destroy_widget_and_quit ),
		      (gpointer) NULL );

  /* Allow Esc to cancel */
  gtk_widget_add_accelerator( cancel_button, "clicked",
                              gtk_accel_group_get_default(),
                              GDK_Escape, 0, 0 );

  /* Set the window to be modal and display it */
  gtk_window_set_modal( GTK_WINDOW( dialog.dialog ), TRUE );
  gtk_widget_show_all( dialog.dialog );

  /* Process events until the window is done with */
  gtk_main();

  /* And then carry on with emulation again */
  fuse_emulation_unpause();
}

/* Callback used by the machine selection dialog */
static void gtkui_select_done( GtkWidget *widget, gpointer user_data )
{
  int i;
  gtkui_select_info *ptr = (gtkui_select_info*)user_data;

  for( i=0; i<machine_count; i++ ) {
    if( gtk_toggle_button_get_active( GTK_TOGGLE_BUTTON(ptr->buttons[i]) ) &&
        machine_current != machine_types[i]
      )
    {
      machine_select( machine_types[i]->machine );
    }
  }

  gtk_widget_destroy( ptr->dialog );
  gtk_main_quit();
}
    
/* Called by the menu when Tape/Open selected */
static void gtkui_tape_open( GtkWidget *widget, gpointer data )
{
  char *filename;

  fuse_emulation_pause();

  filename = gtkui_fileselector_get_filename( "Fuse - Open Tape" );
  if( !filename ) { fuse_emulation_unpause(); return; }

  tape_open( filename );

  free( filename );

  fuse_emulation_unpause();
}

/* Called by the menu when Tape/Play selected */
static void gtkui_tape_play( GtkWidget *widget, gpointer data )
{
  tape_toggle_play();
}

/* Called by the menu when Tape/Rewind selected */
static void gtkui_tape_rewind( GtkWidget *widget, gpointer data )
{
  tape_rewind();
}

/* Called by the menu when Tape/Clear selected */
static void gtkui_tape_clear( GtkWidget *widget, gpointer data )
{
  tape_close();
}

/* Called by the menu when Tape/Write selected */
static void gtkui_tape_write( GtkWidget *widget, gpointer data )
{
  char *filename;

  fuse_emulation_pause();

  filename = gtkui_fileselector_get_filename( "Fuse - Write Tape" );
  if( !filename ) { fuse_emulation_unpause(); return; }

  tape_write( filename );

  free( filename );

  fuse_emulation_unpause();
}

static void gtkui_help_keyboard( GtkWidget *widget, gpointer data )
{
  widget_picture_data picture_data = { "keyboard.scr", NULL, 0 };

  widget_menu_keyboard( &picture_data );
}

/* Generic `tidy-up' callback */
void gtkui_destroy_widget_and_quit( GtkWidget *widget, gpointer data )
{
  gtk_widget_destroy( widget );
  gtk_main_quit();
}

/* Bits used for the file selection dialogs */

typedef struct gktui_fileselector_info {

  GtkWidget *selector;
  gchar *filename;

} gtkui_fileselector_info;

static char* gtkui_fileselector_get_filename( const char *title )
{
  gtkui_fileselector_info selector;

  selector.selector = gtk_file_selection_new( title );
  selector.filename = NULL;

  gtk_signal_connect(
      GTK_OBJECT( GTK_FILE_SELECTION( selector.selector )->ok_button ),
      "clicked",
      GTK_SIGNAL_FUNC( gtkui_fileselector_done ),
      (gpointer) &selector );

  gtk_signal_connect(
       GTK_OBJECT( GTK_FILE_SELECTION( selector.selector )->cancel_button),
       "clicked",
       GTK_SIGNAL_FUNC(gtkui_fileselector_cancel ),
       (gpointer) &selector );

  gtk_signal_connect( GTK_OBJECT( selector.selector ), "delete_event",
		      GTK_SIGNAL_FUNC( gtkui_destroy_widget_and_quit ),
		      (gpointer) &selector );

  /* Allow Esc to cancel */
  gtk_widget_add_accelerator(
       GTK_FILE_SELECTION( selector.selector )->cancel_button,
       "clicked",
       gtk_accel_group_get_default(),
       GDK_Escape, 0, 0 );

  gtk_window_set_modal( GTK_WINDOW( selector.selector ), TRUE );

  gtk_widget_show( selector.selector );

  gtk_main();

  return selector.filename;
}

static void gtkui_fileselector_done( GtkButton *button, gpointer user_data )
{
  gtkui_fileselector_info *ptr = (gtkui_fileselector_info*) user_data;
  char *filename;

  filename =
    gtk_file_selection_get_filename( GTK_FILE_SELECTION( ptr->selector ) );

  /* FIXME: what to do if this runs out of memory? */
  ptr->filename = strdup( filename );

  gtk_widget_destroy( ptr->selector );

  gtk_main_quit();
}

static void gtkui_fileselector_cancel( GtkButton *button, gpointer user_data )
{
  gtkui_fileselector_info *ptr = (gtkui_fileselector_info*) user_data;

  gtk_widget_destroy( ptr->selector );

  gtk_main_quit();
}

#endif			/* #ifdef UI_GTK */
