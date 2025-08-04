/* cat-util.h
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

#pragma once

#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#include <glib/gstdio.h>

#include <libdex.h>

/* You don't need to do this in your code. This just allows us to build
 * Dex on older systems where GLib features we want to use are not available
 * such as aligned allocations.
 */
#include "dex-compat-private.h"

G_BEGIN_DECLS

typedef struct _Cat Cat;
typedef struct _Buffer Buffer;

struct _Cat
{
  gsize buffer_size;
  int read_fd;
  int write_fd;
  gssize to_read;
  GQueue buffer_pool;
  DexChannel *channel;
  GMainLoop *main_loop;
  GError *error;
  Buffer *current;
};

struct _Buffer
{
  Cat      *cat;
  GList     link;
  gpointer  data;
  gsize     capacity;
  gssize    length;
};

static inline Buffer *
buffer_new (Cat *cat)
{
  Buffer *buffer = g_new0 (Buffer, 1);

  buffer->cat = cat;
  buffer->link.data = buffer;
  buffer->data = g_aligned_alloc (1, cat->buffer_size, 4096);
  buffer->capacity = cat->buffer_size;
  buffer->length = 0;

  return buffer;
}

static inline void
buffer_free (Buffer *buffer)
{
  g_aligned_free (buffer->data);
  g_free (buffer);
}

static inline gboolean
cat_error (GError **error,
           int      errsv)
{
  g_set_error_literal (error,
                       G_IO_ERROR,
                       g_io_error_from_errno (errsv),
                       g_strerror (errsv));
  return FALSE;
}

static inline gboolean
cat_init (Cat      *cat,
          int      *argc,
          char   ***argv,
          GError  **error)
{
  GOptionContext *context;
  char *output = NULL;
  gboolean ret = FALSE;
  int buffer_size = (1024*256 - 2*sizeof(gpointer)); /* 256k minus malloc overhead */
  int queue_size = 32;
#ifdef HAVE_POSIX_FADVISE
  gssize len;
#endif
  struct stat stbuf;
  const GOptionEntry entries[] = {
    { "output", 'o', 0, G_OPTION_ARG_FILENAME, &output, "Cat contents into OUTPUT", "OUTPUT" },
    { "buffer-size", 'b', 0, G_OPTION_ARG_INT, &buffer_size, "Read/Write buffer size", "BYTES" },
    { "queue-size", 'q', 0, G_OPTION_ARG_INT, &queue_size, "Amount of reads that can advance ahead of writes (default 32)", "COUNT" },
    { 0 }
  };

  memset (cat, 0, sizeof *cat);

  cat->buffer_size = buffer_size;
  cat->read_fd = -1;
  cat->write_fd = -1;
  cat->channel = dex_channel_new (queue_size);
  cat->main_loop = g_main_loop_new (NULL, FALSE);

  context = g_option_context_new ("- FILE");
  g_option_context_add_main_entries (context, entries, NULL);
  if (!g_option_context_parse (context, argc, argv, error))
    goto cleanup;

  cat->write_fd = STDOUT_FILENO;

  if (output != NULL)
    {
      g_unlink (output);

      if (-1 == (cat->write_fd = open (output, O_WRONLY|O_CREAT, 0644)))
        goto cleanup;
    }

  if (*argc > 2)
    {
      g_set_error_literal (error,
                           G_IO_ERROR,
                           G_IO_ERROR_INVAL,
                           "Only a single file is supported");
      goto cleanup;
    }
  else if (*argc == 2)
    {
      if (-1 == (cat->read_fd = open ((*argv)[1], O_RDONLY)))
        goto cleanup;
    }
  else
    {
      cat->read_fd = STDIN_FILENO;
    }

#ifdef HAVE_POSIX_FADVISE
  len = lseek (cat->read_fd, 0, SEEK_END);

  if (len > 0)
    {
      posix_fadvise (cat->read_fd, 0, len, POSIX_FADV_SEQUENTIAL);
      posix_fadvise (cat->write_fd, 0, len, POSIX_FADV_SEQUENTIAL);

      lseek (cat->read_fd, 0, SEEK_SET);
    }
#endif

  if (fstat (cat->read_fd, &stbuf) == 0)
    cat->to_read = stbuf.st_size;
  else
    cat->to_read = -1;

  ret = TRUE;

cleanup:
  if (ret == FALSE)
    {
      int errsv = errno;
      char *help = g_option_context_get_help (context, TRUE, NULL);
      g_printerr ("%s\n", help);
      g_free (help);

      if (error && !*error)
        g_set_error_literal (error,
                             G_IO_ERROR,
                             g_io_error_from_errno (errsv),
                             g_strerror (errsv));
    }

  g_option_context_free (context);
  g_free (output);

  return ret;
}

static inline void
cat_clear (Cat *cat)
{
  if (cat->write_fd >= 0)
    close (cat->write_fd);

  if (cat->read_fd >= 0)
    close (cat->read_fd);

  while (cat->buffer_pool.head != NULL)
    {
      Buffer *buffer = cat->buffer_pool.head->data;
      g_queue_unlink (&cat->buffer_pool, &buffer->link);
      buffer_free (buffer);
    }

  dex_clear (&cat->channel);
  g_clear_pointer (&cat->current, buffer_free);
  g_clear_pointer (&cat->main_loop, g_main_loop_unref);
}

static inline Buffer *
cat_pop_buffer (Cat *cat)
{
  if (cat->buffer_pool.length == 0)
    return buffer_new (cat);

  return g_queue_pop_head_link (&cat->buffer_pool)->data;
}

static inline void
cat_push_buffer (Cat    *cat,
                 Buffer *buffer)
{
  g_assert (cat != NULL);
  g_assert (buffer != NULL);
  g_assert (buffer->link.data == buffer);
  g_assert (buffer->link.prev == NULL);
  g_assert (buffer->link.next == NULL);

  buffer->length = 0;

  g_queue_push_head_link (&cat->buffer_pool, &buffer->link);
}

static inline DexFuture *
cat_run_cb (DexFuture *future,
            gpointer   user_data)
{
  Cat *cat = user_data;
  dex_future_get_value (future, &cat->error);
  g_main_loop_quit (cat->main_loop);
  return NULL;
}

static inline DexFuture *
cat_close_send (DexFuture *future,
                gpointer   user_data)
{
  Cat *cat = user_data;
  dex_channel_close_send (cat->channel);
  return NULL;
}

static inline gboolean
cat_run (Cat        *cat,
         DexFuture  *read_routine,
         DexFuture  *write_routine,
         GError    **error)
{
  DexFuture *future;

  future = dex_future_finally (read_routine, cat_close_send, cat, NULL);
  future = dex_future_all (future, write_routine, NULL);
  future = dex_future_finally (future, cat_run_cb, cat, NULL);

  g_main_loop_run (cat->main_loop);

  dex_unref (future);

  if (cat->error)
    {
      g_propagate_error (error, g_steal_pointer (&cat->error));
      return FALSE;
    }

  return TRUE;
}

G_END_DECLS
