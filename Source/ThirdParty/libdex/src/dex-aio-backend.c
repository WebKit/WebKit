/* dex-aio-backend.c
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

#include "dex-aio-backend-private.h"

#ifdef HAVE_LIBURING
# include "dex-uring-aio-backend-private.h"
#endif

#include "dex-posix-aio-backend-private.h"

DEX_DEFINE_ABSTRACT_TYPE (DexAioBackend, dex_aio_backend, DEX_TYPE_OBJECT)

static void
dex_aio_backend_class_init (DexAioBackendClass *aio_backend_class)
{
}

static void
dex_aio_backend_init (DexAioBackend *aio_backend)
{
}

DexAioContext *
dex_aio_backend_create_context (DexAioBackend *aio_backend)
{
  g_return_val_if_fail (DEX_IS_AIO_BACKEND (aio_backend), NULL);

  return DEX_AIO_BACKEND_GET_CLASS (aio_backend)->create_context (aio_backend);
}

DexFuture *
dex_aio_backend_read (DexAioBackend *aio_backend,
                      DexAioContext *aio_context,
                      int            fd,
                      gpointer       buffer,
                      gsize          count,
                      goffset        offset)
{
  g_return_val_if_fail (DEX_IS_AIO_BACKEND (aio_backend), NULL);
  g_return_val_if_fail (aio_context != NULL, NULL);

  return DEX_AIO_BACKEND_GET_CLASS (aio_backend)->read (aio_backend, aio_context, fd, buffer, count, offset);
}

DexFuture *
dex_aio_backend_write (DexAioBackend *aio_backend,
                       DexAioContext *aio_context,
                       int            fd,
                       gconstpointer  buffer,
                       gsize          count,
                       goffset        offset)
{
  g_return_val_if_fail (DEX_IS_AIO_BACKEND (aio_backend), NULL);
  g_return_val_if_fail (aio_context != NULL, NULL);

  return DEX_AIO_BACKEND_GET_CLASS (aio_backend)->write (aio_backend, aio_context, fd, buffer, count, offset);
}

DexAioBackend *
dex_aio_backend_get_default (void)
{
  static DexAioBackend *instance;

  if (g_once_init_enter (&instance))
    {
      DexAioBackend *backend = NULL;

#if defined(HAVE_LIBURING)
      backend = dex_uring_aio_backend_new ();
#endif

      if (backend == NULL)
        backend = dex_posix_aio_backend_new ();

      g_debug ("Using AIO backend %s",
               DEX_OBJECT_TYPE_NAME (backend));

      g_once_init_leave (&instance, backend);
    }

  return instance;
}
