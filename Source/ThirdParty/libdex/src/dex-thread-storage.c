/*
 * dex-thread-storage.c
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

#include "dex-thread-storage-private.h"

static GPrivate dex_thread_storage_key = G_PRIVATE_INIT (g_free);

DexThreadStorage *
dex_thread_storage_get (void)
{
  DexThreadStorage *ret = g_private_get (&dex_thread_storage_key);

  if G_UNLIKELY (ret == NULL)
    {
      ret = g_new0 (DexThreadStorage, 1);
      g_private_set (&dex_thread_storage_key, ret);
    }

  return ret;
}
