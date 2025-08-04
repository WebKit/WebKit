/*
 * dex-async-pair-private.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#include <gio/gio.h>

#include "dex-async-pair.h"
#include "dex-future-private.h"

G_BEGIN_DECLS

typedef struct _DexAsyncPair
{
  DexFuture parent_instance;
  gpointer instance;
  GCancellable *cancellable;
  DexAsyncPairInfo *info;
  guint cancel_on_discard : 1;
} DexAsyncPair;

typedef struct _DexAsyncPairClass
{
  DexFutureClass parent_class;
} DexAsyncPairClass;

G_END_DECLS
