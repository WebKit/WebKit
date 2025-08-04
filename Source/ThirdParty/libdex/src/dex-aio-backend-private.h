/* dex-aio-backend-private.h
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

#include "dex-object-private.h"
#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_AIO_BACKEND           (dex_aio_backend_get_type())
#define DEX_AIO_BACKEND(object)        (G_TYPE_CHECK_INSTANCE_CAST(object, DEX_TYPE_AIO_BACKEND, DexAioBackend))
#define DEX_IS_AIO_BACKEND(object)     (G_TYPE_CHECK_INSTANCE_TYPE(object, DEX_TYPE_AIO_BACKEND))
#define DEX_AIO_BACKEND_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST(klass, DEX_TYPE_AIO_BACKEND, DexAioBackendClass))
#define DEX_AIO_BACKEND_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS(obj, DEX_TYPE_AIO_BACKEND, DexAioBackendClass))

typedef struct _DexAioBackend      DexAioBackend;
typedef struct _DexAioBackendClass DexAioBackendClass;
typedef struct _DexAioContext      DexAioContext;

struct _DexAioBackend
{
  DexObject parent_instance;
};

struct _DexAioBackendClass
{
  DexObjectClass parent_class;

  DexAioContext *(*create_context) (DexAioBackend *aio_backend);
  DexFuture     *(*read)           (DexAioBackend *aio_backend,
                                    DexAioContext *aio_context,
                                    int            fd,
                                    gpointer       buffer,
                                    gsize          count,
                                    goffset        offset);
  DexFuture     *(*write)          (DexAioBackend *aio_backend,
                                    DexAioContext *aio_context,
                                    int            fd,
                                    gconstpointer  buffer,
                                    gsize          count,
                                    goffset        offset);
};

struct _DexAioContext
{
  GSource        parent_source;
  DexAioBackend *aio_backend;
  /*< private >*/
};

GType          dex_aio_backend_get_type       (void) G_GNUC_CONST;
DexAioBackend *dex_aio_backend_get_default    (void);
DexAioContext *dex_aio_backend_create_context (DexAioBackend *aio_backend);
DexFuture     *dex_aio_backend_read           (DexAioBackend *aio_backend,
                                               DexAioContext *aio_context,
                                               int            fd,
                                               gpointer       buffer,
                                               gsize          count,
                                               goffset        offset);
DexFuture     *dex_aio_backend_write          (DexAioBackend *aio_backend,
                                               DexAioContext *aio_context,
                                               int            fd,
                                               gconstpointer  buffer,
                                               gsize          count,
                                               goffset        offset);

G_END_DECLS
