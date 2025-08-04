/*
 * dex-work-queue-private.h
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

#include "dex-scheduler-private.h"

G_BEGIN_DECLS

#define DEX_TYPE_WORK_QUEUE    (dex_work_queue_get_type())
#define DEX_WORK_QUEUE(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_WORK_QUEUE, DexWorkQueue))
#define DEX_IS_WORK_QUEUE(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_WORK_QUEUE))

typedef struct _DexWorkQueue DexWorkQueue;

GType         dex_work_queue_get_type (void) G_GNUC_CONST;
DexWorkQueue *dex_work_queue_new      (void);
void          dex_work_queue_push     (DexWorkQueue *work_queue,
                                       DexWorkItem   work_item);
gboolean      dex_work_queue_try_pop  (DexWorkQueue *work_queue,
                                       DexWorkItem  *out_work_item);
DexFuture    *dex_work_queue_run      (DexWorkQueue *work_queue);

G_END_DECLS
