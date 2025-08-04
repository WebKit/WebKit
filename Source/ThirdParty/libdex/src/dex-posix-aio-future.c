/* dex-posix-aio-future.c
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
#include <unistd.h>

#include <gio/gio.h>

#include "dex-future-private.h"
#include "dex-posix-aio-backend-private.h"
#include "dex-posix-aio-future-private.h"

typedef enum _DexPosixAioFutureKind
{
  DEX_POSIX_AIO_FUTURE_READ = 1,
  DEX_POSIX_AIO_FUTURE_WRITE,
} DexPosixAioFutureKind;

struct _DexPosixAioFuture
{
  DexFuture              parent_instance;
  GMainContext          *main_context;
  DexPosixAioContext    *aio_context;
  DexPosixAioFutureKind  kind;
  int                    errsv;
  union {
    struct {
      int                fd;
      gpointer           buffer;
      gsize              count;
      goffset            offset;
      gssize             res;
    } read;
    struct {
      int                fd;
      gconstpointer      buffer;
      gsize              count;
      goffset            offset;
      gssize             res;
    } write;
  };
};

typedef struct _DexPosixAioFutureClass
{
  DexFutureClass parent_class;
} DexPosixAioFutureClass;

DEX_DEFINE_FINAL_TYPE (DexPosixAioFuture, dex_posix_aio_future, DEX_TYPE_FUTURE)

#undef DEX_TYPE_POSIX_AIO_FUTURE
#define DEX_TYPE_POSIX_AIO_FUTURE dex_posix_aio_future_type

static void
dex_posix_aio_future_finalize (DexObject *object)
{
  DexPosixAioFuture *posix_aio_future = DEX_POSIX_AIO_FUTURE (object);

  g_clear_pointer ((GSource **)&posix_aio_future->aio_context, g_source_unref);
  g_clear_pointer (&posix_aio_future->main_context, g_main_context_unref);

  DEX_OBJECT_CLASS (dex_posix_aio_future_parent_class)->finalize (object);
}

static void
dex_posix_aio_future_class_init (DexPosixAioFutureClass *posix_aio_future_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (posix_aio_future_class);

  object_class->finalize = dex_posix_aio_future_finalize;
}

static void
dex_posix_aio_future_init (DexPosixAioFuture *posix_aio_future)
{
}

static DexPosixAioFuture *
dex_posix_aio_future_new (DexPosixAioFutureKind  kind,
                          DexPosixAioContext    *aio_context)
{
  DexPosixAioFuture *posix_aio_future;
  GMainContext *main_context;

  if ((main_context = g_source_get_context ((GSource *)aio_context)))
    g_main_context_ref (main_context);

  posix_aio_future = (DexPosixAioFuture *)dex_object_create_instance (DEX_TYPE_POSIX_AIO_FUTURE);
  posix_aio_future->kind = kind;
  posix_aio_future->aio_context = (DexPosixAioContext *)g_source_ref ((GSource *)aio_context);
  posix_aio_future->main_context = main_context;

  return posix_aio_future;
}

DexPosixAioFuture *
dex_posix_aio_future_new_read (DexPosixAioContext *posix_aio_context,
                               int                 fd,
                               gpointer            buffer,
                               gsize               count,
                               goffset             offset)
{
  DexPosixAioFuture *posix_aio_future;

  posix_aio_future = dex_posix_aio_future_new (DEX_POSIX_AIO_FUTURE_READ, posix_aio_context);
  posix_aio_future->read.fd = fd;
  posix_aio_future->read.buffer = buffer;
  posix_aio_future->read.count = count;
  posix_aio_future->read.offset = offset;
  posix_aio_future->read.res = -1;

  return posix_aio_future;
}

DexPosixAioFuture *
dex_posix_aio_future_new_write (DexPosixAioContext *posix_aio_context,
                                int                 fd,
                                gconstpointer       buffer,
                                gsize               count,
                                goffset             offset)
{
  DexPosixAioFuture *posix_aio_future;

  posix_aio_future = dex_posix_aio_future_new (DEX_POSIX_AIO_FUTURE_WRITE, posix_aio_context);
  posix_aio_future->write.fd = fd;
  posix_aio_future->write.buffer = buffer;
  posix_aio_future->write.count = count;
  posix_aio_future->write.offset = offset;
  posix_aio_future->write.res = -1;

  return posix_aio_future;
}

void
dex_posix_aio_future_run (DexPosixAioFuture *posix_aio_future)
{
  g_return_if_fail (DEX_IS_POSIX_AIO_FUTURE (posix_aio_future));

  errno = 0;

  switch (posix_aio_future->kind)
    {
    case DEX_POSIX_AIO_FUTURE_READ:
      if (posix_aio_future->read.offset >= 0)
        posix_aio_future->read.res =
          pread (posix_aio_future->read.fd,
                 posix_aio_future->read.buffer,
                 posix_aio_future->read.count,
                 posix_aio_future->read.offset);
      else
        posix_aio_future->read.res =
          read (posix_aio_future->read.fd,
                posix_aio_future->read.buffer,
                posix_aio_future->read.count);

      /* Silence unused-result */
      (void)posix_aio_future->read.res;
      break;

    case DEX_POSIX_AIO_FUTURE_WRITE:
      if (posix_aio_future->write.offset >= 0)
        posix_aio_future->write.res =
          pwrite (posix_aio_future->write.fd,
                  posix_aio_future->write.buffer,
                  posix_aio_future->write.count,
                  posix_aio_future->write.offset);
      else
        posix_aio_future->write.res =
          write (posix_aio_future->write.fd,
                 posix_aio_future->write.buffer,
                 posix_aio_future->write.count);

      /* Silence unused-result */
      (void)posix_aio_future->write.res;
      break;

    default:
      g_assert_not_reached ();
    }

  posix_aio_future->errsv = errno;
}

static void
dex_posix_aio_future_complete_int64 (DexPosixAioFuture *posix_aio_future,
                                     gint64             res)
{
  g_assert (DEX_IS_POSIX_AIO_FUTURE (posix_aio_future));

  if (res < 0)
    dex_future_complete (DEX_FUTURE (posix_aio_future),
                         NULL,
                         g_error_new_literal (G_IO_ERROR,
                                              g_io_error_from_errno (posix_aio_future->errsv),
                                              g_strerror (posix_aio_future->errsv)));
  else
    dex_future_complete (DEX_FUTURE (posix_aio_future),
                         &(GValue) { G_TYPE_INT64, {{.v_int64 = res}}},
                         NULL);
}

void
dex_posix_aio_future_complete (DexPosixAioFuture *posix_aio_future)
{
  g_return_if_fail (DEX_IS_POSIX_AIO_FUTURE (posix_aio_future));

  switch (posix_aio_future->kind)
    {
    case DEX_POSIX_AIO_FUTURE_READ:
      dex_posix_aio_future_complete_int64 (posix_aio_future, posix_aio_future->read.res);
      break;

    case DEX_POSIX_AIO_FUTURE_WRITE:
      dex_posix_aio_future_complete_int64 (posix_aio_future, posix_aio_future->write.res);
      break;

    default:
      g_assert_not_reached ();
    }
}

DexPosixAioContext *
dex_posix_aio_future_get_aio_context (DexPosixAioFuture *posix_aio_future)
{
  g_return_val_if_fail (DEX_IS_POSIX_AIO_FUTURE (posix_aio_future), NULL);

  return posix_aio_future->aio_context;
}

GMainContext *
dex_posix_aio_future_get_main_context (DexPosixAioFuture *posix_aio_future)
{
  g_return_val_if_fail (DEX_IS_POSIX_AIO_FUTURE (posix_aio_future), NULL);

  return posix_aio_future->main_context;
}
