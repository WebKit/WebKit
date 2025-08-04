/*
 * dex-async-pair.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <gio/gio.h>

#include "dex-async-pair-private.h"
#include "dex-error.h"

DEX_DEFINE_FINAL_TYPE (DexAsyncPair, dex_async_pair, DEX_TYPE_FUTURE)

#undef DEX_TYPE_ASYNC_PAIR
#define DEX_TYPE_ASYNC_PAIR dex_async_pair_type

static void
dex_async_pair_discard (DexFuture *future)
{
  DexAsyncPair *async_pair = DEX_ASYNC_PAIR (future);

  if (async_pair->cancel_on_discard)
    g_cancellable_cancel (async_pair->cancellable);
}

static void
dex_async_pair_finalize (DexObject *object)
{
  DexAsyncPair *async_pair = DEX_ASYNC_PAIR (object);

  g_clear_object (&async_pair->instance);
  g_clear_object (&async_pair->cancellable);
  g_clear_pointer (&async_pair->info, g_free);

  DEX_OBJECT_CLASS (dex_async_pair_parent_class)->finalize (object);
}

static void
dex_async_pair_class_init (DexAsyncPairClass *async_pair_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (async_pair_class);
  DexFutureClass *future_class = DEX_FUTURE_CLASS (async_pair_class);

  object_class->finalize = dex_async_pair_finalize;

  future_class->discard = dex_async_pair_discard;
}

static void
dex_async_pair_init (DexAsyncPair *async_pair)
{
  async_pair->cancellable = g_cancellable_new ();
  async_pair->cancel_on_discard = TRUE;
}

static void
dex_async_pair_ready_callback (GObject      *object,
                               GAsyncResult *result,
                               gpointer      user_data)
{
  DexAsyncPair *async_pair = user_data;
  GValue value = G_VALUE_INIT;
  GError *error = NULL;
  GType gtype;

  g_assert (G_IS_OBJECT (object));
  g_assert (G_IS_ASYNC_RESULT (result));
  g_assert (DEX_IS_ASYNC_PAIR (async_pair));

  if (g_cancellable_is_cancelled (async_pair->cancellable))
    {
      error = g_error_new_literal (G_IO_ERROR,
                                   G_IO_ERROR_CANCELLED,
                                   "Operation cancelled");
      goto complete;
    }

#define FINISH_AS(ap, TYPE) \
  (((TYPE (*) (gpointer, GAsyncResult*, GError**))ap->info->finish) (ap->instance, result, &error))

  gtype = async_pair->info->return_type;

  switch (gtype)
    {
    case G_TYPE_BOOLEAN:
      g_value_init (&value, G_TYPE_BOOLEAN);
      g_value_set_boolean (&value, FINISH_AS (async_pair, gboolean));
      break;

    case G_TYPE_INT:
      g_value_init (&value, G_TYPE_INT);
      g_value_set_int (&value, FINISH_AS (async_pair, int));
      break;

    case G_TYPE_UINT:
      g_value_init (&value, G_TYPE_UINT);
      g_value_set_uint (&value, FINISH_AS (async_pair, guint));
      break;

    case G_TYPE_INT64:
      g_value_init (&value, G_TYPE_INT64);
      g_value_set_int64 (&value, FINISH_AS (async_pair, gint64));
      break;

    case G_TYPE_UINT64:
      g_value_init (&value, G_TYPE_UINT64);
      g_value_set_uint64 (&value, FINISH_AS (async_pair, guint64));
      break;

    case G_TYPE_LONG:
      g_value_init (&value, G_TYPE_LONG);
      g_value_set_long (&value, FINISH_AS (async_pair, glong));
      break;

    case G_TYPE_ULONG:
      g_value_init (&value, G_TYPE_ULONG);
      g_value_set_ulong (&value, FINISH_AS (async_pair, gulong));
      break;

    case G_TYPE_STRING:
      g_value_init (&value, G_TYPE_STRING);
      g_value_take_string (&value, FINISH_AS (async_pair, char *));
      break;

    case G_TYPE_POINTER:
      g_value_init (&value, G_TYPE_POINTER);
      g_value_set_pointer (&value, FINISH_AS (async_pair, gpointer));
      break;

    case G_TYPE_OBJECT:
      g_value_init (&value, G_TYPE_OBJECT);
      g_value_take_object (&value, FINISH_AS (async_pair, gpointer));
      break;

    case G_TYPE_ENUM:
    case G_TYPE_FLAGS:
    default:
      if (gtype == G_TYPE_ENUM || g_type_is_a (gtype, G_TYPE_ENUM))
        {
          g_value_init (&value, gtype);
          g_value_set_enum (&value, FINISH_AS (async_pair, guint));
        }
      else if (gtype == G_TYPE_FLAGS || g_type_is_a (gtype, G_TYPE_FLAGS))
        {
          g_value_init (&value, gtype);
          g_value_set_flags (&value, FINISH_AS (async_pair, guint));
        }
      else if (g_type_is_a (gtype, G_TYPE_BOXED))
        {
          g_value_init (&value, gtype);
          g_value_take_boxed (&value, FINISH_AS (async_pair, gpointer));
        }
      else
        {
          error = g_error_new (DEX_ERROR,
                               DEX_ERROR_TYPE_NOT_SUPPORTED,
                               "Type '%s' is not currently supported by DexAsyncPair!",
                               g_type_name (async_pair->info->return_type));
        }
      break;
    }

#undef FINISH_AS

complete:
  if (error != NULL)
    dex_future_complete (DEX_FUTURE (async_pair), NULL, g_steal_pointer (&error));
  else
    dex_future_complete (DEX_FUTURE (async_pair), &value, NULL);

  g_value_unset (&value);
  dex_clear (&async_pair);
}

DexFuture *
dex_async_pair_new (gpointer                instance,
                    const DexAsyncPairInfo *info)
{
  void (*async_func) (gpointer, GCancellable*, GAsyncReadyCallback, gpointer);
  DexAsyncPair *async_pair;

  g_return_val_if_fail (!instance || G_IS_OBJECT (instance), NULL);
  g_return_val_if_fail (info != NULL, NULL);

  async_func = info->async;

  async_pair = (DexAsyncPair *)dex_object_create_instance (DEX_TYPE_ASYNC_PAIR);
  async_pair->info = g_memdup2 (info, sizeof *info);
  g_set_object (&async_pair->instance, instance);

  async_func (async_pair->instance,
              async_pair->cancellable,
              dex_async_pair_ready_callback,
              dex_ref (async_pair));

  return DEX_FUTURE (async_pair);
}

/**
 * dex_async_pair_get_cancellable:
 * @async_pair: a #DexAsyncPair
 *
 * Gets the cancellable for the async pair.
 *
 * If the DexAsyncPair is discarded by its callers, then it will automatically
 * be cancelled using g_cancellable_cancel().
 *
 * Returns: (transfer none): a #GCancellable
 */
GCancellable *
dex_async_pair_get_cancellable (DexAsyncPair *async_pair)
{
  g_return_val_if_fail (DEX_IS_ASYNC_PAIR (async_pair), NULL);

  return async_pair->cancellable;
}

/**
 * dex_async_pair_return_object:
 * @async_pair: a #DexAsyncPair
 * @instance: (type GObject) (transfer full): a #GObject
 *
 * Resolves @async_pair with a value of @instance.
 *
 * This function is meant to be used when manually wrapping
 * various #GAsyncReadyCallback based API.
 *
 * The ownership of @instance is taken when calling this function.
 */
void
dex_async_pair_return_object (DexAsyncPair *async_pair,
                              gpointer      instance)
{
  GValue value = G_VALUE_INIT;

  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));
  g_return_if_fail (G_IS_OBJECT (instance));

  g_value_init (&value, G_OBJECT_TYPE (instance));
  g_value_take_object (&value, instance);
  dex_future_complete (DEX_FUTURE (async_pair), &value, NULL);
  g_value_unset (&value);
}

/**
 * dex_async_pair_return_error:
 * @async_pair: a #DexAsyncPair
 * @error: (transfer full): a #GError
 *
 * Rejects @async_pair with @error.
 *
 * This function is meant to be used when manually wrapping
 * various #GAsyncReadyCallback based API.
 *
 * The ownership of @error is taken when calling this function.
 */
void
dex_async_pair_return_error (DexAsyncPair *async_pair,
                             GError       *error)
{
  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));
  g_return_if_fail (error != NULL);

  dex_future_complete (DEX_FUTURE (async_pair), NULL, error);
}

void
dex_async_pair_return_int64 (DexAsyncPair *async_pair,
                             gint64        value)
{
  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));

  dex_future_complete (DEX_FUTURE (async_pair), &(GValue) {G_TYPE_INT64, {{.v_int64 = value}}}, NULL);
}

void
dex_async_pair_return_uint64 (DexAsyncPair *async_pair,
                              guint64       value)
{
  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));

  dex_future_complete (DEX_FUTURE (async_pair), &(GValue) {G_TYPE_UINT64, {{.v_uint64 = value}}}, NULL);
}

void
dex_async_pair_return_boolean (DexAsyncPair *async_pair,
                               gboolean      value)
{
  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));

  dex_future_complete (DEX_FUTURE (async_pair), &(GValue) {G_TYPE_BOOLEAN, {{.v_int = value}}}, NULL);
}

/**
 * dex_async_pair_return_string:
 * @async_pair: a #DexAsyncPair
 * @value: (transfer full) (nullable): a string or %NULL
 *
 * Resolves @async_pair with @value.
 */
void
dex_async_pair_return_string (DexAsyncPair *async_pair,
                              char         *value)
{
  GValue gvalue = G_VALUE_INIT;

  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));

  g_value_init (&gvalue, G_TYPE_STRING);
  g_value_take_string (&gvalue, value);
  dex_future_complete (DEX_FUTURE (async_pair), &gvalue, NULL);
  g_value_unset (&gvalue);
}

/**
 * dex_async_pair_return_boxed: (skip)
 * @async_pair: a #DexAsyncPair
 * @boxed_type: a #GType of %G_TYPE_BOXED
 * @instance: (transfer full): the boxed value to store
 *
 * Resolves @async_pair with @instance.
 */
void
dex_async_pair_return_boxed (DexAsyncPair *async_pair,
                             GType         boxed_type,
                             gpointer      instance)
{
  GValue gvalue = G_VALUE_INIT;

  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));
  g_return_if_fail (g_type_is_a (boxed_type, G_TYPE_BOXED));

  g_value_init (&gvalue, boxed_type);
  g_value_take_boxed (&gvalue, instance);
  dex_future_complete (DEX_FUTURE (async_pair), &gvalue, NULL);
  g_value_unset (&gvalue);
}

/**
 * dex_async_pair_return_variant:
 * @async_pair: a #DexAsyncPair
 * @variant: (transfer full): the variant to resolve with
 *
 * Resolves @async_pair with @variant.
 */
void
dex_async_pair_return_variant (DexAsyncPair *async_pair,
                               GVariant     *variant)
{
  GValue gvalue = G_VALUE_INIT;

  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));

  g_value_init (&gvalue, G_TYPE_VARIANT);
  g_value_take_variant (&gvalue, variant);
  dex_future_complete (DEX_FUTURE (async_pair), &gvalue, NULL);
  g_value_unset (&gvalue);
}

/**
 * dex_async_pair_set_cancel_on_discard:
 * @async_pair: a #DexAsyncPair
 * @cancel_on_discard: if the operation should cancel when the future is discarded
 *
 * Sets whether or not the future should cancel the async operation when
 * the future is discarded. This happens when no more futures are awaiting
 * the completion of this future.
 *
 * Since: 0.4
 */
void
dex_async_pair_set_cancel_on_discard (DexAsyncPair *async_pair,
                                      gboolean      cancel_on_discard)
{
  g_return_if_fail (DEX_IS_ASYNC_PAIR (async_pair));

  dex_object_lock (async_pair);
  async_pair->cancel_on_discard = !!cancel_on_discard;
  dex_object_unlock (async_pair);
}
