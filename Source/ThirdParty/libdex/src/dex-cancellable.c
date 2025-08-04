/*
 * dex-cancellable.c
 *
 * Copyright 2022 Christian Hergert <chergert@gnome.org>
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

#include "dex-cancellable.h"
#include "dex-future-private.h"

/**
 * DexCancellable:
 *
 * `DexCancellable` is a simple cancellation primitive which allows
 * for you to create `DexFuture` that will reject upon cancellation.
 *
 * Use this combined with other futures using dex_future_all_race()
 * to create a future that resolves when all other futures complete
 * or `dex_cancellable_cancel()` is called to reject.
 */

typedef struct _DexCancellable
{
  DexFuture     parent_instance;
  GCancellable *cancellable;
  gulong        handler;
} DexCancellable;

typedef struct _DexCancellableClass
{
  DexFutureClass parent_class;
} DexCancellableClass;

DEX_DEFINE_FINAL_TYPE (DexCancellable, dex_cancellable, DEX_TYPE_FUTURE)

static void
dex_cancellable_finalize (DexObject *object)
{
  DexCancellable *self = (DexCancellable *)object;

  if (self->handler)
    g_cancellable_disconnect (self->cancellable, self->handler);

  g_clear_object (&self->cancellable);

  DEX_OBJECT_CLASS (dex_cancellable_parent_class)->finalize (object);
}

static void
dex_cancellable_class_init (DexCancellableClass *cancellable_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (cancellable_class);

  object_class->finalize = dex_cancellable_finalize;
}

static void
dex_cancellable_init (DexCancellable *cancellable)
{
}

DexCancellable *
dex_cancellable_new (void)
{
  return (DexCancellable *)dex_object_create_instance (DEX_TYPE_CANCELLABLE);
}

static void
dex_cancellable_cancelled_cb (GCancellable *cancellable,
                              DexWeakRef   *wr)
{
  g_autoptr(DexCancellable) self = NULL;

  g_assert (G_IS_CANCELLABLE (cancellable));
  g_assert (wr != NULL);

  if ((self = dex_weak_ref_get (wr)))
    {
      self->handler = 0;
      dex_future_complete (DEX_FUTURE (self),
                           NULL,
                           g_error_new_literal (G_IO_ERROR,
                                                G_IO_ERROR_CANCELLED,
                                                "Operation cancelled"));
    }
}

static void
weak_ref_free (gpointer data)
{
  dex_weak_ref_clear (data);
  g_free (data);
}

DexFuture *
dex_cancellable_new_from_cancellable (GCancellable *cancellable)
{
  DexWeakRef *wr;
  DexCancellable *ret;

  g_return_val_if_fail (G_IS_CANCELLABLE (cancellable), NULL);

  ret = dex_cancellable_new ();

  wr = g_new0 (DexWeakRef, 1);
  dex_weak_ref_init (wr, ret);

  ret->cancellable = g_object_ref (cancellable);
  ret->handler = g_cancellable_connect (cancellable,
                                        G_CALLBACK (dex_cancellable_cancelled_cb),
                                        wr, weak_ref_free);

  return DEX_FUTURE (ret);
}

void
dex_cancellable_cancel (DexCancellable *cancellable)
{
  g_return_if_fail (DEX_IS_CANCELLABLE (cancellable));

  dex_future_complete (DEX_FUTURE (cancellable),
                       NULL,
                       g_error_new_literal (G_IO_ERROR,
                                            G_IO_ERROR_CANCELLED,
                                            "Operation cancelled"));
}
