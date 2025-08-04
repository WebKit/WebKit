/*
 * dex-thread-storage-private.h
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

#include <glib.h>

G_BEGIN_DECLS

#define DEX_THREAD_POOL_WORKER_CURRENT (dex_thread_storage_get()->worker)
#define DEX_DISPATCH_RECURSE_MAX       4

typedef struct _DexAioContext DexAioContext;
typedef struct _DexFiberScheduler DexFiberScheduler;
typedef struct _DexScheduler DexScheduler;
typedef struct _DexThreadPoolWorker DexThreadPoolWorker;

typedef struct _DexThreadStorage
{
  DexScheduler        *scheduler;
  DexThreadPoolWorker *worker;
  DexAioContext       *aio_context;
  DexFiberScheduler   *fiber_scheduler;
  guint                sync_dispatch_depth;
} DexThreadStorage;

DexThreadStorage *dex_thread_storage_get (void);

G_END_DECLS
