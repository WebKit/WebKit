/*
 * dex-infinite.c
 *
 * Copyright 2023 Christian Hergert <chergert@redhat.com>
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

#include "dex-infinite-private.h"

typedef struct _DexInfinite
{
  DexFuture parent_instance;
} DexInfinite;

typedef struct _DexInfiniteClass
{
  DexFutureClass parent_class;
} DexInfiniteClass;

DEX_DEFINE_FINAL_TYPE (DexInfinite, dex_infinite, DEX_TYPE_FUTURE)

#undef DEX_TYPE_INFINITE
#define DEX_TYPE_INFINITE dex_infinite_type

static void
dex_infinite_discard (DexFuture *future)
{
}

static gboolean
dex_infinite_propagate (DexFuture *future,
                        DexFuture *completed)
{
  g_return_val_if_reached (FALSE);
}

static void
dex_infinite_class_init (DexInfiniteClass *infinite_class)
{
  DexFutureClass *future_class = DEX_FUTURE_CLASS (infinite_class);

  future_class->propagate = dex_infinite_propagate;
  future_class->discard = dex_infinite_discard;
}

static void
dex_infinite_init (DexInfinite *infinite)
{
}

DexFuture *
dex_infinite_new (void)
{
  return (DexFuture *)dex_object_create_instance (dex_infinite_type);
}
