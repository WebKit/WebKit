/*
 * dex-cancellable.h
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

#pragma once

#if !defined (DEX_INSIDE) && !defined (DEX_COMPILATION)
# error "Only <libdex.h> can be included directly."
#endif

#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_CANCELLABLE    (dex_cancellable_get_type())
#define DEX_CANCELLABLE(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_CANCELLABLE, DexCancellable))
#define DEX_IS_CANCELLABLE(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_CANCELLABLE))

typedef struct _DexCancellable DexCancellable;

DEX_AVAILABLE_IN_ALL
GType           dex_cancellable_get_type             (void) G_GNUC_CONST;
DEX_AVAILABLE_IN_ALL
DexCancellable *dex_cancellable_new                  (void)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture      *dex_cancellable_new_from_cancellable (GCancellable *cancellable)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
void            dex_cancellable_cancel               (DexCancellable *cancellable);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexCancellable, dex_unref)

G_END_DECLS
