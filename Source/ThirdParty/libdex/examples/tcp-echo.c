/* tcp-echo.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This library is free software; you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as
 * published by the Free Software Foundation; either version 2.1 of the
 * License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libdex.h>

static DexScheduler *thread_pool;

static DexFuture *
socket_connection_fiber (gpointer user_data)
{
  GSocketConnection *connection = user_data;
  GOutputStream *output;
  GInputStream *input;
  GError *error = NULL;
  guint8 buffer[1024];

  g_assert (G_IS_SOCKET_CONNECTION (connection));

  input = g_io_stream_get_input_stream (G_IO_STREAM (connection));
  output = g_io_stream_get_output_stream (G_IO_STREAM (connection));

  for (;;)
    {
      gssize n_read;
      gssize to_write;
      gssize n_written;

      n_read = dex_await_int64 (dex_input_stream_read (input, buffer, sizeof buffer, G_PRIORITY_DEFAULT), &error);
      if (n_read == 0 || error != NULL)
        break;

      for (to_write = n_read; to_write > 0; to_write -= n_written)
        {
          n_written = dex_await_int64 (dex_output_stream_write (output, &buffer[n_read-to_write], to_write, G_PRIORITY_HIGH), &error);
          if (n_written == 0 || error != NULL)
            break;
        }
    }

  return error ? dex_future_new_for_error (error) : NULL;
}

static DexFuture *
socket_listener_fiber (gpointer user_data)
{
  GSocketListener *socket_listener = user_data;

  g_assert (G_IS_SOCKET_LISTENER (socket_listener));

  for (;;)
    {
      g_autoptr(GSocketConnection) connection = NULL;
      g_autoptr(DexFuture) fiber = NULL;
      g_autoptr(GError) error = NULL;

      /* Accept an incoming connection */
      if (!(connection = dex_await_object (dex_socket_listener_accept (socket_listener), &error)))
        return dex_future_new_for_error (g_steal_pointer (&error));

      /* Spawn a fiber to handle connection on thread pool */
      fiber = dex_scheduler_spawn (thread_pool, 0,
                                   socket_connection_fiber,
                                   g_steal_pointer (&connection),
                                   g_object_unref);
    }

  return NULL;
}

static DexFuture *
quit_cb (DexFuture *completed,
         gpointer   user_data)
{
  GMainLoop *main_loop = user_data;
  g_main_loop_quit (main_loop);
  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GSocketListener) socket_listener = NULL;
  g_autoptr(GMainLoop) main_loop = NULL;
  g_autoptr(DexFuture) future = NULL;
  g_autoptr(GError) error = NULL;
  guint port = 8080;

  dex_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  thread_pool = dex_thread_pool_scheduler_new ();
  socket_listener = g_socket_listener_new ();

  /* Try to listen on configured port, or bail */
  if (!g_socket_listener_add_inet_port (socket_listener, port, NULL, &error))
    g_error ("Failed to listen on port %u: %s", port, error->message);

  g_print ("Listening on 0.0.0.0:%u\n", port);

  /* Spawn a fiber on current thread for socket loop */
  future = dex_scheduler_spawn (NULL, 0,
                                socket_listener_fiber,
                                g_object_ref (socket_listener),
                                g_object_unref);

  /* When it completes, call quit_cb to quit main loop */
  future = dex_future_finally (future,
                               quit_cb,
                               g_main_loop_ref (main_loop),
                               (GDestroyNotify) g_main_loop_unref);

  /* block on our main loop until quit_cb runs */
  g_main_loop_run (main_loop);

  return 0;
}
