/*
 * cat.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <unistd.h>

#include <gio/gio.h>
#include <gio/gunixinputstream.h>
#include <gio/gunixoutputstream.h>

#include "cat-util.h"

/* NOTE:
 *
 * `cat` from coreutils is certainly faster than this, especially if you're
 * doing things like `./examples/cat foo > bar` as it will use
 * copy_file_range() to avoid reading into userspace.
 *
 * `gio cat` is likely faster than this doing synchronous IO on the calling
 * thread because it doesn't have to coordinate across thread pools.
 */

static DexFuture *
cat_read_fiber (gpointer user_data)
{
  Cat *cat = user_data;
  GInputStream *stream = g_unix_input_stream_new (cat->read_fd, FALSE);
  Buffer *buffer = NULL;

  for (;;)
    {
      Buffer *next;

      /* Suspend while sending the buffer to the channel, which may
       * help throttle reads if they get too far ahead of writes.
       */
      if (buffer != NULL)
        dex_await (dex_channel_send (cat->channel,
                                     dex_future_new_for_pointer (g_steal_pointer (&buffer))),
                   NULL);

      /* Get next buffer from the pool */
      next = cat_pop_buffer (cat);

      /* Suspend while reading into the buffer */
      next->length = dex_await_int64 (dex_input_stream_read (stream,
                                                             next->data,
                                                             next->capacity,
                                                             G_PRIORITY_DEFAULT),
                                      NULL);

      /* If we got length <= 0, we failed or finished */
      if (next->length <= 0)
        {
          dex_channel_close_send (cat->channel);
          cat_push_buffer (cat, next);
          break;
        }

      buffer = next;
    }

  /* Suspend while we close the stream */
  dex_await (dex_input_stream_close (stream, 0), NULL);

  g_object_unref (stream);

  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
cat_write_fiber (gpointer user_data)
{
  Cat *cat = user_data;
  GOutputStream *stream = g_unix_output_stream_new (cat->write_fd, FALSE);

  for (;;)
    {
      Buffer *buffer;
      gssize len;

      /* Suspend while we wait for another buffer (or error on channel closed) */
      if (!(buffer = dex_await_pointer (dex_channel_receive (cat->channel), NULL)))
        break;

      /* Suspend while we write the buffer contents to output stream */
      len = dex_await_int64 (dex_output_stream_write (G_OUTPUT_STREAM (stream),
                                                      buffer->data,
                                                      buffer->length,
                                                      G_PRIORITY_DEFAULT),
                            NULL);

      /* Give the buffer back to the pool */
      cat_push_buffer (cat, buffer);

      /* Bail if we got a failure or empty buffer */
      if (len <= 0)
        break;
    }

  /* Asynchronously close the stream, which may cause flushing to occur. */
  dex_await (dex_output_stream_close (stream, 0), NULL);

  /* Release the stream, which should not block since it was closed above */
  g_object_unref (stream);

  return dex_future_new_for_boolean (TRUE);
}

int
main (int   argc,
      char *argv[])
{
  GError *error = NULL;
  Cat cat;
  int ret = EXIT_SUCCESS;

  dex_init ();

  if (!cat_init (&cat, &argc, &argv, &error) ||
      !cat_run (&cat,
                dex_scheduler_spawn (NULL, 0, cat_read_fiber, &cat, NULL),
                dex_scheduler_spawn (NULL, 0, cat_write_fiber, &cat, NULL),
                &error))
    {
      g_printerr ("cat: %s\n", error->message);
      g_clear_error (&error);
      ret = EXIT_FAILURE;
    }

  cat_clear (&cat);

  return ret;
}
