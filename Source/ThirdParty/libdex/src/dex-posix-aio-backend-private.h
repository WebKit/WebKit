/* dex-posix-aio-backend-private.h
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

#define DEX_TYPE_POSIX_AIO_BACKEND    (dex_posix_aio_backend_get_type())
#define DEX_POSIX_AIO_BACKEND(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_POSIX_AIO_BACKEND, DexPosixAioBackend))
#define DEX_IS_POSIX_AIO_BACKEND(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_POSIX_AIO_BACKEND))

typedef struct _DexPosixAioBackend      DexPosixAioBackend;
typedef struct _DexPosixAioContext      DexPosixAioContext;
typedef struct _DexPosixAioFuture       DexPosixAioFuture;
typedef struct _DexPosixAioBackendClass DexPosixAioBackendClass;

GType          dex_posix_aio_backend_get_type (void) G_GNUC_CONST;
DexAioBackend *dex_posix_aio_backend_new      (void);
void           dex_posix_aio_context_enqueue  (DexPosixAioContext *posix_aio_context,
                                               DexPosixAioFuture  *posix_aio_future);

G_END_DECLS
