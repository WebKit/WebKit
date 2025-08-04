/* dex-uring-future-private.h
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

#include <liburing.h>

#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_URING_FUTURE    (dex_uring_future_get_type())
#define DEX_URING_FUTURE(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_URING_FUTURE, DexUringFuture))
#define DEX_IS_URING_FUTURE(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_URING_FUTURE))

typedef struct _DexUringFuture DexUringFuture;

GType           dex_uring_future_get_type  (void) G_GNUC_CONST;
DexUringFuture *dex_uring_future_new_read  (int                  fd,
                                            gpointer             buffer,
                                            gsize                count,
                                            goffset              offset);
DexUringFuture *dex_uring_future_new_write (int                  fd,
                                            gconstpointer        buffer,
                                            gsize                count,
                                            goffset              offset);
void            dex_uring_future_sqe       (DexUringFuture      *uring_future,
                                            struct io_uring_sqe *sqe);
void            dex_uring_future_cqe       (DexUringFuture      *uring_future,
                                            struct io_uring_cqe *cqe);
void            dex_uring_future_complete  (DexUringFuture      *uring_future);

G_END_DECLS
