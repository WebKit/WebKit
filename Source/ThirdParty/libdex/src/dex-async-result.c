/*
 * dex-async-result.c
 *
 * Copyright 2022-2023 Christian Hergert <>
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

#include "dex-async-result.h"
#include "dex-cancellable.h"
#include "dex-compat-private.h"
#include "dex-error.h"

/**
 * DexAsyncResult:
 *
 * `DexAsyncResult` is used to integrate a `DexFuture` with `GAsyncResult`.
 *
 * Use this class when you need to expose the traditional async/finish
 * behavior of `GAsyncResult`.
 */

struct _DexAsyncResult
{
  GObject              parent_instance;
  GMutex               mutex;
  GMainContext        *main_context;
  gpointer             source_object;
  GCancellable        *cancellable;
  GAsyncReadyCallback  callback;
  gpointer             user_data;
  gpointer             tag;
  DexFuture           *future;
  char                *name;
  int                  priority;
  guint                name_is_static : 1;
  guint                await_once : 1;
  guint                await_returned : 1;
};

static gboolean
dex_async_result_is_tagged (GAsyncResult *async_result,
                            gpointer      tag)
{
  DexAsyncResult *self = DEX_ASYNC_RESULT (async_result);

  return self->tag == tag;
}

static gpointer
dex_async_result_get_user_data (GAsyncResult *async_result)
{
  DexAsyncResult *self = DEX_ASYNC_RESULT (async_result);

  return self->user_data;
}

static GObject *
dex_async_result_get_source_object (GAsyncResult *async_result)
{
  DexAsyncResult *self = DEX_ASYNC_RESULT (async_result);

  return self->source_object ? g_object_ref (self->source_object) : NULL;
}

static void
async_result_iface_init (GAsyncResultIface *iface)
{
  iface->get_user_data = dex_async_result_get_user_data;
  iface->get_source_object = dex_async_result_get_source_object;
  iface->is_tagged = dex_async_result_is_tagged;
}

G_DEFINE_FINAL_TYPE_WITH_CODE (DexAsyncResult, dex_async_result, G_TYPE_OBJECT,
                               G_IMPLEMENT_INTERFACE (G_TYPE_ASYNC_RESULT, async_result_iface_init))

DexAsyncResult *
dex_async_result_new (gpointer             source_object,
                      GCancellable        *cancellable,
                      GAsyncReadyCallback  callback,
                      gpointer             user_data)
{
  DexAsyncResult *self;

  g_return_val_if_fail (!source_object || G_IS_OBJECT (source_object), NULL);

  self = g_object_new (DEX_TYPE_ASYNC_RESULT, NULL);
  self->user_data = user_data;
  self->callback = callback;
  g_set_object (&self->source_object, source_object);
  g_set_object (&self->cancellable, cancellable);
  self->main_context = g_main_context_ref_thread_default ();

  return self;
}

static void
dex_async_result_finalize (GObject *object)
{
  DexAsyncResult *self = (DexAsyncResult *)object;

  dex_clear (&self->future);
  g_clear_object (&self->source_object);
  g_clear_object (&self->cancellable);
  g_clear_pointer (&self->main_context, g_main_context_unref);

  if (!self->name_is_static)
    g_clear_pointer (&self->name, g_free);

  g_mutex_clear (&self->mutex);

  G_OBJECT_CLASS (dex_async_result_parent_class)->finalize (object);
}

static void
dex_async_result_class_init (DexAsyncResultClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);

  object_class->finalize = dex_async_result_finalize;
}

static void
dex_async_result_init (DexAsyncResult *self)
{
  g_mutex_init (&self->mutex);
}

const char *
dex_async_result_get_name (DexAsyncResult *self)
{
  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (self), NULL);

  return self->name;
}

void
dex_async_result_set_name (DexAsyncResult *async_result,
                           const char     *name)
{
  g_return_if_fail (DEX_IS_ASYNC_RESULT (async_result));

  g_mutex_lock (&async_result->mutex);
  if (async_result->name == NULL)
    async_result->name = g_strdup (name);
  g_mutex_unlock (&async_result->mutex);
}

void
dex_async_result_set_static_name (DexAsyncResult *async_result,
                                  const char     *name)
{
  DexAsyncResult *self = DEX_ASYNC_RESULT (async_result);

  g_return_if_fail (DEX_IS_ASYNC_RESULT (self));

  g_mutex_lock (&self->mutex);
  if (self->name == NULL)
    {
      self->name = (gpointer)name;
      self->name_is_static = TRUE;
    }
  g_mutex_unlock (&self->mutex);
}

/**
 * dex_async_result_dup_future:
 * @async_result: a #DexAsyncResult
 *
 * Gets the future for the #DexAsyncResult, or %NULL if a future
 * is not available.
 *
 * Returns: (transfer full) (nullable): a #DexFuture or %NULL
 */
DexFuture *
dex_async_result_dup_future (DexAsyncResult *async_result)
{
  DexFuture *future = NULL;

  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (async_result), NULL);

  g_mutex_lock (&async_result->mutex);
  if (async_result->future != NULL)
    future = dex_ref (async_result->future);
  g_mutex_unlock (&async_result->mutex);

  return g_steal_pointer (&future);
}

static const GValue *
dex_async_result_propagate (DexAsyncResult  *async_result,
                            GError         **error)
{
  const GValue *value = NULL;
  DexFuture *future;

  g_assert (DEX_IS_ASYNC_RESULT (async_result));

  if ((future = dex_async_result_dup_future (async_result)))
    {
      value = dex_future_get_value (future, error);
      dex_unref (future);
      return value;
    }

  g_set_error (error,
               DEX_ERROR,
               DEX_ERROR_PENDING,
               "Future pending");

  return NULL;
}

gpointer
dex_async_result_propagate_pointer (DexAsyncResult  *async_result,
                                    GError         **error)
{
  const GValue *value;

  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (async_result), NULL);

  if ((value = dex_async_result_propagate (async_result, error)))
    {
      if (G_VALUE_HOLDS_OBJECT (value))
        return g_value_dup_object (value);
      else if (G_VALUE_HOLDS_BOXED (value))
        return g_value_dup_boxed (value);
      else if (G_VALUE_HOLDS_VARIANT (value))
        return g_value_dup_variant (value);
      else if (G_VALUE_HOLDS_POINTER (value))
        return g_value_get_pointer (value);
      else
        g_critical ("Cannot propagate pointer of type %s",
                    G_VALUE_TYPE_NAME (value));
    }

  return NULL;
}

gssize
dex_async_result_propagate_int (DexAsyncResult  *async_result,
                                GError         **error)
{
  const GValue *value;

  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (async_result), 0);

  if ((value = dex_async_result_propagate (async_result, error)))
    {
      if (G_VALUE_HOLDS_INT (value))
        return g_value_get_int (value);
      else if (G_VALUE_HOLDS_UINT (value))
        return g_value_get_uint (value);
      else if (G_VALUE_HOLDS_INT64 (value))
        return g_value_get_int64 (value);
      else if (G_VALUE_HOLDS_UINT64 (value))
        return g_value_get_uint64 (value);
      else if (G_VALUE_HOLDS_LONG (value))
        return g_value_get_long (value);
      else if (G_VALUE_HOLDS_ULONG (value))
        return g_value_get_ulong (value);
      else
        g_critical ("Cannot propagate int from type %s",
                    G_VALUE_TYPE_NAME (value));
    }

  return 0;
}

double
dex_async_result_propagate_double (DexAsyncResult  *async_result,
                                   GError         **error)
{
  const GValue *value;

  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (async_result), 0);

  if ((value = dex_async_result_propagate (async_result, error)))
    {
      if (G_VALUE_HOLDS_DOUBLE (value))
        return g_value_get_double (value);
      else if (G_VALUE_HOLDS_FLOAT (value))
        return g_value_get_float (value);
    }

  return .0;
}

gboolean
dex_async_result_propagate_boolean (DexAsyncResult  *async_result,
                                    GError         **error)
{
  const GValue *value;

  g_return_val_if_fail (DEX_IS_ASYNC_RESULT (async_result), FALSE);

  if ((value = dex_async_result_propagate (async_result, error)))
    {
      if (G_VALUE_HOLDS_BOOLEAN (value))
        return g_value_get_boolean (value);

      g_critical ("%s() got future of type %s, expected gboolean",
                  G_STRFUNC, G_VALUE_TYPE_NAME (value));

      return FALSE;
    }

  return FALSE;
}

static gboolean
dex_async_result_complete_in_idle_cb (gpointer user_data)
{
  DexAsyncResult *async_result = user_data;

  g_assert (DEX_IS_ASYNC_RESULT (async_result));

  async_result->callback (async_result->source_object,
                          G_ASYNC_RESULT (async_result),
                          g_steal_pointer (&async_result->user_data));

  dex_clear (&async_result->future);

  return G_SOURCE_REMOVE;
}

static DexFuture *
dex_async_result_await_cb (DexFuture *future,
                           gpointer   user_data)
{
  DexAsyncResult *async_result = user_data;

  g_assert (DEX_IS_FUTURE (future));
  g_assert (DEX_IS_ASYNC_RESULT (async_result));
  g_assert (async_result->await_once);
  g_assert (async_result->main_context != NULL);

  g_mutex_lock (&async_result->mutex);

  g_assert (!async_result->await_returned);

  if (async_result->callback != NULL)
    {
      GSource *idle_source = g_idle_source_new ();

      g_source_set_priority (idle_source, async_result->priority);
      g_source_set_callback (idle_source,
                             dex_async_result_complete_in_idle_cb,
                             g_object_ref (async_result),
                             g_object_unref);

      if (async_result->name_is_static)
        _g_source_set_static_name (idle_source, async_result->name);
      else
        g_source_set_name (idle_source, async_result->name);

      g_source_attach (idle_source, async_result->main_context);
      g_source_unref (idle_source);
    }

  async_result->await_returned = TRUE;

  g_mutex_unlock (&async_result->mutex);

  return NULL;
}

/**
 * dex_async_result_await:
 * @async_result: a #GAsyncResult
 * @future: (transfer full): a #DexFuture
 *
 * Tracks the result of @future and uses the value to complete @async_result,
 * eventually calling the registered #GAsyncReadyCallback.
 */
void
dex_async_result_await (DexAsyncResult *async_result,
                        DexFuture      *future)
{
  DexFuture *new_future = NULL;
  DexFuture *cancellable;

  g_return_if_fail (DEX_IS_ASYNC_RESULT (async_result));
  g_return_if_fail (DEX_IS_FUTURE (future));

  g_mutex_lock (&async_result->mutex);
  if G_UNLIKELY (async_result->await_once != FALSE)
    {
      g_mutex_unlock (&async_result->mutex);
      g_critical ("%s() called more than once on %s @ %p [%s]",
                  G_STRFUNC, G_OBJECT_TYPE_NAME (async_result),
                  async_result,
                  async_result->name ? async_result->name : "unnamed task");
      return;
    }
  async_result->await_once = TRUE;
  g_mutex_unlock (&async_result->mutex);

  if (async_result->cancellable == NULL)
    cancellable = NULL;
  else
    cancellable = dex_cancellable_new_from_cancellable (async_result->cancellable);

  /* We have to be careful about ownership here and ensure we
   * drop our references when the callback is executed.
   */
  g_object_ref (async_result);
  new_future = dex_future_finally (dex_future_first (future, cancellable, NULL),
                                   dex_async_result_await_cb,
                                   g_object_ref (async_result),
                                   g_object_unref);
  g_mutex_lock (&async_result->mutex);
  g_assert (async_result->future == NULL);
  async_result->future = g_steal_pointer (&new_future);
  g_mutex_unlock (&async_result->mutex);
  g_object_unref (async_result);
}

void
dex_async_result_set_priority (DexAsyncResult *async_result,
                               int             priority)
{
  g_return_if_fail (DEX_IS_ASYNC_RESULT (async_result));

  g_mutex_lock (&async_result->mutex);
  async_result->priority = priority;
  g_mutex_unlock (&async_result->mutex);
}
