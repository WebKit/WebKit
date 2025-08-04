/*
 * dex-enums.h
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

#pragma once

#if !defined (DEX_INSIDE) && !defined (DEX_COMPILATION)
# error "Only <libdex.h> can be included directly."
#endif

#include <glib-object.h>

#include "dex-version-macros.h"

G_BEGIN_DECLS

#define DEX_TYPE_FUTURE_STATUS (dex_future_status_get_type())

typedef enum _DexFutureStatus
{
  DEX_FUTURE_STATUS_PENDING,
  DEX_FUTURE_STATUS_RESOLVED,
  DEX_FUTURE_STATUS_REJECTED,
} DexFutureStatus;

DEX_AVAILABLE_IN_ALL
GType dex_future_status_get_type (void) G_GNUC_CONST;

G_END_DECLS
