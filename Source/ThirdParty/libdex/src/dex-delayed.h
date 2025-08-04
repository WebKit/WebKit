/*
 * dex-delayed.h
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

#pragma once

#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_DELAYED       (dex_delayed_get_type())
#define DEX_DELAYED(object)    (G_TYPE_CHECK_INSTANCE_CAST(object, DEX_TYPE_DELAYED, DexDelayed))
#define DEX_IS_DELAYED(object) (G_TYPE_CHECK_INSTANCE_TYPE(object, DEX_TYPE_DELAYED))

typedef struct _DexDelayed DexDelayed;

DEX_AVAILABLE_IN_ALL
GType      dex_delayed_get_type   (void) G_GNUC_CONST;
DEX_AVAILABLE_IN_ALL
DexFuture *dex_delayed_new        (DexFuture  *future)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
void       dex_delayed_release    (DexDelayed *delayed);
DEX_AVAILABLE_IN_ALL
DexFuture *dex_delayed_dup_future (DexDelayed *delayed)
  G_GNUC_WARN_UNUSED_RESULT;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexDelayed, dex_unref)

G_END_DECLS
