/*
 * dex-future-set.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
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

#include "dex-error.h"
#include "dex-future-set-private.h"

/**
 * DexFutureSet:
 *
 * #DexFutureSet represents a set of #DexFuture.
 *
 * You may retrieve each underlying #DexFuture using
 * dex_future_set_get_future_at().
 *
 * The #DexFutureStatus of of the #DexFutureSet depends on how the set
 * was created using dex_future_all(), dex_future_any(), and similar mmethods.
 */

typedef struct _DexFutureSet
{
  DexFuture           parent_instance;
  DexFuture         **futures;
  guint               n_futures;
  guint               n_resolved;
  guint               n_rejected;
  DexFutureSetFlags   flags : 4;
  guint               padding : 28;
  /* If n_futures <= 2, we use this instead of mallocing. We could
   * potentially get a 3rd without breaking 2-cachelines (128 bytes)
   * if we used a bit above to manage a union of futures/embedded.
   */
  DexFuture          *embedded[2];
} DexFutureSet;

typedef struct _DexFutureSetClass
{
  DexFutureClass parent_class;
} DexFutureSetClass;

DEX_DEFINE_FINAL_TYPE (DexFutureSet, dex_future_set, DEX_TYPE_FUTURE)

#undef DEX_TYPE_FUTURE_SET
#define DEX_TYPE_FUTURE_SET dex_future_set_type

static GValue success_value = G_VALUE_INIT;

static gboolean
dex_future_set_propagate (DexFuture *future,
                          DexFuture *completed)
{
  DexFutureSet *future_set = DEX_FUTURE_SET (future);
  const GValue *resolved = NULL;
  DexFutureStatus status;
  gboolean do_discard = FALSE;
  GError *rejected = NULL;
  guint n_active = 0;

  g_assert (DEX_IS_FUTURE_SET (future_set));
  g_assert (DEX_IS_FUTURE (completed));
  g_assert (future != completed);

  dex_object_lock (future_set);

  /* Short-circuit if we've already returned a value */
  if (future->status != DEX_FUTURE_STATUS_PENDING)
    {
      dex_object_unlock (future_set);
      return TRUE;
    }

  switch ((int)(status = dex_future_get_status (completed)))
    {
    case DEX_FUTURE_STATUS_RESOLVED:
      future_set->n_resolved++;
      break;

    case DEX_FUTURE_STATUS_REJECTED:
      future_set->n_rejected++;
      break;

    default:
      g_assert_not_reached ();
    }

  n_active = future_set->n_futures - future_set->n_rejected - future_set->n_resolved;
  resolved = dex_future_get_value (completed, &rejected);

  g_assert (future_set->n_rejected <= future_set->n_futures);
  g_assert (future_set->n_resolved <= future_set->n_futures);
  g_assert (future_set->n_resolved + future_set->n_rejected <= future_set->n_futures);
  g_assert (rejected != NULL || resolved != NULL);

  dex_object_unlock (future_set);

  if (n_active == 0)
    {
      do_discard = TRUE;

      if (resolved != NULL)
        {
          if (future_set->flags & DEX_FUTURE_SET_FLAGS_PROPAGATE_RESOLVE)
            dex_future_complete (future, resolved, NULL);
          else
            dex_future_complete (future, &success_value, NULL);
        }
      else
        {
          if (future_set->flags & DEX_FUTURE_SET_FLAGS_PROPAGATE_REJECT)
            dex_future_complete (future, NULL, g_steal_pointer (&rejected));
          else
            dex_future_complete (future,
                                 NULL,
                                 g_error_new_literal (DEX_ERROR,
                                                      DEX_ERROR_DEPENDENCY_FAILED,
                                                      "Too many futures failed"));
        }
    }
  else if (future_set->flags & DEX_FUTURE_SET_FLAGS_PROPAGATE_FIRST)
    {
      do_discard = TRUE;

      if (resolved && (future_set->flags & DEX_FUTURE_SET_FLAGS_PROPAGATE_RESOLVE) != 0)
        dex_future_complete (future, resolved, NULL);
      else if (rejected && (future_set->flags & DEX_FUTURE_SET_FLAGS_PROPAGATE_REJECT) != 0)
        dex_future_complete (future, NULL, g_steal_pointer (&rejected));
      else
        do_discard = FALSE;
    }

  g_clear_error (&rejected);

  if (do_discard)
    {
      if (dex_future_get_status (future) != DEX_FUTURE_STATUS_PENDING)
        {
          for (guint i = 0; i < future_set->n_futures; i++)
            dex_future_discard (future_set->futures[i], future);
        }
    }

  return TRUE;
}

static void
dex_future_set_finalize (DexObject *object)
{
  DexFutureSet *future_set = DEX_FUTURE_SET (object);

  for (guint i = 0; i < future_set->n_futures; i++)
    {
      DexFuture *future = g_steal_pointer (&future_set->futures[i]);

      if (future != NULL)
        {
          dex_future_discard (future, DEX_FUTURE (future_set));
          dex_unref (future);
        }
    }

  if (future_set->futures != future_set->embedded)
    g_clear_pointer (&future_set->futures, g_free);

  future_set->futures = NULL;
  future_set->n_futures = 0;
  future_set->flags = 0;
  future_set->n_resolved = 0;
  future_set->n_rejected = 0;

  DEX_OBJECT_CLASS (dex_future_set_parent_class)->finalize (object);
}

static void
dex_future_set_class_init (DexFutureSetClass *future_set_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (future_set_class);
  DexFutureClass *future_class = DEX_FUTURE_CLASS (future_set_class);

  object_class->finalize = dex_future_set_finalize;

  future_class->propagate = dex_future_set_propagate;

  g_value_init (&success_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&success_value, TRUE);
}

static void
dex_future_set_init (DexFutureSet *future_set)
{
}

/**
 * dex_future_set_new: (skip)
 * @futures: (transfer none) (array length=n_futures): an array of futures to chain
 * @n_futures: number of futures
 * @flags: the flags for how the future set should resolve/reject
 *
 * Creates a new future set (also a future) which will resovle or reject
 * based on the completion from sub #DexFuture.
 *
 * There must be at least 1 future provided in @futures.
 *
 * Returns: the new #DexFutureSet
 */
DexFutureSet *
dex_future_set_new (DexFuture * const *futures,
                    guint              n_futures,
                    DexFutureSetFlags  flags)
{
  DexFutureSet *future_set;

  g_return_val_if_fail (futures != NULL, NULL);
  g_return_val_if_fail (n_futures > 0, NULL);
  g_return_val_if_fail ((flags & DEX_FUTURE_SET_FLAGS_PROPAGATE_FIRST) == 0 ||
                        (flags & (DEX_FUTURE_SET_FLAGS_PROPAGATE_RESOLVE|DEX_FUTURE_SET_FLAGS_PROPAGATE_REJECT)) != 0,
                        NULL);

  /* Setup our new DexFuture to contain the results */
  future_set = (DexFutureSet *)dex_object_create_instance (DEX_TYPE_FUTURE_SET);
  future_set->n_futures = n_futures;
  future_set->flags = flags;

  if (n_futures <= G_N_ELEMENTS (future_set->embedded))
    future_set->futures = future_set->embedded;
  else
    future_set->futures = g_new0 (DexFuture *, n_futures);

  /* Ref all futures before potentially calling out to chain them */
  for (guint i = 0; i < n_futures; i++)
    future_set->futures[i] = dex_ref (futures[i]);

  /* Now start chaining futures, even if we progress from pending while
   * we iterate this list (as we're safe against multiple resolves).
   */
  for (guint i = 0; i < n_futures; i++)
    dex_future_chain (future_set->futures[i], DEX_FUTURE (future_set));

  return future_set;
}

DexFutureSet *
dex_future_set_new_va (DexFuture         *first_future,
                       va_list           *args,
                       DexFutureSetFlags  flags)
{
  DexFutureSet *future_set;
  DexFuture *future = first_future;
  guint capacity = 8;

  g_return_val_if_fail (DEX_IS_FUTURE (first_future), NULL);
  g_return_val_if_fail ((flags & DEX_FUTURE_SET_FLAGS_PROPAGATE_FIRST) == 0 ||
                        (flags & (DEX_FUTURE_SET_FLAGS_PROPAGATE_RESOLVE|DEX_FUTURE_SET_FLAGS_PROPAGATE_REJECT)) != 0,
                        NULL);

  future_set = (DexFutureSet *)dex_object_create_instance (DEX_TYPE_FUTURE_SET);
  future_set->flags = flags;
  future_set->futures = future_set->embedded;

  g_assert (capacity > G_N_ELEMENTS (future_set->embedded));

  while (future != NULL)
    {
      if G_UNLIKELY (future_set->n_futures == G_N_ELEMENTS (future_set->embedded))
        {
          future_set->futures = g_new0 (DexFuture *, capacity);
          for (guint j = 0; j < G_N_ELEMENTS (future_set->embedded); j++)
            future_set->futures[j] = future_set->embedded[j];
        }
      else if G_UNLIKELY (future_set->n_futures + 1 > capacity)
        {
          capacity *= 2;
          future_set->futures = g_realloc_n (future_set->futures, capacity, sizeof (DexFuture *));
        }

      future_set->futures[future_set->n_futures++] = future;
      future = va_arg (*args, DexFuture *);
    }

  /* Now start chaining futures, even if we progress from pending while
   * we iterate this list (as we're safe against multiple resolves).
   */
  for (guint i = 0; i < future_set->n_futures; i++)
    dex_future_chain (future_set->futures[i], DEX_FUTURE (future_set));

  return future_set;
}

/**
 * dex_future_set_get_size:
 * @future_set: a #DexFutureSet
 *
 * Gets the number of futures associated with the #DexFutureSet. You may
 * use dex_future_set_get_future_at() to obtain the individual #DexFuture.
 *
 * Returns: the number of #DexFuture in @future_set.
 */
guint
dex_future_set_get_size (DexFutureSet *future_set)
{
  g_return_val_if_fail (DEX_IS_FUTURE_SET (future_set), 0);

  return future_set->n_futures;
}

/**
 * dex_future_set_get_future_at:
 * @future_set: a #DexFutureSet
 *
 * Gets a #DexFuture that was used to produce the result of @future_set.
 *
 * Use dex_future_set_get_size() to determine the number of #DexFuture that
 * are contained within the #DexFutureSet.
 *
 * Returns: (transfer none): a #DexFuture
 */
DexFuture *
dex_future_set_get_future_at (DexFutureSet *future_set,
                              guint         position)
{
  g_return_val_if_fail (DEX_IS_FUTURE_SET (future_set), NULL);
  g_return_val_if_fail (position < future_set->n_futures, NULL);

  return future_set->futures[position];
}

/**
 * dex_future_set_get_value_at:
 * @future_set: a #DexFutureSet
 * @position: the #DexFuture position within the set
 * @error: location for a #GError, or %NULL
 *
 * Gets the result from a #DexFuture that is part of the
 * #DexFutureSet.
 *
 * Returns: (transfer none): a #GValue if successful; otherwise %NULL
 *   and @error is set.
 */
const GValue *
dex_future_set_get_value_at (DexFutureSet  *future_set,
                             guint          position,
                             GError       **error)
{
  g_return_val_if_fail (DEX_IS_FUTURE_SET (future_set), NULL);
  g_return_val_if_fail (position < future_set->n_futures, NULL);

  return dex_future_get_value (future_set->futures[position], error);
}
