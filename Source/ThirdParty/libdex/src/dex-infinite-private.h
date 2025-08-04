/*
 * dex-infinite-private.h
 *
 * Copyright 2023 Christian Hergert <chergert@gnome.org>
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

G_BEGIN_DECLS

#define DEX_TYPE_INFINITE    (dex_infinite_get_type())
#define DEX_INFINITE(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_INFINITE, DexInfinite))
#define DEX_IS_INFINITE(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_INFINITE))

GType      dex_infinite_get_type (void) G_GNUC_CONST;
DexFuture *dex_infinite_new      (void);

G_END_DECLS
