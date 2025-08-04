/* dex-uring-future.c
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

#include <liburing.h>

#include <gio/gio.h>

#include "dex-future-private.h"
#include "dex-uring-future-private.h"

typedef enum _DexUringType
{
  DEX_URING_TYPE_READ = 1,
  DEX_URING_TYPE_WRITE,
} DexUringType;

struct _DexUringFuture
{
  DexFuture parent_instance;
  DexUringType type;
  union {
    struct {
      int fd;
      gpointer buffer;
      gsize count;
      goffset offset;
      gssize result;
    } read;
    struct {
      int fd;
      gconstpointer buffer;
      gsize count;
      goffset offset;
      gssize result;
    } write;
  };
};

typedef struct _DexUringFutureClass
{
  DexFutureClass parent_class;
} DexUringFutureClass;

DEX_DEFINE_FINAL_TYPE (DexUringFuture, dex_uring_future, DEX_TYPE_FUTURE)

#undef DEX_TYPE_URING_FUTURE
#define DEX_TYPE_URING_FUTURE dex_uring_future_type

static void
dex_uring_future_class_init (DexUringFutureClass *uring_future_class)
{
}

static void
dex_uring_future_init (DexUringFuture *uring_future)
{
}

static GError *
create_error (int error_code)
{
  return g_error_new_literal (G_IO_ERROR,
                              g_io_error_from_errno (-error_code),
                              g_strerror (-error_code));
}

static void
complete_ssize (DexUringFuture *uring_future,
                gssize          value)
{
  if (value < 0)
    dex_future_complete (DEX_FUTURE (uring_future),
                         NULL,
                         create_error (-value));
  else
    dex_future_complete (DEX_FUTURE (uring_future),
                         &(GValue) { G_TYPE_INT64, {{.v_int64 = value}}},
                         NULL);
}

void
dex_uring_future_complete (DexUringFuture *uring_future)
{
  switch (uring_future->type)
    {
    case DEX_URING_TYPE_READ:
      complete_ssize (uring_future, uring_future->read.result);
      break;

    case DEX_URING_TYPE_WRITE:
      complete_ssize (uring_future, uring_future->write.result);
      break;

    default:
      g_assert_not_reached ();
    }
}

void
dex_uring_future_cqe (DexUringFuture      *uring_future,
                      struct io_uring_cqe *cqe)
{
  switch (uring_future->type)
    {
    case DEX_URING_TYPE_READ:
      uring_future->read.result = cqe->res;
      break;

    case DEX_URING_TYPE_WRITE:
      uring_future->write.result = cqe->res;
      break;

    default:
      g_assert_not_reached ();
    }
}

void
dex_uring_future_sqe (DexUringFuture      *uring_future,
                      struct io_uring_sqe *sqe)
{
  switch (uring_future->type)
    {
    case DEX_URING_TYPE_READ:
      io_uring_prep_read (sqe,
                          uring_future->read.fd,
                          uring_future->read.buffer,
                          uring_future->read.count,
                          uring_future->read.offset);
      break;

    case DEX_URING_TYPE_WRITE:
      io_uring_prep_write (sqe,
                           uring_future->write.fd,
                           uring_future->write.buffer,
                           uring_future->write.count,
                           uring_future->write.offset);
      break;

    default:
      g_assert_not_reached ();
    }
}

DexUringFuture *
dex_uring_future_new_read (int      fd,
                           gpointer buffer,
                           gsize    count,
                           goffset  offset)
{
  DexUringFuture *future;

  future = (DexUringFuture *)dex_object_create_instance (DEX_TYPE_URING_FUTURE);
  future->type = DEX_URING_TYPE_READ;
  future->read.fd = fd;
  future->read.buffer = buffer;
  future->read.count = count;
  future->read.offset = offset;

  return future;
}

DexUringFuture *
dex_uring_future_new_write (int           fd,
                            gconstpointer buffer,
                            gsize         count,
                            goffset       offset)
{
  DexUringFuture *future;

  future = (DexUringFuture *)dex_object_create_instance (DEX_TYPE_URING_FUTURE);
  future->type = DEX_URING_TYPE_WRITE;
  future->write.fd = fd;
  future->write.buffer = buffer;
  future->write.count = count;
  future->write.offset = offset;

  return future;
}
