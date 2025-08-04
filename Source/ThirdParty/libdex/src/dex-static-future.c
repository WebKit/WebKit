/* dex-static-future.c
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

#include "dex-future-private.h"
#include "dex-static-future-private.h"

/**
 * DexStaticFuture:
 *
 * `DexStaticFuture` represents a future that is resolved from the initial
 * state.
 *
 * Use this when you need to create a future for API reasons but already have
 * the value or rejection at that point.
 *
 * #DexStaticFuture is used internally by functions like
 * dex_future_new_for_boolean() and similar.
 */

struct _DexStaticFuture
{
  DexFuture parent_instance;
};

typedef struct _DexStaticFutureClass
{
  DexFutureClass parent_class;
} DexStaticFutureClass;

DEX_DEFINE_FINAL_TYPE (DexStaticFuture, dex_static_future, DEX_TYPE_FUTURE)

#undef DEX_TYPE_STATIC_FUTURE
#define DEX_TYPE_STATIC_FUTURE dex_static_future_type

static void
dex_static_future_class_init (DexStaticFutureClass *static_future_class)
{
}

static void
dex_static_future_init (DexStaticFuture *static_future)
{
}

DexFuture *
dex_static_future_new_rejected (GError *error)
{
  DexFuture *ret;

  g_return_val_if_fail (error != NULL, NULL);

  ret = (DexFuture *)dex_object_create_instance (DEX_TYPE_STATIC_FUTURE);
  ret->rejected = error;
  ret->status = DEX_FUTURE_STATUS_REJECTED;

  return DEX_FUTURE (ret);
}

DexFuture *
dex_static_future_new_resolved (const GValue *value)
{
  DexFuture *ret;

  g_return_val_if_fail (G_IS_VALUE (value), NULL);

  ret = (DexFuture *)dex_object_create_instance (DEX_TYPE_STATIC_FUTURE);
  g_value_init (&ret->resolved, G_VALUE_TYPE (value));
  g_value_copy (value, &ret->resolved);
  ret->status = DEX_FUTURE_STATUS_RESOLVED;

  return DEX_FUTURE (ret);
}
