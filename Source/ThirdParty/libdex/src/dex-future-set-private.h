/*
 * dex-future-set-private.h
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

#include "dex-future-set.h"
#include "dex-future-private.h"

G_BEGIN_DECLS

typedef enum _DexFutureSetFlags
{
  DEX_FUTURE_SET_FLAGS_NONE = 0,

  /* Propagate first resolve/reject (use extra flags to specify) */
  DEX_FUTURE_SET_FLAGS_PROPAGATE_FIRST = 1 << 0,

  /* with PROPAGATE_FIRST, propagates on first resolve */
  DEX_FUTURE_SET_FLAGS_PROPAGATE_RESOLVE = 1 << 1,

  /* with PROPAGATE_FIRST, propagates on first reject */
  DEX_FUTURE_SET_FLAGS_PROPAGATE_REJECT = 1 << 2,
} DexFutureSetFlags;

DexFutureSet *dex_future_set_new_va (DexFuture         *first_future,
                                     va_list           *args,
                                     DexFutureSetFlags  flags);
DexFutureSet *dex_future_set_new    (DexFuture * const *futures,
                                     guint              n_futures,
                                     DexFutureSetFlags  flags);

G_END_DECLS
