/* dex-posix-aio-future-private.h
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

#include "dex-aio-backend-private.h"

G_BEGIN_DECLS

#define DEX_TYPE_POSIX_AIO_FUTURE    (dex_posix_aio_future_get_type())
#define DEX_POSIX_AIO_FUTURE(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_POSIX_AIO_FUTURE, DexPosixAioFuture))
#define DEX_IS_POSIX_AIO_FUTURE(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_POSIX_AIO_FUTURE))

GType               dex_posix_aio_future_get_type         (void) G_GNUC_CONST;
DexPosixAioFuture  *dex_posix_aio_future_new_read         (DexPosixAioContext  *posix_aio_context,
                                                           int                  fd,
                                                           gpointer             buffer,
                                                           gsize                count,
                                                           goffset              offset);
DexPosixAioFuture  *dex_posix_aio_future_new_write        (DexPosixAioContext  *posix_aio_context,
                                                           int                  fd,
                                                           gconstpointer        buffer,
                                                           gsize                count,
                                                           goffset              offset);
void                dex_posix_aio_future_run              (DexPosixAioFuture   *posix_aio_future);
void                dex_posix_aio_future_complete         (DexPosixAioFuture   *posix_aio_future);
DexPosixAioContext *dex_posix_aio_future_get_aio_context  (DexPosixAioFuture *posix_aio_future);
GMainContext       *dex_posix_aio_future_get_main_context (DexPosixAioFuture *posix_aio_future);

G_END_DECLS
