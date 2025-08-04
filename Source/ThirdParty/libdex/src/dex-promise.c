/*
 * dex-promise.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@gnome.org>
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

#include "dex-future-private.h"
#include "dex-promise.h"

/**
 * DexPromise:
 *
 * #DexPromise is a convenient #DexFuture for prpoagating a result or
 * rejection in appliction and library code.
 *
 * Use this when there is not a more specialized #DexFuture for your needs to
 * propagate a result or rejection to the caller in an asynchronous fashion.
 */

typedef struct _DexPromise
{
  DexFuture parent_instance;
  GCancellable *cancellable;
} DexPromise;

typedef struct _DexPromiseClass
{
  DexFutureClass parent_class;
} DexPromiseClass;

DEX_DEFINE_FINAL_TYPE (DexPromise, dex_promise, DEX_TYPE_FUTURE)

#undef DEX_TYPE_PROMISE
#define DEX_TYPE_PROMISE dex_promise_type

static void
dex_promise_discard (DexFuture *future)
{
  DexPromise *promise = DEX_PROMISE (future);

  g_cancellable_cancel (promise->cancellable);
}

static void
dex_promise_finalize (DexObject *object)
{
  DexPromise *self = (DexPromise *)object;

  g_clear_object (&self->cancellable);

  DEX_OBJECT_CLASS (dex_promise_parent_class)->finalize (object);
}

static void
dex_promise_class_init (DexPromiseClass *promise_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (promise_class);
  DexFutureClass *future_class = DEX_FUTURE_CLASS (promise_class);

  object_class->finalize = dex_promise_finalize;

  future_class->discard = dex_promise_discard;
}

static void
dex_promise_init (DexPromise *promise)
{
}

DexPromise *
(dex_promise_new) (void)
{
  return (DexPromise *)dex_object_create_instance (dex_promise_type);
}

/**
 * dex_promise_new_cancellable:
 *
 * Creates a new #DexPromise that can propagate cancellation if the
 * promise is discarded.
 *
 * This can be used to plumb cancellation between promises and
 * #GAsyncReadyCallback based APIs.
 *
 * Returns: (transfer full): a #DexPromise
 */
DexPromise *
(dex_promise_new_cancellable) (void)
{
  DexPromise *self = (DexPromise *)dex_object_create_instance (dex_promise_type);

  self->cancellable = g_cancellable_new ();

  return self;
}

/**
 * dex_promise_get_cancellable:
 * @promise: a #DexPromise
 *
 * Gets a #GCancellable that will cancel when the promise has
 * been discarded (and therefore result no longer necessary).
 *
 * This is useful when manually implementing wrappers around various
 * #GAsyncReadyCallback based API.
 *
 * If @promise was created with dex_promise_new(), then %NULL is returned.
 *
 * Returns: (transfer none) (nullable): a #GCancellable or %NULL
 */
GCancellable *
dex_promise_get_cancellable (DexPromise *promise)
{
  g_return_val_if_fail (DEX_IS_PROMISE (promise), NULL);

  return promise->cancellable;
}

/**
 * dex_promise_resolve:
 * @promise: a #DexPromise
 * @value: a #GValue containing the resolved value
 *
 * Sets the result for a #DexPromise.
 */
void
dex_promise_resolve (DexPromise   *promise,
                     const GValue *value)
{
  g_return_if_fail (DEX_IS_PROMISE (promise));
  g_return_if_fail (value != NULL && G_IS_VALUE (value));

  dex_future_complete (DEX_FUTURE (promise), value, NULL);
}

/**
 * dex_promise_reject:
 * @promise: a #DexPromise
 * @error: (transfer full): a #GError
 *
 * Marks the promise as rejected, indicating a failure.
 */
void
dex_promise_reject (DexPromise *promise,
                    GError     *error)
{
  g_return_if_fail (DEX_IS_PROMISE (promise));
  g_return_if_fail (error != NULL);

  dex_future_complete (DEX_FUTURE (promise), NULL, error);
}

void
dex_promise_resolve_int (DexPromise *promise,
                         int         value)
{
  GValue gvalue = {G_TYPE_INT, {{.v_int = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_uint (DexPromise *promise,
                          guint       value)
{
  GValue gvalue = {G_TYPE_UINT, {{.v_uint = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_int64 (DexPromise *promise,
                           gint64      value)
{
  GValue gvalue = {G_TYPE_INT64, {{.v_int64 = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_uint64 (DexPromise *promise,
                            guint64     value)
{
  GValue gvalue = {G_TYPE_UINT64, {{.v_int64 = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_long (DexPromise *promise,
                          glong       value)
{
  GValue gvalue = {G_TYPE_LONG, {{.v_long = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_ulong (DexPromise *promise,
                           glong       value)
{
  GValue gvalue = {G_TYPE_ULONG, {{.v_ulong = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_float (DexPromise *promise,
                           float       value)
{
  GValue gvalue = {G_TYPE_FLOAT, {{.v_float = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_double (DexPromise *promise,
                            double      value)
{
  GValue gvalue = {G_TYPE_DOUBLE, {{.v_double = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

void
dex_promise_resolve_boolean (DexPromise *promise,
                             gboolean    value)
{
  GValue gvalue = {G_TYPE_BOOLEAN, {{.v_int = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
}

/**
 * dex_promise_resolve_string:
 * @promise: a #DexPromise
 * @value: (transfer full): a string to use to resolve the promise
 *
 */
void
dex_promise_resolve_string (DexPromise *promise,
                            char       *value)
{
  GValue gvalue = {G_TYPE_STRING, {{.v_pointer = value}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
  g_free (value);
}

/**
 * dex_promise_resolve_object:
 * @promise: a #DexPromise
 * @object: (type GObject) (transfer full) (nullable): a #GObject
 *
 */
void
dex_promise_resolve_object (DexPromise *promise,
                            gpointer    object)
{
  GType gtype = object ? G_OBJECT_TYPE (object) : G_TYPE_OBJECT;
  GValue gvalue = {gtype, {{.v_pointer = object}, {.v_int = 0}}};
  dex_promise_resolve (promise, &gvalue);
  g_clear_object (&object);
}

/**
 * dex_promise_resolve_variant:
 * @promise: a #DexPromise
 * @variant: (transfer full) (nullable): a #GVariant
 *
 * If @variant is floating, its reference is consumed.
 *
 * Since: 0.8
 */
void
dex_promise_resolve_variant (DexPromise *promise,
                             GVariant   *variant)
{
  GValue gvalue = G_VALUE_INIT;
  g_value_init (&gvalue, G_TYPE_VARIANT);
  g_value_take_variant (&gvalue, variant);
  dex_promise_resolve (promise, &gvalue);
  g_value_unset (&gvalue);
}
