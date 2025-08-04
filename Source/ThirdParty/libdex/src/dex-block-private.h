/*
 * dex-block-private.h
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

#include "dex-future-private.h"
#include "dex-block.h"

G_BEGIN_DECLS

DexFuture *dex_block_new (DexFuture         *future,
                          DexScheduler      *scheduler,
                          DexBlockKind       kind,
                          DexFutureCallback  callback,
                          gpointer           callback_data,
                          GDestroyNotify     callback_data_destroy);

G_END_DECLS
