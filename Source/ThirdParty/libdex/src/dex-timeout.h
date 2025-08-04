/*
 * dex-timeout.h
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

#define DEX_TYPE_TIMEOUT    (dex_timeout_get_type())
#define DEX_TIMEOUT(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_TIMEOUT, DexTimeout))
#define DEX_IS_TIMEOUT(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_TIMEOUT))

typedef struct _DexTimeout DexTimeout;

DEX_AVAILABLE_IN_ALL
GType      dex_timeout_get_type       (void) G_GNUC_CONST;
DEX_AVAILABLE_IN_ALL
DexFuture *dex_timeout_new_deadline   (gint64      deadline);
DEX_AVAILABLE_IN_ALL
DexFuture *dex_timeout_new_seconds    (int         seconds);
DEX_AVAILABLE_IN_ALL
DexFuture *dex_timeout_new_msec       (int         msec);
DEX_AVAILABLE_IN_ALL
DexFuture *dex_timeout_new_usec       (gint64      usec);
DEX_AVAILABLE_IN_ALL
void       dex_timeout_postpone_until (DexTimeout *timeout,
                                       gint64      deadline);

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexTimeout, dex_unref)

G_END_DECLS
