/*
 * dex-enums.c
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

#include "config.h"

#include "dex-compat-private.h"
#include "dex-enums.h"

G_DEFINE_ENUM_TYPE (DexFutureStatus, dex_future_status,
                    G_DEFINE_ENUM_VALUE (DEX_FUTURE_STATUS_PENDING, "pending"),
                    G_DEFINE_ENUM_VALUE (DEX_FUTURE_STATUS_RESOLVED, "resolved"),
                    G_DEFINE_ENUM_VALUE (DEX_FUTURE_STATUS_REJECTED, "rejected"))
