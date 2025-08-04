/*
 * dex-scheduler-private.h
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

#include "dex-aio-backend-private.h"
#include "dex-fiber.h"
#include "dex-object-private.h"
#include "dex-scheduler.h"

G_BEGIN_DECLS

#define DEX_SCHEDULER_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS(obj, DEX_TYPE_SCHEDULER, DexSchedulerClass))
#define DEX_SCHEDULER_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST(klass, DEX_TYPE_SCHEDULER, DexSchedulerClass))

typedef struct _DexWorkItem
{
  DexSchedulerFunc func;
  gpointer         func_data;
} DexWorkItem;

typedef struct _DexScheduler
{
  DexObject parent_instance;
} DexScheduler;

typedef struct _DexSchedulerClass
{
  DexObjectClass parent_class;

  void           (*push)             (DexScheduler *scheduler,
                                      DexWorkItem   work_item);
  void           (*spawn)            (DexScheduler *scheduler,
                                      DexFiber     *fiber);
  GMainContext  *(*get_main_context) (DexScheduler *scheduler);
  DexAioContext *(*get_aio_context)  (DexScheduler *scheduler);
} DexSchedulerClass;

void           dex_scheduler_set_thread_default (DexScheduler *scheduler);
void           dex_scheduler_set_default        (DexScheduler *scheduler);
DexAioContext *dex_scheduler_get_aio_context    (DexScheduler *scheduler);

static inline void
dex_work_item_invoke (const DexWorkItem *work_item)
{
  work_item->func (work_item->func_data);
}

G_END_DECLS
