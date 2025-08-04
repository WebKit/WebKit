/*
 * dex-error.h
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

#include "dex-version-macros.h"

G_BEGIN_DECLS

#define DEX_ERROR (dex_error_quark())

typedef enum _DexError
{
  DEX_ERROR_UNKNOWN,
  DEX_ERROR_CHANNEL_CLOSED,
  DEX_ERROR_DEPENDENCY_FAILED,
  DEX_ERROR_FIBER_EXITED,
  DEX_ERROR_NO_FIBER,
  DEX_ERROR_PENDING,
  DEX_ERROR_SEMAPHORE_CLOSED,
  DEX_ERROR_TIMED_OUT,
  DEX_ERROR_TYPE_MISMATCH,
  DEX_ERROR_TYPE_NOT_SUPPORTED,
  DEX_ERROR_FIBER_CANCELLED,
} DexError;

DEX_AVAILABLE_IN_ALL
GQuark dex_error_quark (void) G_GNUC_CONST;

G_END_DECLS
