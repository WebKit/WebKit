/* dex-uring-aio-backend.c
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
#include <sys/eventfd.h>
#include <sys/utsname.h>
#include <stdio.h>
#include <unistd.h>

#include <liburing.h>

#include "dex-thread-storage-private.h"
#include "dex-uring-aio-backend-private.h"
#include "dex-uring-future-private.h"
#include "dex-uring-version.h"

#define DEFAULT_URING_SIZE 32

struct _DexUringAioBackend
{
  DexAioBackend parent_instance;
};

struct _DexUringAioBackendClass
{
  DexAioBackendClass parent_class;
};

typedef struct _DexUringAioContext
{
  DexAioContext    parent;
  struct io_uring  ring;
  int              eventfd;
  gpointer         eventfdtag;
  GMutex           mutex;
  GQueue           queued;
  guint            ring_initialized : 1;
} DexUringAioContext;

DEX_DEFINE_FINAL_TYPE (DexUringAioBackend, dex_uring_aio_backend, DEX_TYPE_AIO_BACKEND)

G_GNUC_UNUSED static gboolean
dex_uring_check_kernel_version (int major,
                                int minor)
{
  static gsize initialized;
  static int kernel_major;
  static int kernel_minor;

  if (g_once_init_enter (&initialized))
    {
      static struct utsname u;

      if (uname (&u) == 0)
        {
          int maj, min;

          if (sscanf (u.release, "%u.%u.", &maj, &min) == 2)
            {
              kernel_major = maj;
              kernel_minor = min;
            }
        }

      g_once_init_leave (&initialized, TRUE);
    }

  if (kernel_major == 0)
    return FALSE;

  return kernel_major >= major ||
         (kernel_major == major && kernel_minor >= minor);
}

static gboolean
dex_uring_aio_context_dispatch (GSource     *source,
                                GSourceFunc  callback,
                                gpointer     user_data)
{
  DexUringAioContext *aio_context = (DexUringAioContext *)source;
  DexUringFuture *handledstack[32];
  struct io_uring_cqe *cqe;
  gint64 counter;
  guint n_handled;

  if (g_source_query_unix_fd (source, aio_context->eventfdtag) & G_IO_IN)
    {
      if (read (aio_context->eventfd, &counter, sizeof counter) <= 0)
        {
          /* Do mothing */
        }
    }

again:
  n_handled = 0;
  while (io_uring_peek_cqe (&aio_context->ring, &cqe) == 0)
    {
      DexUringFuture *future = io_uring_cqe_get_data (cqe);
      dex_uring_future_cqe (future, cqe);
      io_uring_cqe_seen (&aio_context->ring, cqe);
      handledstack[n_handled++] = future;

      if G_UNLIKELY (n_handled == G_N_ELEMENTS (handledstack))
        break;
    }

  for (guint i = 0; i < n_handled; i++)
    {
      DexUringFuture *future = handledstack[i];
      dex_uring_future_complete (future);
      dex_unref (future);
    }

  if G_UNLIKELY (n_handled == G_N_ELEMENTS (handledstack))
    goto again;

  return G_SOURCE_CONTINUE;
}

static gboolean
dex_uring_aio_context_prepare (GSource *source,
                               int     *timeout)
{
  DexUringAioContext *aio_context = (DexUringAioContext *)source;
  gboolean do_submit;

  g_assert (aio_context != NULL);
  g_assert (DEX_IS_URING_AIO_BACKEND (aio_context->parent.aio_backend));

  *timeout = -1;

  g_mutex_lock (&aio_context->mutex);

  do_submit = aio_context->queued.length > 0;

  while (aio_context->queued.length)
    {
      struct io_uring_sqe *sqe;
      DexUringFuture *future;

      /* Try to get the next sqe, and submit if we can't get
       * one right away. If we still fail to get an sqe, then
       * we'll wait for completions to come in to advance this.
       */
      if G_UNLIKELY (!(sqe = io_uring_get_sqe (&aio_context->ring)))
        {
          io_uring_submit (&aio_context->ring);

          if (!(sqe = io_uring_get_sqe (&aio_context->ring)))
            break;
        }

      future = g_queue_pop_head (&aio_context->queued);
      dex_uring_future_sqe (future, sqe);
      io_uring_sqe_set_data (sqe, dex_ref (future));
    }

  if (do_submit || io_uring_sq_ready (&aio_context->ring) > 0)
    io_uring_submit (&aio_context->ring);

  g_mutex_unlock (&aio_context->mutex);

  return io_uring_cq_ready (&aio_context->ring) > 0;
}

static gboolean
dex_uring_aio_context_check (GSource *source)
{
  DexUringAioContext *aio_context = (DexUringAioContext *)source;

  g_assert (aio_context != NULL);
  g_assert (DEX_IS_URING_AIO_BACKEND (aio_context->parent.aio_backend));

  return io_uring_cq_ready (&aio_context->ring) > 0;
}

static void
dex_uring_aio_context_finalize (GSource *source)
{
  DexUringAioContext *aio_context = (DexUringAioContext *)source;

  g_assert (aio_context != NULL);
  g_assert (DEX_IS_URING_AIO_BACKEND (aio_context->parent.aio_backend));

  if (aio_context->queued.length > 0)
    g_critical ("Destroying DexAioContext with queued items!");

  if (aio_context->ring_initialized)
    io_uring_queue_exit (&aio_context->ring);

  dex_clear (&aio_context->parent.aio_backend);
  g_mutex_clear (&aio_context->mutex);

  if (aio_context->eventfd != -1)
    {
      close (aio_context->eventfd);
      aio_context->eventfd = -1;
    }
}

static GSourceFuncs dex_uring_aio_context_source_funcs = {
  .check = dex_uring_aio_context_check,
  .prepare = dex_uring_aio_context_prepare,
  .dispatch = dex_uring_aio_context_dispatch,
  .finalize = dex_uring_aio_context_finalize,
};

static DexFuture *
dex_uring_aio_context_queue (DexUringAioContext *aio_context,
                             DexUringFuture     *future)
{
  gboolean is_same_thread;
  struct io_uring_sqe *sqe;

  g_assert (aio_context != NULL);
  g_assert (DEX_IS_URING_AIO_BACKEND (aio_context->parent.aio_backend));
  g_assert (DEX_IS_URING_FUTURE (future));

  is_same_thread = dex_thread_storage_get ()->aio_context == (DexAioContext *)aio_context;

  g_mutex_lock (&aio_context->mutex);
  if G_LIKELY (is_same_thread &&
               aio_context->queued.length == 0 &&
               (sqe = io_uring_get_sqe (&aio_context->ring)))
    {
      dex_uring_future_sqe (future, sqe);
      io_uring_sqe_set_data (sqe, dex_ref (future));
    }
  else
    {
      g_queue_push_tail (&aio_context->queued, dex_ref (future));
    }
  g_mutex_unlock (&aio_context->mutex);

  if (!is_same_thread)
    g_main_context_wakeup (g_source_get_context ((GSource *)aio_context));

  return DEX_FUTURE (future);
}

static DexAioContext *
dex_uring_aio_backend_create_context (DexAioBackend *aio_backend)
{
  DexUringAioContext *aio_context;
  guint uring_flags = 0;

  g_assert (DEX_IS_URING_AIO_BACKEND (aio_backend));

  aio_context = (DexUringAioContext *)
    g_source_new (&dex_uring_aio_context_source_funcs,
                  sizeof *aio_context);
  g_source_set_can_recurse ((GSource *)aio_context, TRUE);
  aio_context->parent.aio_backend = dex_ref (aio_backend);
  g_mutex_init (&aio_context->mutex);

#if DEX_URING_CHECK_VERSION(2, 2)
  if (dex_uring_check_kernel_version (5, 19))
    uring_flags |= IORING_SETUP_COOP_TASKRUN;
#endif

#if DEX_URING_CHECK_VERSION(2, 3)
  if (dex_uring_check_kernel_version (6, 0))
    uring_flags |= IORING_SETUP_SINGLE_ISSUER;
#endif

  aio_context->eventfd = -1;

  /* Setup uring submission/completion queue */
  if (io_uring_queue_init (DEFAULT_URING_SIZE, &aio_context->ring, uring_flags) != 0)
    goto failure;

  aio_context->ring_initialized = TRUE;

#if DEX_URING_CHECK_VERSION(2, 2)
  /* Register the ring FD so we don't have to on every io_ring_enter() */
  if (io_uring_register_ring_fd (&aio_context->ring) < 0)
    goto failure;
#endif

  /* Create eventfd() we can poll() on with GMainContext since GMainContext
   * knows nothing of uring and how to drive the loop using that.
   */
  if (-1 == (aio_context->eventfd = eventfd (0, EFD_CLOEXEC)) ||
      io_uring_register_eventfd (&aio_context->ring, aio_context->eventfd) != 0)
    goto failure;

  /* Add the eventfd() to our set of pollfds and keep the tag around so
   * we can check the condition directly.
   */
  aio_context->eventfdtag = g_source_add_unix_fd ((GSource *)aio_context,
                                                  aio_context->eventfd,
                                                  G_IO_IN);

  return (DexAioContext *)aio_context;

failure:
  g_source_unref ((GSource *)aio_context);

  return NULL;
}

static DexFuture *
dex_uring_aio_backend_read (DexAioBackend *aio_backend,
                            DexAioContext *aio_context,
                            int            fd,
                            gpointer       buffer,
                            gsize          count,
                            goffset        offset)
{
  return dex_uring_aio_context_queue ((DexUringAioContext *)aio_context,
                                      dex_uring_future_new_read (fd, buffer, count, offset));
}

static DexFuture *
dex_uring_aio_backend_write (DexAioBackend *aio_backend,
                             DexAioContext *aio_context,
                             int            fd,
                             gconstpointer  buffer,
                             gsize          count,
                             goffset        offset)
{
  return dex_uring_aio_context_queue ((DexUringAioContext *)aio_context,
                                      dex_uring_future_new_write (fd, buffer, count, offset));
}

static void
dex_uring_aio_backend_class_init (DexUringAioBackendClass *uring_aio_backend_class)
{
  DexAioBackendClass *aio_backend_class = DEX_AIO_BACKEND_CLASS (uring_aio_backend_class);

  aio_backend_class->create_context = dex_uring_aio_backend_create_context;
  aio_backend_class->read = dex_uring_aio_backend_read;
  aio_backend_class->write = dex_uring_aio_backend_write;
}

static void
dex_uring_aio_backend_init (DexUringAioBackend *uring_aio_backend)
{
}

DexAioBackend *
dex_uring_aio_backend_new (void)
{
  DexAioBackend *aio_backend;
  DexAioContext *aio_context;

  /* We run into a number of issues with io_uring on older kernels which
   * makes it hard to detect up-front if things will work. So just bail
   * out if we have a kernel older than 6.1.
   *
   * See https://gitlab.gnome.org/GNOME/libdex/-/issues/17
   */
  if (!dex_uring_check_kernel_version (6, 1))
    return NULL;

  aio_backend = (DexAioBackend *)dex_object_create_instance (DEX_TYPE_URING_AIO_BACKEND);

  /* Make sure we are capable of creating an aio_context */
  if (!(aio_context = dex_aio_backend_create_context (aio_backend)))
    {
      dex_unref (aio_backend);
      return NULL;
    }

  g_source_unref ((GSource *)aio_context);

  return aio_backend;
}
