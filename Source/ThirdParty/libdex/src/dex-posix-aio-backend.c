/* dex-posix-aio-backend.c
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

#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "dex-compat-private.h"
#include "dex-posix-aio-backend-private.h"
#include "dex-posix-aio-future-private.h"

#define N_IO_WORKERS 8

/*
 * The DexPosixAioBackend uses synchronous IO on a worker threads for
 * read()/write() as we don't have a reliable poll()-style interface for
 * doing IO on regular files. epoll_create() would be an option except
 * that on Linux it doesn't guarantee support for regular files to
 * return EAGAIN properly.
 *
 * If/when we introduce AIO operations for send()/recv() then those would
 * use g_source_add_unix_fd()/g_source_remove_unix_fd() to poll() within
 * the GMainContext.
 *
 * This is primarily meant to be a fallback for cases where we cannot
 * support more specific APIs like io_posix or kqueue.
 */

struct _DexPosixAioBackend
{
  DexAioBackend parent_instance;
};

struct _DexPosixAioBackendClass
{
  DexAioBackendClass parent_class;
};

typedef struct _DexPosixAioContext
{
  DexAioContext parent;
  GMutex        mutex;
  GQueue        completed;
} DexPosixAioContext;

DEX_DEFINE_FINAL_TYPE (DexPosixAioBackend, dex_posix_aio_backend, DEX_TYPE_AIO_BACKEND)

static GThreadPool *io_thread_pool;

static void
dex_posix_aio_context_take (DexPosixAioContext *posix_aio_context,
                            DexPosixAioFuture  *posix_aio_future)
{
  GMainContext *main_context;

  g_assert (posix_aio_context != NULL);
  g_assert (DEX_IS_POSIX_AIO_FUTURE (posix_aio_future));

  g_mutex_lock (&posix_aio_context->mutex);
  g_queue_push_tail (&posix_aio_context->completed, posix_aio_future);
  main_context = dex_posix_aio_future_get_main_context (posix_aio_future);
  g_mutex_unlock (&posix_aio_context->mutex);

  if (main_context)
    g_main_context_wakeup (main_context);
}

static inline void
steal_queue (GQueue *from_queue,
             GQueue *to_queue)
{
  *to_queue = *from_queue;
  *from_queue = (GQueue) {NULL, NULL, 0};
}

static gboolean
dex_posix_aio_context_dispatch (GSource     *source,
                                GSourceFunc  callback,
                                gpointer     user_data)
{
  DexPosixAioContext *aio_context = (DexPosixAioContext *)source;
  GQueue completed;

  g_mutex_lock (&aio_context->mutex);
  steal_queue (&aio_context->completed, &completed);
  g_mutex_unlock (&aio_context->mutex);

  while (completed.length > 0)
    {
      DexPosixAioFuture *posix_aio_future = g_queue_pop_head (&completed);
      dex_posix_aio_future_complete (posix_aio_future);
      dex_unref (posix_aio_future);
    }

  return G_SOURCE_CONTINUE;
}

static gboolean
dex_posix_aio_context_check (GSource *source)
{
  DexPosixAioContext *aio_context = (DexPosixAioContext *)source;
  gboolean ret;

  g_assert (aio_context != NULL);
  g_assert (DEX_IS_POSIX_AIO_BACKEND (aio_context->parent.aio_backend));

  g_mutex_lock (&aio_context->mutex);
  ret = aio_context->completed.length > 0;
  g_mutex_unlock (&aio_context->mutex);

  return ret;
}

static gboolean
dex_posix_aio_context_prepare (GSource *source,
                               int     *timeout)
{
  *timeout = -1;
  return dex_posix_aio_context_check (source);
}

static void
dex_posix_aio_context_finalize (GSource *source)
{
  DexPosixAioContext *aio_context = (DexPosixAioContext *)source;

  g_assert (aio_context != NULL);
  g_assert (DEX_IS_POSIX_AIO_BACKEND (aio_context->parent.aio_backend));
  g_assert (aio_context->completed.length == 0);

  g_mutex_clear (&aio_context->mutex);
}

static GSourceFuncs dex_posix_aio_context_source_funcs = {
  .check = dex_posix_aio_context_check,
  .prepare = dex_posix_aio_context_prepare,
  .dispatch = dex_posix_aio_context_dispatch,
  .finalize = dex_posix_aio_context_finalize,
};

static DexAioContext *
dex_posix_aio_backend_create_context (DexAioBackend *aio_backend)
{
  DexPosixAioContext *aio_context;

  g_assert (DEX_IS_POSIX_AIO_BACKEND (aio_backend));

  aio_context = (DexPosixAioContext *)
    g_source_new (&dex_posix_aio_context_source_funcs,
                  sizeof *aio_context);
  _g_source_set_static_name ((GSource *)aio_context, "[dex-posix-aio-backend]");
  g_source_set_can_recurse ((GSource *)aio_context, TRUE);
  aio_context->parent.aio_backend = dex_ref (aio_backend);
  g_mutex_init (&aio_context->mutex);

  return (DexAioContext *)aio_context;
}

static DexFuture *
dex_posix_aio_backend_read (DexAioBackend *aio_backend,
                            DexAioContext *aio_context,
                            int            fd,
                            gpointer       buffer,
                            gsize          count,
                            goffset        offset)
{
  DexPosixAioFuture *posix_aio_future;

  posix_aio_future = dex_posix_aio_future_new_read ((DexPosixAioContext *)aio_context, fd, buffer, count, offset);
  g_thread_pool_push (io_thread_pool, dex_ref (posix_aio_future), NULL);

  return DEX_FUTURE (posix_aio_future);
}

static DexFuture *
dex_posix_aio_backend_write (DexAioBackend *aio_backend,
                             DexAioContext *aio_context,
                             int            fd,
                             gconstpointer  buffer,
                             gsize          count,
                             goffset        offset)
{
  DexPosixAioFuture *posix_aio_future;

  posix_aio_future = dex_posix_aio_future_new_write ((DexPosixAioContext *)aio_context, fd, buffer, count, offset);
  g_thread_pool_push (io_thread_pool, dex_ref (posix_aio_future), NULL);

  return DEX_FUTURE (posix_aio_future);
}

static void
dex_posix_aio_backend_worker (gpointer data,
                              gpointer user_data)
{
  DexPosixAioFuture *posix_aio_future = data;
  DexPosixAioContext *posix_aio_context;

  g_assert (DEX_IS_POSIX_AIO_FUTURE (posix_aio_future));

  posix_aio_context = dex_posix_aio_future_get_aio_context (posix_aio_future);
  dex_posix_aio_future_run (posix_aio_future);
  dex_posix_aio_context_take (posix_aio_context, posix_aio_future);
}

static void
dex_posix_aio_backend_class_init (DexPosixAioBackendClass *posix_aio_backend_class)
{
  DexAioBackendClass *aio_backend_class = DEX_AIO_BACKEND_CLASS (posix_aio_backend_class);
  GError *error = NULL;

  aio_backend_class->create_context = dex_posix_aio_backend_create_context;
  aio_backend_class->read = dex_posix_aio_backend_read;
  aio_backend_class->write = dex_posix_aio_backend_write;

  io_thread_pool = g_thread_pool_new (dex_posix_aio_backend_worker,
                                      NULL,
                                      N_IO_WORKERS,
                                      FALSE,
                                      &error);

  if (io_thread_pool == NULL)
    g_error ("Failed to create thread pool: %s", error->message);

  g_type_ensure (DEX_TYPE_POSIX_AIO_FUTURE);
}

static void
dex_posix_aio_backend_init (DexPosixAioBackend *posix_aio_backend)
{
}

DexAioBackend *
dex_posix_aio_backend_new (void)
{
  return (DexAioBackend *)dex_object_create_instance (DEX_TYPE_POSIX_AIO_BACKEND);
}
