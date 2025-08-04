/* dex-fiber.h
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

#define DEX_TYPE_FIBER    (dex_fiber_get_type())
#define DEX_FIBER(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_FIBER, DexFiber))
#define DEX_IS_FIBER(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_FIBER))

typedef struct _DexFiber DexFiber;

DEX_AVAILABLE_IN_ALL
GType dex_fiber_get_type (void) G_GNUC_CONST;

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexFiber, dex_unref)

G_END_DECLS
