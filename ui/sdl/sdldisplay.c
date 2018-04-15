/* sdldisplay.c: Routines for dealing with the SDL display
   Copyright (c) 2000-2006 Philip Kendall, Matan Ziv-Av, Fredrick Meunier
   Copyright (c) 2015 Adrien Destugues

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

#include <config.h>

#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <SDL.h>

#ifdef SDL_USE_GL
#include <GL/gl.h>
#endif

#include <libspectrum.h>

#include "display.h"
#include "fuse.h"
#include "machine.h"
#include "peripherals/scld.h"
#include "screenshot.h"
#include "settings.h"
#include "ui/ui.h"
#include "ui/scaler/scaler.h"
#include "ui/uidisplay.h"
#include "utils.h"

SDL_Surface *sdldisplay_gc = NULL;   /* Hardware screen */
static SDL_Surface *tmp_screen=NULL; /* Temporary screen for scalers */

static SDL_Surface *red_cassette[2], *green_cassette[2];
static SDL_Surface *red_mdr[2], *green_mdr[2];
static SDL_Surface *red_disk[2], *green_disk[2];

static ui_statusbar_state sdl_disk_state, sdl_mdr_state, sdl_tape_state;
static int sdl_status_updated;

static int tmp_screen_width = 0;

static Uint32 colour_values[16];

static SDL_Color colour_palette[] = {
  {   0,   0,   0,   0 }, 
  {   0,   0, 192,   0 }, 
  { 192,   0,   0,   0 }, 
  { 192,   0, 192,   0 }, 
  {   0, 192,   0,   0 }, 
  {   0, 192, 192,   0 }, 
  { 192, 192,   0,   0 }, 
  { 192, 192, 192,   0 }, 
  {   0,   0,   0,   0 }, 
  {   0,   0, 255,   0 }, 
  { 255,   0,   0,   0 }, 
  { 255,   0, 255,   0 }, 
  {   0, 255,   0,   0 }, 
  {   0, 255, 255,   0 }, 
  { 255, 255,   0,   0 }, 
  { 255, 255, 255,   0 }
};

static Uint32 bw_values[16];

/* This is a rule of thumb for the maximum number of rects that can be updated
   each frame. If more are generated we just update the whole screen */
#define MAX_UPDATE_RECT 300
static SDL_Rect updated_rects[MAX_UPDATE_RECT];
static int num_rects = 0;
static libspectrum_byte sdldisplay_force_full_refresh = 1;

static int max_fullscreen_height;
static int min_fullscreen_height;
static int fullscreen_width = 0;
static int fullscreen_x_off = 0;
static int fullscreen_y_off = 0;

/* The current size of the display (in units of DISPLAY_SCREEN_*) */
static float sdldisplay_current_size = 1;

static libspectrum_byte sdldisplay_is_full_screen = 0;

static int image_width;
static int image_height;

static int timex;

#ifdef SDL_USE_GL
static int gl_usable = -1;
static int gl_scaler = -1;
static Uint32 gl_flag;
static float gl_tw, gl_th;   /* texture used width/height */
static float gl_vw, gl_vh;   /* vertex width/height */
static int gl_ttw, gl_tth;   /* GLTexture Total width height (align to 2^x) */
static int gl_width, gl_height;   /* current size of SDL window */
static GLuint gl_tex;
static GLenum gl_error = GL_NO_ERROR;
static int screen_width = -1, screen_height = -1;
#ifdef USE_HW_SCALER
static int hwscaler_usable = 0;
#endif
#endif  /* #ifdef SDL_USE_GL */

static void init_scalers( void );
static int sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                                        Uint32 *bw_values );

static int sdldisplay_load_gfx_mode( void );

static void
init_scalers( void )
{
  scaler_register_clear();

  scaler_register( SCALER_NORMAL );
  scaler_register( SCALER_DOUBLESIZE );
  scaler_register( SCALER_TRIPLESIZE );
  scaler_register( SCALER_2XSAI );
  scaler_register( SCALER_SUPER2XSAI );
  scaler_register( SCALER_SUPEREAGLE );
  scaler_register( SCALER_ADVMAME2X );
  scaler_register( SCALER_ADVMAME3X );
  scaler_register( SCALER_DOTMATRIX );
  scaler_register( SCALER_PALTV );
  scaler_register( SCALER_HQ2X );
#ifdef USE_HW_SCALER
  if( hwscaler_usable )
    scaler_register( SCALER_HW );
#endif  /* USE_HW_SCALER */
  if( machine_current->timex ) {
    scaler_register( SCALER_HALF ); 
    scaler_register( SCALER_HALFSKIP );
    scaler_register( SCALER_TIMEXTV );
    scaler_register( SCALER_TIMEX1_5X );
  } else {
    scaler_register( SCALER_TV2X );
    scaler_register( SCALER_TV3X );
    scaler_register( SCALER_PALTV2X );
    scaler_register( SCALER_PALTV3X );
    scaler_register( SCALER_HQ3X );
  }
  
  if( scaler_is_supported( current_scaler ) ) {
    scaler_select_scaler( current_scaler );
  } else {
    scaler_select_scaler( SCALER_NORMAL );
  }
}

static int
sdl_convert_icon( SDL_Surface *source, SDL_Surface **icon, int red )
{
  SDL_Surface *copy;   /* Copy with altered palette */
  int i;

  SDL_Color colors[ source->format->palette->ncolors ];

  copy = SDL_ConvertSurface( source, source->format, SDL_SWSURFACE );

  for( i = 0; i < copy->format->palette->ncolors; i++ ) {
    colors[i].r = red ? copy->format->palette->colors[i].r : 0;
    colors[i].g = red ? 0 : copy->format->palette->colors[i].g;
    colors[i].b = 0;
  }

  SDL_SetPalette( copy, SDL_LOGPAL, colors, 0, i );

  icon[0] = SDL_ConvertSurface( copy, tmp_screen->format, SDL_SWSURFACE );

  SDL_FreeSurface( copy );

  icon[1] = SDL_CreateRGBSurface( SDL_SWSURFACE,
                                  (icon[0]->w)<<1, (icon[0]->h)<<1,
                                  icon[0]->format->BitsPerPixel,
                                  icon[0]->format->Rmask,
                                  icon[0]->format->Gmask,
                                  icon[0]->format->Bmask,
                                  icon[0]->format->Amask
                                );

  ( scaler_get_proc16( SCALER_DOUBLESIZE ) )(
        (libspectrum_byte*)icon[0]->pixels,
        icon[0]->pitch,
        (libspectrum_byte*)icon[1]->pixels,
        icon[1]->pitch, icon[0]->w, icon[0]->h
      );

  return 0;
}

static int
sdl_load_status_icon( const char*filename, SDL_Surface **red, SDL_Surface **green )
{
  char path[ PATH_MAX ];
  SDL_Surface *temp;    /* Copy of image as loaded */

  if( utils_find_file_path( filename, path, UTILS_AUXILIARY_LIB ) ) {
    fprintf( stderr, "%s: Error getting path for icons\n", fuse_progname );
    return -1;
  }

  if((temp = SDL_LoadBMP(path)) == NULL) {
    fprintf( stderr, "%s: Error loading icon \"%s\" text:%s\n", fuse_progname,
             path, SDL_GetError() );
    return -1;
  }

  if(temp->format->palette == NULL) {
    fprintf( stderr, "%s: Icon \"%s\" is not paletted\n", fuse_progname, path );
    return -1;
  }

  sdl_convert_icon( temp, red, 1 );
  sdl_convert_icon( temp, green, 0 );

  SDL_FreeSurface( temp );

  return 0;
}

int
uidisplay_init( int width, int height )
{
  SDL_Rect **modes;
  int no_modes;
  int i, mw = 0, mh = 0, mn = 0;

#ifdef SDL_USE_GL
  const GLubyte *gl_ven = NULL, *gl_ren = NULL, *gl_ver = NULL;
  GLint gl_tsize;
  const SDL_VideoInfo* info;

  /* get current screen height and width */
  info = SDL_GetVideoInfo();   /* calls SDL_GetVideoInfo() */
  screen_width = info->current_w;
  screen_height = info->current_h;

#ifdef USE_HW_SCALER
  /* Check resizable GL video mode available */
  modes = SDL_ListModes( NULL, SDL_OPENGL | SDL_RESIZABLE );
  /* -1 if any dimension is okay for the given format. -> we need this */
  hwscaler_usable = ( modes == (SDL_Rect **) -1 ) ? 1 : 0;
#endif

  /* Check GL available */
  modes = SDL_ListModes( NULL, SDL_OPENGL | SDL_FULLSCREEN );
  gl_usable = ( modes == (SDL_Rect **) 0 || modes == (SDL_Rect **) -1 ) ? 0 : 1;
  if( gl_usable ) {
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 16 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    /* try to set up a 320x240 GL video mode */
    if( SDL_SetVideoMode( 320, 240, 15, SDL_OPENGL ) == 0 ) {
      gl_usable = 0;
    } else {
      /* get essential parameters to check we can use OpenGL */
      gl_ven = glGetString( GL_VENDOR );
      gl_ren = glGetString( GL_RENDERER );
      gl_ver = glGetString( GL_VERSION );
      glGetIntegerv( GL_MAX_TEXTURE_SIZE, &gl_tsize );
      gl_usable = gl_tsize >= 1024 ? 1 : 0;
    }
  }

#ifdef USE_HW_SCALER
  if( !gl_usable )
    hwscaler_usable = 0;
#endif
#endif  /* #ifdef SDL_USE_GL */

  /* Get available fullscreen/software modes */
  modes=SDL_ListModes(NULL, SDL_FULLSCREEN|SDL_SWSURFACE);

  no_modes = ( modes == (SDL_Rect **) 0 || modes == (SDL_Rect **) -1 ) ? 1 : 0;

  if( settings_current.sdl_fullscreen_mode &&
      strcmp( settings_current.sdl_fullscreen_mode, "list" ) == 0 ) {

    fprintf( stderr,
    "=====================================================================\n"
    " List of available SDL fullscreen modes:\n"
    "---------------------------------------------------------------------\n" );

#ifdef SDL_USE_GL
    /* if we have GL video mode, we print some parameters */
    if( gl_usable ) {
      fprintf( stderr,
        " OpenGL informations:\n  Vendor: %s\n  Renderer: %s\n"
        "  Version: %s\n"
        "  Max texture size: %ux%u pixel\n  OpenGL full screen scaler is supported.\n",
        gl_ven, gl_ren, gl_ver, gl_tsize, gl_tsize );
#ifdef USE_HW_SCALER
      if( hwscaler_usable ) {
        fprintf( stderr, " Hardware scaler is supported (OpenGL backend)\n" );
      }
#endif
      fprintf( stderr,
        "---------------------------------------------------------------------\n"
      );
    }
#endif  /* #ifdef SDL_USE_GL */

    fprintf( stderr,
      "  No. width height\n"
      "---------------------------------------------------------------------\n"
      );
    if( no_modes ) {
      fprintf( stderr, "  ** The modes list is empty%s...\n",
                       no_modes == 2 ? ", all resolution allowed" : "" );
    } else {
      for( i = 0; modes[i]; i++ ) {
        fprintf( stderr, "% 3d  % 5d % 5d\n", i + 1, modes[i]->w, modes[i]->h );
      }
    }
    fprintf( stderr,
    "=====================================================================\n");
    fuse_exiting = 1;
    return 0;
  }

  for( i=0; modes[i]; ++i ); /* count modes */
  if( settings_current.sdl_fullscreen_mode ) {
    if( sscanf( settings_current.sdl_fullscreen_mode, " %dx%d", &mw, &mh ) != 2 ) {
      if( sscanf( settings_current.sdl_fullscreen_mode, " %d", &mn ) == 1 && mn <= i ) {
        mw = modes[mn - 1]->w; mh = modes[mn - 1]->h;
      }
    }
  }

  /* Check if there are any modes available, or if our resolution is restricted
     at all */
  if( no_modes ){
    /* Just try whatever we have and see what happens */
    max_fullscreen_height = 480;
    min_fullscreen_height = 240;
  } else if( mh > 0 ) {
    /* set from command line */
    max_fullscreen_height = min_fullscreen_height = mh;
    fullscreen_width = mw;
  } else {
    /* Record the largest supported fullscreen software mode */
    max_fullscreen_height = modes[0]->h;

    /* Record the smallest supported fullscreen software mode */
    for( i=0; modes[i]; ++i ) {
      min_fullscreen_height = modes[i]->h;
    }
  }

  image_width = width;
  image_height = height;

  timex = machine_current->timex;

  init_scalers();

  if ( scaler_select_scaler( current_scaler ) )
    scaler_select_scaler( SCALER_NORMAL );

  if( sdldisplay_load_gfx_mode() ) return 1;

  SDL_WM_SetCaption( "Fuse", "Fuse" );

  /* We can now output error messages to our output device */
  display_ui_initialised = 1;

  sdl_load_status_icon( "cassette.bmp", red_cassette, green_cassette );
  sdl_load_status_icon( "microdrive.bmp", red_mdr, green_mdr );
  sdl_load_status_icon( "plus3disk.bmp", red_disk, green_disk );

  return 0;
}

static int
sdldisplay_allocate_colours( int numColours, Uint32 *colour_values,
                             Uint32 *bw_values )
{
  int i;
  Uint8 red, green, blue, grey;

  for( i = 0; i < numColours; i++ ) {

      red = colour_palette[i].r;
    green = colour_palette[i].g;
     blue = colour_palette[i].b;

    /* Addition of 0.5 is to avoid rounding errors */
    grey = ( 0.299 * red + 0.587 * green + 0.114 * blue ) + 0.5;

    colour_values[i] = SDL_MapRGB( tmp_screen->format,  red, green, blue );
    bw_values[i]     = SDL_MapRGB( tmp_screen->format, grey,  grey, grey );
  }

  return 0;
}

/*
fixed <-> fullscreen x <-> y
hardware <-> fixed x <-> y
hardware <-> fullscreen gl / normal <-> normal
*/
static void
sdldisplay_find_best_fullscreen_scaler( void )
{
  static int windowed_scaler = -1;
#ifdef USE_HW_SCALER
  static int last_w, last_h;
#endif
  static int searching_fullscreen_scaler = 0;

  /* Make sure we have at least more than half of the screen covered in
     fullscreen to avoid the "postage stamp" on machines that don't support
     320x240 anymore e.g. Mac notebooks */
  if( settings_current.full_screen ) {
    int i = 0;

    if( searching_fullscreen_scaler ) return;
    searching_fullscreen_scaler = 1;

#ifdef SDL_USE_GL
    if( gl_scaler > 1 ) {
      /* with OpenGL we always "use" (not really use) the Normal scaler */
      if( windowed_scaler == -1) {
        windowed_scaler = current_scaler;
#ifdef USE_HW_SCALER
        last_w = sdldisplay_gc->w; last_h = sdldisplay_gc->h;
#endif
      }
      return;
    }
#endif  /* #ifdef SDL_USE_GL */

    while( i < SCALER_NUM &&
           ( image_height*sdldisplay_current_size <= min_fullscreen_height/2 ||
             image_height*sdldisplay_current_size > max_fullscreen_height ) ) {
      if( windowed_scaler == -1) windowed_scaler = current_scaler;
      while( !scaler_is_supported(i) ) i++;
      scaler_select_scaler( i++ );
      sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
      /* if we failed to find a suitable size scaler, just use normal (what the
         user had originally may be too big) */
      if( image_height * sdldisplay_current_size <= min_fullscreen_height/2 ||
          image_height * sdldisplay_current_size > max_fullscreen_height ) {
        scaler_select_scaler( SCALER_NORMAL );
        sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
      }
    }
    searching_fullscreen_scaler = 0;
  } else {
    if( windowed_scaler != -1 ) {

#ifdef USE_HW_SCALER
      if( windowed_scaler == SCALER_HW ) {
        gl_width = last_w; gl_height = last_h;
      }
#endif  /* #ifdef USE_HW_SCALER */

      scaler_select_scaler( windowed_scaler );
      windowed_scaler = -1;
      sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );
    }
  }
}

#ifdef SDL_USE_GL
/* If we want to debug OpenGL
void MessageCallback( GLenum source,
                      GLenum type,
                      GLuint id,
                      GLenum severity,
                      GLsizei length,
                      const GLchar* message,
                      const void* userParam )
{
  fprintf( stderr, "GL CALLBACK: %s type = 0x%x, severity = 0x%x, message = %s\n",
           ( type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "" ),
            type, severity, message );
}
*/

#if defined SDL_USE_GL || defined USE_HW_SCALER
void
sdldisplay_resize( int w, int h )
{
  if( settings_current.aspect_hint ) {
    gl_vw = w > h * 4 / 3 ? h * 4.0 / 3 / w : 1.0;
    gl_vh = h > w * 3 / 4 ? w * 3.0 / 4 / h : 1.0;
  } else {
    gl_vw = 1.0; gl_vh = 1.0;
  }

  /* we have to resize the SDL window after a window resize event!!! (?) */
  if( sdldisplay_gc->w != w || sdldisplay_gc->h != h )
    sdldisplay_gc = SDL_SetVideoMode( w, h, 16, gl_flag );

  glViewport( 0, 0, w, h ); /* setup viewport */
}
#endif  /* #if defined SDL_USE_GL || defined USE_HW_SCALER */

static int
create_gl_texture( void )
{
  /* we create a 2^n dimension texture to maximal compatibility with older HW */
  gl_ttw = timex ? 1024 : 512;
  gl_tth = 256;

  glGenTextures( 1, &gl_tex );
  glBindTexture( GL_TEXTURE_2D, gl_tex );

  /* we do not use mipmaps */
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_BASE_LEVEL, 0);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);

  /* create the texture */
  glTexImage2D( GL_TEXTURE_2D, 0, GL_RGB, gl_ttw, gl_tth, 0, GL_RGB,
                GL_UNSIGNED_BYTE, NULL );

  /* for scaling, we use this texture to strech to a rectangle which has a
     proper size */
  if( ( gl_error = glGetError() ) != GL_NO_ERROR ) {
    ui_error( UI_ERROR_ERROR, "Cannot create OpenGL texture. Error code: %d",
              gl_error );
    return 0;
  }

  return 1;
}
#endif  /* #ifdef SDL_USE_GL */

static int
sdldisplay_load_gfx_mode( void )
{
  Uint16 *tmp_screen_pixels;
  int   tmp_screen_height;

  static int load_gfx = 0;

  if( load_gfx ) return 0;

  load_gfx = 1;

  sdldisplay_force_full_refresh = 1;

  /* Free the old surface */
  if( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen );
    tmp_screen = NULL;
  }

#ifdef SDL_USE_GL
  /* 0 -> none; 1 -> hwscaler; 2,3 -> GL fullscreen */
  gl_scaler = 0 +
#ifdef USE_HW_SCALER
              ( hwscaler_usable && current_scaler == SCALER_HW ) +
#endif  /* #ifdef USE_HW_SCALER */
              2 * ( gl_usable && settings_current.full_screen );
#endif  /* #ifdef SDL_USE_GL */

#ifdef SDL_USE_GL
  if( gl_scaler ) {
    /* for fullscreen, we use the screen dimensions, but for hwscaler we use
       the current window size */
    gl_width = ( gl_scaler > 1 ? screen_width : sdldisplay_gc->w );
    gl_height = ( gl_scaler > 1 ? screen_height : sdldisplay_gc->h );
  }
#endif  /* #ifdef SDL_USE_GL */

  sdldisplay_current_size = scaler_get_scaling_factor( current_scaler );

  sdldisplay_find_best_fullscreen_scaler();

  /* Create the surface that contains the scaled graphics in 16 bit mode */
#ifdef SDL_USE_GL
  if( gl_scaler ) {
    gl_flag = SDL_OPENGL + ( gl_scaler > 1 ? SDL_FULLSCREEN : 0 ) +
        ( gl_scaler == 1 ? SDL_RESIZABLE : 0 );
    SDL_GL_SetAttribute( SDL_GL_RED_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_GREEN_SIZE, 6 );
    SDL_GL_SetAttribute( SDL_GL_BLUE_SIZE, 5 );
    SDL_GL_SetAttribute( SDL_GL_DEPTH_SIZE, 0 );
    SDL_GL_SetAttribute( SDL_GL_DOUBLEBUFFER, 1 );
    sdldisplay_gc = SDL_SetVideoMode( gl_width, gl_height,
      16, gl_flag
    );
    /* restore video mode to normal... */
    if( !sdldisplay_gc  ) {
      sdldisplay_gc = SDL_SetVideoMode( gl_width, gl_height, 16, 0 );
      fuse_abort();
    }
    if( !create_gl_texture() ) {
      gl_usable = 0; /* disable OpenGL scaler */
#ifdef USE_HW_SCALER
      hwscaler_usable = 0; /* disable HW scaler */
#endif
      gl_scaler = 0;
    } else {
      /* we use 4 byte aligned buffer */
      tmp_screen_width = image_width + 4;
      tmp_screen_height = image_height + 4;
    }
  }

  /* if cannot create GL texture, we switch back to normal fullscreen scaler */
  if( !gl_scaler ) {
#endif  /* #ifdef SDL_USE_GL */

  tmp_screen_width = image_width + 3;
  tmp_screen_height = image_height + 3;

  sdldisplay_gc = SDL_SetVideoMode(
    settings_current.full_screen && fullscreen_width ? fullscreen_width :
      image_width * sdldisplay_current_size,
    settings_current.full_screen && fullscreen_width ? max_fullscreen_height :
      image_height * sdldisplay_current_size,
    16,
    settings_current.full_screen ? (SDL_FULLSCREEN|SDL_SWSURFACE)
                                 : SDL_SWSURFACE
  );

#ifdef SDL_USE_GL
  }  /* if( !gl_scaler ) */
#endif

  if( !sdldisplay_gc ) {
    fprintf( stderr, "%s: couldn't create SDL graphics context\n", fuse_progname );
    fuse_abort();
  }

  settings_current.full_screen =
      !!( sdldisplay_gc->flags & ( SDL_FULLSCREEN | SDL_NOFRAME ) );
  sdldisplay_is_full_screen = settings_current.full_screen;

#ifdef SDL_USE_GL
  /* for openGL we always use 565, BTW: OpenGL fb novadays 8888 */
  if( gl_scaler ) {
    scaler_select_bitformat( 565 );
  } else {
#endif
  /* Distinguish 555 and 565 mode */
  if( sdldisplay_gc->format->Gmask >> sdldisplay_gc->format->Gshift == 0x1f )
    scaler_select_bitformat( 555 );
  else
    scaler_select_bitformat( 565 );
#ifdef SDL_USE_GL
  }
#endif

  /* Create the surface used for the graphics in 16 bit before scaling */

  /* Need some extra bytes around when using 2xSaI */
  tmp_screen_pixels = (Uint16*)calloc(tmp_screen_width*tmp_screen_height, sizeof(Uint16));
#ifdef SDL_USE_GL
  if( gl_scaler ) {
    /* We create an RGB565 surface.
       WARNING: we have no BE/LE (scaler) code for RGB565/BGR565 */
    tmp_screen = SDL_CreateRGBSurfaceFrom( tmp_screen_pixels,
                                           tmp_screen_width,
                                           tmp_screen_height,
                                           16, tmp_screen_width * 2,
                                           0xf800, 0x07e0, 0x001f, 0x0 );
  } else {
#endif  /* #ifdef SDL_USE_GL */
  tmp_screen = SDL_CreateRGBSurfaceFrom(tmp_screen_pixels,
                                        tmp_screen_width,
                                        tmp_screen_height,
                                        16, tmp_screen_width*2,
                                        sdldisplay_gc->format->Rmask,
                                        sdldisplay_gc->format->Gmask,
                                        sdldisplay_gc->format->Bmask,
                                        sdldisplay_gc->format->Amask );
#ifdef SDL_USE_GL
  }
#endif

  if( !tmp_screen ) {
    fprintf( stderr, "%s: couldn't create tmp_screen\n", fuse_progname );
    fuse_abort();
  }

  fullscreen_x_off = ( sdldisplay_gc->w - image_width * sdldisplay_current_size ) *
                     sdldisplay_is_full_screen  / 2;
  fullscreen_y_off = ( sdldisplay_gc->h - image_height * sdldisplay_current_size ) *
                     sdldisplay_is_full_screen / 2;

  sdldisplay_allocate_colours( 16, colour_values, bw_values );

#ifdef SDL_USE_GL
  if( gl_scaler ) {
    sdldisplay_resize( gl_width, gl_height );

    /* now set up the transformation */
    glMatrixMode( GL_PROJECTION );
    glLoadIdentity();
    glOrtho( -1.0, 1.0, -1.0, 1.0, -1.0, 1.0 );
    glMatrixMode( GL_MODELVIEW );
    glLoadIdentity();

    /* "set up" tmp_screen pitch */
    glPixelStorei( GL_UNPACK_ROW_LENGTH, tmp_screen->pitch / 2 );
    glBindTexture( GL_TEXTURE_2D, gl_tex );

    /* magnifying and minifying filters */
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
                     settings_current.opengl_filter_nearest ?
                       GL_NEAREST : GL_LINEAR );
    glTexParameteri( GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
                     settings_current.opengl_filter_nearest ?
                       GL_NEAREST : GL_LINEAR );

    /* calculate the "real" texture size (we use a 2^n with/height texture for
       maximal compability */
    gl_tw = (double)image_width  / gl_ttw;
    gl_th = (double)image_height / gl_tth;

    /* simple replace the texture if update_rects */
    glTexEnvf( GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE );
    glClearColor( 0.0, 0.0, 0.0, 0.0 );
  }
#endif  /* #ifdef SDL_USE_GL */

  /* Redraw the entire screen... */
  display_refresh_all();

  load_gfx = 0;

  return 0;
}

int
uidisplay_hotswap_gfx_mode( void )
{
  fuse_emulation_pause();

  /* Free the old surface */
  if( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }

  /* Setup the new GFX mode */
  if( sdldisplay_load_gfx_mode() ) return 1;

  /* reset palette */
  SDL_SetColors( sdldisplay_gc, colour_palette, 0, 16 );

  /* Mac OS X resets the state of the cursor after a switch to full screen
     mode */
  if ( settings_current.full_screen || ui_mouse_grabbed ) {
    SDL_ShowCursor( SDL_DISABLE );
    SDL_WarpMouse( 128, 128 );
  } else {
    SDL_ShowCursor( SDL_ENABLE );
  }

  fuse_emulation_unpause();

  return 0;
}

SDL_Surface *saved = NULL;

void
uidisplay_frame_save( void )
{
  if( saved ) {
    SDL_FreeSurface( saved );
    saved = NULL;
  }

  saved = SDL_ConvertSurface( tmp_screen, tmp_screen->format,
                              SDL_SWSURFACE );
}

void
uidisplay_frame_restore( void )
{
  if( saved ) {
    SDL_BlitSurface( saved, NULL, tmp_screen, NULL );
    sdldisplay_force_full_refresh = 1;
  }
}

static void
sdl_blit_icon( SDL_Surface **icon,
               SDL_Rect *r, Uint32 tmp_screen_pitch,
               Uint32 dstPitch )
{
  int x, y, w, h, dst_x, dst_y, dst_h;

  if( timex ) {
    r->x<<=1;
    r->y<<=1;
    r->w<<=1;
    r->h<<=1;
  }

  x = r->x;
  y = r->y;
  w = r->w;
  h = r->h;
  r->x++;
  r->y++;

  if( SDL_BlitSurface( icon[timex], NULL, tmp_screen, r ) ) return;

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &w, &h, image_width, image_height );

  dst_y = y * sdldisplay_current_size + fullscreen_y_off;
  dst_h = h;
  dst_x = x * sdldisplay_current_size + fullscreen_x_off;

#ifdef SDL_USE_GL
/* with OpenGL we just draw over the texture with pixels of the icon */
  if( gl_scaler ) {
    glTexSubImage2D( GL_TEXTURE_2D, 0, r->x, r->y, r->w, r->h,
                     GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                     tmp_screen->pixels + ( r->y + 1 ) * tmp_screen_pitch
                                        + 2 * ( r->x + 1 ) );
  } else {
#endif  /* #ifdef SDL_USE_GL */
  scaler_proc16(
	(libspectrum_byte*)tmp_screen->pixels +
			(x+1) * tmp_screen->format->BytesPerPixel +
	                (y+1) * tmp_screen_pitch,
	tmp_screen_pitch,
	(libspectrum_byte*)sdldisplay_gc->pixels +
			dst_x * sdldisplay_gc->format->BytesPerPixel +
			dst_y * dstPitch,
	dstPitch, w, dst_h
  );
#ifdef SDL_USE_GL
  }
#endif

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Adjust rects for the destination rect size */
  updated_rects[num_rects].x = dst_x;
  updated_rects[num_rects].y = dst_y;
  updated_rects[num_rects].w = w * sdldisplay_current_size;
  updated_rects[num_rects].h = dst_h * sdldisplay_current_size;

  num_rects++;
}

static void
sdl_icon_overlay( Uint32 tmp_screen_pitch, Uint32 dstPitch )
{
  SDL_Rect r = { 243, 218, red_disk[0]->w, red_disk[0]->h };

  switch( sdl_disk_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_disk, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }

  r.x = 264;
  r.y = 218;
  r.w = red_mdr[0]->w;
  r.h = red_mdr[0]->h;

  switch( sdl_mdr_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_mdr, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
    sdl_blit_icon( red_mdr, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    break;
  }

  r.x = 285;
  r.y = 220;
  r.w = red_cassette[0]->w;
  r.h = red_cassette[0]->h;

  switch( sdl_tape_state ) {
  case UI_STATUSBAR_STATE_ACTIVE:
    sdl_blit_icon( green_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  case UI_STATUSBAR_STATE_INACTIVE:
  case UI_STATUSBAR_STATE_NOT_AVAILABLE:
    sdl_blit_icon( red_cassette, &r, tmp_screen_pitch, dstPitch );
    break;
  }

  sdl_status_updated = 0;
}

/* Set one pixel in the display */
void
uidisplay_putpixel( int x, int y, int colour )
{
  libspectrum_word *dest_base, *dest;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;

  Uint32 palette_colour = palette_values[ colour ];

  if( machine_current->timex ) {
    x <<= 1; y <<= 1;
    dest_base = dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
    dest = (libspectrum_word*)
      ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
    *(dest++) = palette_colour;
    *(dest++) = palette_colour;
  } else {
    dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    *dest = palette_colour;
  }
}

/* Print the 8 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (8*x) , y ) */
void
uidisplay_plot8( int x, int y, libspectrum_byte data,
	         libspectrum_byte ink, libspectrum_byte paper )
{
  libspectrum_word *dest;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;

  Uint32 palette_ink = palette_values[ ink ];
  Uint32 palette_paper = palette_values[ paper ];

  if( machine_current->timex ) {
    int i;
    libspectrum_word *dest_base;

    x <<= 4; y <<= 1;

    dest_base =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    for( i=0; i<2; i++ ) {
      dest = dest_base;

      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
      *(dest++) = ( data & 0x01 ) ? palette_ink : palette_paper;
      *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;

      dest_base = (libspectrum_word*)
        ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
    }
  } else {
    x <<= 3;

    dest =
      (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                           (x+1) * tmp_screen->format->BytesPerPixel +
                           (y+1) * tmp_screen->pitch);

    *(dest++) = ( data & 0x80 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x40 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x20 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x10 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x08 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x04 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x02 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x01 ) ? palette_ink : palette_paper;
  }
}

/* Print the 16 pixels in `data' using ink colour `ink' and paper
   colour `paper' to the screen at ( (16*x) , y ) */
void
uidisplay_plot16( int x, int y, libspectrum_word data,
		  libspectrum_byte ink, libspectrum_byte paper )
{
  libspectrum_word *dest_base, *dest;
  int i;
  Uint32 *palette_values = settings_current.bw_tv ? bw_values :
                           colour_values;
  Uint32 palette_ink = palette_values[ ink ];
  Uint32 palette_paper = palette_values[ paper ];
  x <<= 4; y <<= 1;

  dest_base =
    (libspectrum_word*)( (libspectrum_byte*)tmp_screen->pixels +
                         (x+1) * tmp_screen->format->BytesPerPixel +
                         (y+1) * tmp_screen->pitch);

  for( i=0; i<2; i++ ) {
    dest = dest_base;

    *(dest++) = ( data & 0x8000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x4000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x2000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x1000 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0800 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0400 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0200 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0100 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0080 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0040 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0020 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0010 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0008 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0004 ) ? palette_ink : palette_paper;
    *(dest++) = ( data & 0x0002 ) ? palette_ink : palette_paper;
    *dest     = ( data & 0x0001 ) ? palette_ink : palette_paper;

    dest_base = (libspectrum_word*)
      ( (libspectrum_byte*)dest_base + tmp_screen->pitch);
  }
}

void
uidisplay_frame_end( void )
{
  SDL_Rect *r;
  Uint32 tmp_screen_pitch, dstPitch;
  SDL_Rect *last_rect;

  /* We check for a switch to fullscreen here to give systems with a
     windowed-only UI a chance to free menu etc. resources before
     the switch to fullscreen (e.g. Mac OS X) */
  if( sdldisplay_is_full_screen != settings_current.full_screen &&
      uidisplay_hotswap_gfx_mode() ) {
    fprintf( stderr, "%s: Error switching to fullscreen\n", fuse_progname );
    fuse_abort();
  }

  /* Force a full redraw if requested */
  if ( sdldisplay_force_full_refresh ) {
    num_rects = 1;

    updated_rects[0].x = 0;
    updated_rects[0].y = 0;
    updated_rects[0].w = image_width;
    updated_rects[0].h = image_height;
  }

  if ( !(ui_widget_level >= 0) && num_rects == 0 && !sdl_status_updated )
    return;

  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_LockSurface( sdldisplay_gc );

  tmp_screen_pitch = tmp_screen->pitch;

  dstPitch = sdldisplay_gc->pitch;

  last_rect = updated_rects + num_rects;

  for( r = updated_rects; r != last_rect; r++ ) {

    int dst_y = r->y * sdldisplay_current_size + fullscreen_y_off;
    int dst_h = r->h;
    int dst_x = r->x * sdldisplay_current_size + fullscreen_x_off;

#ifdef SDL_USE_GL
/* we just update the texture */
    if( gl_scaler ) {
      glTexSubImage2D( GL_TEXTURE_2D, 0, r->x, r->y, r->w, r->h,
                       GL_RGB, GL_UNSIGNED_SHORT_5_6_5,
                       tmp_screen->pixels + ( r->y + 1 ) * tmp_screen_pitch
                                          + 2 * ( r->x + 1 ) );
    } else {
#endif  /* #ifdef SDL_USE_GL */
    scaler_proc16(
      (libspectrum_byte*)tmp_screen->pixels +
                        (r->x+1) * tmp_screen->format->BytesPerPixel +
	                (r->y+1)*tmp_screen_pitch,
      tmp_screen_pitch,
      (libspectrum_byte*)sdldisplay_gc->pixels +
	                 dst_x * sdldisplay_gc->format->BytesPerPixel +
			 dst_y*dstPitch,
      dstPitch, r->w, dst_h
    );
#ifdef SDL_USE_GL
    }
#endif

    /* Adjust rects for the destination rect size */
    r->x = dst_x;
    r->y = dst_y;
    r->w *= sdldisplay_current_size;
    r->h = dst_h * sdldisplay_current_size;
  }

  if ( settings_current.statusbar )
    sdl_icon_overlay( tmp_screen_pitch, dstPitch );

  if( SDL_MUSTLOCK( sdldisplay_gc ) ) SDL_UnlockSurface( sdldisplay_gc );

#ifdef SDL_USE_GL
  /* with OpenGL we draw the whole texture to the screen (a GL_QUAD) */
  if( gl_scaler ) {
    glClear( GL_COLOR_BUFFER_BIT );
    glEnable( GL_TEXTURE_2D );
    glBegin( GL_QUADS );
    glTexCoord2f( 0.0, gl_th );
    glVertex2f( -gl_vw, -gl_vh );
    glTexCoord2f( 0.0, 0.0 );
    glVertex2f( -gl_vw, gl_vh );
    glTexCoord2f( gl_tw, 0.0 );
    glVertex2f( gl_vw, gl_vh );
    glTexCoord2f( gl_tw, gl_th );
    glVertex2f( gl_vw, -gl_vh );
    glEnd();
    glDisable( GL_TEXTURE_2D );
    /* and swap GL buffers */
    SDL_GL_SwapBuffers();
  } else {
#endif  /* #ifdef SDL_USE_GL */
  /* Finally, blit all our changes to the screen */
  SDL_UpdateRects( sdldisplay_gc, num_rects, updated_rects );
#ifdef SDL_USE_GL
  }
#endif

  num_rects = 0;
  sdldisplay_force_full_refresh = 0;
}

void
uidisplay_area( int x, int y, int width, int height )
{
  if ( sdldisplay_force_full_refresh )
    return;

  if( num_rects == MAX_UPDATE_RECT ) {
    sdldisplay_force_full_refresh = 1;
    return;
  }

  /* Extend the dirty region by 1 pixel for scalers
     that "smear" the screen, e.g. 2xSAI */
  if( scaler_flags & SCALER_FLAGS_EXPAND )
    scaler_expander( &x, &y, &width, &height, image_width, image_height );

  updated_rects[num_rects].x = x;
  updated_rects[num_rects].y = y;
  updated_rects[num_rects].w = width;
  updated_rects[num_rects].h = height;

  num_rects++;
}

int
uidisplay_end( void )
{
  int i;

  display_ui_initialised = 0;

  if ( tmp_screen ) {
    free( tmp_screen->pixels );
    SDL_FreeSurface( tmp_screen ); tmp_screen = NULL;
  }

  if( saved ) {
    SDL_FreeSurface( saved ); saved = NULL;
  }

  for( i=0; i<2; i++ ) {
    if ( red_cassette[i] ) {
      SDL_FreeSurface( red_cassette[i] ); red_cassette[i] = NULL;
    }
    if ( green_cassette[i] ) {
      SDL_FreeSurface( green_cassette[i] ); green_cassette[i] = NULL;
    }
    if ( red_mdr[i] ) {
      SDL_FreeSurface( red_mdr[i] ); red_mdr[i] = NULL;
    }
    if ( green_mdr[i] ) {
      SDL_FreeSurface( green_mdr[i] ); green_mdr[i] = NULL;
    }
    if ( red_disk[i] ) {
      SDL_FreeSurface( red_disk[i] ); red_disk[i] = NULL;
    }
    if ( green_disk[i] ) {
      SDL_FreeSurface( green_disk[i] ); green_disk[i] = NULL;
    }
  }

  return 0;
}

/* The statusbar handling function */
int
ui_statusbar_update( ui_statusbar_item item, ui_statusbar_state state )
{
  switch( item ) {

  case UI_STATUSBAR_ITEM_DISK:
    sdl_disk_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_PAUSED:
    /* We don't support pausing this version of Fuse */
    return 0;

  case UI_STATUSBAR_ITEM_TAPE:
    sdl_tape_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_MICRODRIVE:
    sdl_mdr_state = state;
    sdl_status_updated = 1;
    return 0;

  case UI_STATUSBAR_ITEM_MOUSE:
    /* We don't support showing a grab icon */
    return 0;

  }

  ui_error( UI_ERROR_ERROR, "Attempt to update unknown statusbar item %d",
            item );
  return 1;
}
