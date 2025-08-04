/*
 * dex-delayed.c
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

#include "dex-delayed.h"
#include "dex-future-private.h"

/**
 * DexDelayed:
 *
 * #DexDelayed is a future which will resolve or reject the value of another
 * #DexFuture when dex_delayed_release() is called.
 *
 * This allows you to gate the resolution of a future which has already
 * resolved or rejected until a later moment.
 */

struct _DexDelayed
{
  DexFuture parent_instance;
  DexFuture *future;
  guint corked : 1;
};

typedef struct _DexDelayedClass
{
  DexFutureClass parent_class;
} DexDelayedClass;

DEX_DEFINE_FINAL_TYPE (DexDelayed, dex_delayed, DEX_TYPE_FUTURE)

static gboolean
dex_delayed_propagate (DexFuture *future,
                       DexFuture *completed)
{
  DexDelayed *delayed = DEX_DELAYED (future);
  gboolean ret = FALSE;

  g_assert (DEX_IS_DELAYED (delayed));
  g_assert (DEX_IS_FUTURE (completed));

  dex_object_lock (delayed);
  if (delayed->corked)
    ret = TRUE;
  else
    dex_clear (&delayed->future);
  dex_object_unlock (delayed);

  return ret;
}

static void
dex_delayed_discard (DexFuture *future)
{
  DexDelayed *delayed = DEX_DELAYED (future);
  DexFuture *awaiting;

  g_assert (DEX_IS_DELAYED (delayed));

  dex_object_lock (delayed);
  awaiting = g_steal_pointer (&delayed->future);
  dex_object_unlock (delayed);

  if (awaiting != NULL)
    {
      dex_future_discard (awaiting, future);
      dex_clear (&awaiting);
    }
}

static void
dex_delayed_finalize (DexObject *object)
{
  DexDelayed *delayed = DEX_DELAYED (object);

  dex_clear (&delayed->future);

  DEX_OBJECT_CLASS (dex_delayed_parent_class)->finalize (object);
}

static void
dex_delayed_class_init (DexDelayedClass *delayed_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (delayed_class);
  DexFutureClass *future_class = DEX_FUTURE_CLASS (delayed_class);

  object_class->finalize = dex_delayed_finalize;

  future_class->propagate = dex_delayed_propagate;
  future_class->discard = dex_delayed_discard;
}

static void
dex_delayed_init (DexDelayed *delayed)
{
}

DexFuture *
dex_delayed_new (DexFuture *future)
{
  DexDelayed *delayed;

  g_return_val_if_fail (DEX_IS_FUTURE (future), NULL);

  delayed = (DexDelayed *)dex_object_create_instance (DEX_TYPE_DELAYED);
  delayed->corked = TRUE;
  delayed->future = dex_ref (future);

  dex_future_chain (DEX_FUTURE (delayed), future);

  return DEX_FUTURE (delayed);
}

void
dex_delayed_release (DexDelayed *delayed)
{
  DexFuture *complete = NULL;

  g_return_if_fail (DEX_IS_DELAYED (delayed));

  dex_object_lock (delayed);

  if (delayed->corked)
    {
      delayed->corked = FALSE;
      complete = g_steal_pointer (&delayed->future);
    }

  dex_object_unlock (delayed);

  if (complete != NULL)
    {
      dex_future_complete_from (DEX_FUTURE (delayed), complete);
      dex_clear (&complete);
    }
}

/**
 * dex_delayed_dup_future:
 * @delayed: a #DexDelayed
 *
 * Retrieves the delayed future provided to dex_delayed_new().
 *
 * This function can only return a #DexFuture before dex_delayed_release()
 * is called. After that, the delayed future is released and this function
 * will return %NULL.
 *
 * Returns: (transfer full) (nullable): a #DexFuture or %NULL
 */
DexFuture *
dex_delayed_dup_future (DexDelayed *delayed)
{
  DexFuture *ret;

  g_return_val_if_fail (DEX_IS_DELAYED (delayed), NULL);

  dex_object_lock (delayed);
  ret = delayed->future ? dex_ref (delayed->future) : NULL;
  dex_object_unlock (delayed);

  return ret;
}
