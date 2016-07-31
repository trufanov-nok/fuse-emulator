/* listener.c: Debugger listener 
   Copyright (c) 2016 Philip Kendall

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

#include <config.h>

#include <pthread.h>

#ifdef WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#else
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif

#include "compat.h"
#include "debugger/debugger.h"
#include "fuse.h"
#include "ui/ui.h"

static compat_socket_t listener_socket;

static compat_socket_t server_socket;

static pthread_t thread;

static sig_atomic_t stop_io_thread;

static compat_socket_selfpipe_t *selfpipe;

static pthread_mutex_t command_buffer_mutex;

/* Access to any of the command_buffer_* variables should occur only
   under the command_buffer_mutex */

static char *command_buffer = NULL;

static size_t command_buffer_size = 0;

static size_t command_buffer_length = 0;

/* Functions that can be called from either thread */

static void
acquire_command_buffer_lock( void )
{
  int error = pthread_mutex_lock( &command_buffer_mutex );
  if( error ) {
    printf( "debugger_listener: error %d locking mutex\n", error );
    fuse_abort();
  }
}

static void
release_command_buffer_lock( void )
{
  int error = pthread_mutex_unlock( &command_buffer_mutex );
  if( error ) {
    printf( "debugger_listener: error %d unlocking mutex\n", error );
    fuse_abort();
  }
}

/* Functions called solely from the IO thread */

static void
accept_new_connection( void )
{
  struct sockaddr_in sa;
  socklen_t sa_length = sizeof(sa);

  memset( &sa, 0, sa_length );

  server_socket = accept( listener_socket, (struct sockaddr*)&sa, &sa_length );
  if( server_socket == compat_socket_invalid ) {
    printf( "debugger_listener: error from accept: errno %d: %s\n", compat_socket_get_error(), compat_socket_get_strerror() );
    return;
  }

  printf( "debugger_listener: accepted connection from %s:%d\n", inet_ntoa(sa.sin_addr), ntohs(sa.sin_port) );
}

static void
append_to_command_buffer( const char *read_buffer, ssize_t bytes_read )
{
  size_t desired_size;

  acquire_command_buffer_lock();

  desired_size = command_buffer_length + bytes_read;

  printf( "debugger_listener: len %zu, read %zu, size %zu, desired %zu\n", command_buffer_length, bytes_read, command_buffer_size, desired_size );

  if( desired_size > command_buffer_size ) {
    size_t new_size = 2 * command_buffer_size;
    if( new_size < desired_size ) new_size = desired_size;

    printf( "debugger_listener: new_size %zu\n", new_size );

    command_buffer = libspectrum_renew( char, command_buffer, new_size + 1 );
    command_buffer_size = new_size;
  }

  memcpy( command_buffer + command_buffer_length, read_buffer, bytes_read );
  command_buffer_length += bytes_read;
  command_buffer[ command_buffer_length ] = 0;

  printf( "debugger_listener: current buffer: \"%s\"\n", command_buffer );

  release_command_buffer_lock();
}

static void
read_data( void )
{
  ssize_t bytes_read;
  char read_buffer[ 1024 ];

  bytes_read = recv( server_socket, read_buffer, 1023, 0 );
  if( bytes_read > 0 ) {
    append_to_command_buffer( read_buffer, bytes_read );
  } else if( bytes_read == 0 ) {
    printf( "debugger_listener: server connection closed\n" );
    compat_socket_close( server_socket );
    server_socket = compat_socket_invalid;
  } else {
    printf( "debugger_listener: error %d reading from socket: %s\n", compat_socket_get_error(), compat_socket_get_strerror() );
  }
}

static void*
listener_io_thread( void *arg )
{
  while( !stop_io_thread ) {

    compat_socket_t selfpipe_socket =
      compat_socket_selfpipe_get_read_fd( selfpipe );

    int max_fd = selfpipe_socket;

    fd_set read_fds;

    int active;

    FD_ZERO( &read_fds );
    FD_SET( selfpipe_socket, &read_fds );

    /* If we don't have a currently active connection, add the listener;
       otherwise, add the active connection */
    if( server_socket == compat_socket_invalid ) {
      printf( "debugger_listener: adding listener socket\n" );
      FD_SET( listener_socket, &read_fds );
      if( listener_socket > max_fd ) max_fd = listener_socket;
    } else {
      printf( "debugger_listener: adding server socket\n" );
      FD_SET( server_socket, &read_fds );
      if( server_socket > max_fd ) max_fd = server_socket;
    }

    printf( "debugger_listener: calling select\n" );

    active = select( max_fd + 1, &read_fds, NULL, NULL, NULL );

    printf( "debugger_listener: back from select\n" );

    if( active != -1 ) {
      if( FD_ISSET( selfpipe_socket, &read_fds ) ) {
        printf( "debugger_listener: discarding selfpipe data\n" );
        compat_socket_selfpipe_discard_data( selfpipe );
      }

      if( FD_ISSET( listener_socket, &read_fds ) ) accept_new_connection();

      if( FD_ISSET( server_socket, &read_fds ) ) read_data();

    } else {
      printf( "debugger_listener: error from select(): %d\n", active );
    }
  }

  return NULL;
}

/* Functions called solely from the main Fuse thread */

static int
create_listener( void )
{
  struct sockaddr_in sa;
  int one = 1;

  listener_socket = socket( AF_INET, SOCK_STREAM, IPPROTO_TCP );
  if( listener_socket == compat_socket_invalid ) {
    printf( "debugger_listener: failed to open socket: errno %d: %s\n", compat_socket_get_error(), compat_socket_get_strerror() );
    goto cleanup;
  }

  if( setsockopt( listener_socket, SOL_SOCKET, SO_REUSEADDR, &one,
    sizeof(one) ) == -1 ) {
    printf( "debugger_listener: failed to set SO_REUSEADDR: errno %d: %s\n", compat_socket_get_error(), compat_socket_get_strerror() );
    goto cleanup;
  }

  memset( &sa, 0, sizeof(sa) );
  sa.sin_family = AF_INET;
  sa.sin_port = htons(29552);
  sa.sin_addr.s_addr = INADDR_ANY;

  if( bind( listener_socket, (struct sockaddr*)&sa, sizeof(sa) ) == -1 ) {
    printf( "debugger_listener: failed to bind socket: errno %d: %s\n", compat_socket_get_error(), compat_socket_get_strerror() );
    goto cleanup;
  }

  if( listen( listener_socket, 1 ) ) {
    printf( "debugger_listener: failed to listen: errno %d: %s\n", compat_socket_get_error(), compat_socket_get_strerror() );
    goto cleanup;
  }

  return 0;

  cleanup:

  if( listener_socket != compat_socket_invalid ) {
    compat_socket_close( listener_socket );
    listener_socket = compat_socket_invalid;
  }
  return 1;
}

void
debugger_listener_init( void )
{
  int error;

  server_socket = compat_socket_invalid;

  pthread_mutex_init( &command_buffer_mutex, NULL );

  if( create_listener() ) return;

  selfpipe = compat_socket_selfpipe_alloc();

  error = pthread_create( &thread, NULL, listener_io_thread, NULL );
  if( error ) {
    ui_error( UI_ERROR_ERROR, "debugger_listener: error %d creating thread",
              error );
    fuse_abort();
  }
}

void
debugger_listener_check( void )
{
  acquire_command_buffer_lock();

  if( command_buffer_length > 0 ) {
    printf( "debugger_listener: consuming command buffer \"%s\"\n", command_buffer );
    debugger_command_evaluate( command_buffer );
    command_buffer_length = 0;
  }

  release_command_buffer_lock();
}

void
debugger_listener_end( void )
{
  if( listener_socket != compat_socket_invalid ) {
    stop_io_thread = 1;
    compat_socket_selfpipe_wake( selfpipe );
    pthread_join( thread, NULL );
    compat_socket_selfpipe_free( selfpipe );

    compat_socket_close( listener_socket );
  }

  if( server_socket != compat_socket_invalid )
    compat_socket_close( server_socket );

  libspectrum_free( command_buffer );

  pthread_mutex_destroy( &command_buffer_mutex );
}
