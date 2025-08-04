/*
 * dex-work-stealing-queue.c
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

#include "config.h"

#include "dex-compat-private.h"
#include "dex-work-stealing-queue-private.h"

#define DEFAULT_BATCH_SIZE 32

DexWorkStealingQueue *
dex_work_stealing_queue_new (gint64 capacity)
{
  DexWorkStealingQueue *work_stealing_queue;

  work_stealing_queue = g_aligned_alloc0 (1,
                                          sizeof (DexWorkStealingQueue),
                                          G_ALIGNOF (DexWorkStealingQueue));
  atomic_store_explicit (&work_stealing_queue->top, 0, memory_order_relaxed);
  atomic_store_explicit (&work_stealing_queue->bottom, 0, memory_order_relaxed);
  atomic_store_explicit (&work_stealing_queue->array,
                         dex_work_stealing_array_new (capacity),
                         memory_order_relaxed);
  work_stealing_queue->garbage = g_ptr_array_new_full (32, (GDestroyNotify)dex_work_stealing_array_free);
  g_atomic_ref_count_init (&work_stealing_queue->ref_count);

  return work_stealing_queue;
}

DexWorkStealingQueue *
dex_work_stealing_queue_ref (DexWorkStealingQueue *work_stealing_queue)
{
  g_atomic_ref_count_inc (&work_stealing_queue->ref_count);
  return work_stealing_queue;
}

void
dex_work_stealing_queue_unref (DexWorkStealingQueue *work_stealing_queue)
{
  if (g_atomic_ref_count_dec (&work_stealing_queue->ref_count))
    {
      GPtrArray *garbage = work_stealing_queue->garbage;
      DexWorkStealingArray *array = atomic_load (&work_stealing_queue->array);

      work_stealing_queue->top = 0;
      work_stealing_queue->bottom = 0;
      work_stealing_queue->array = NULL;
      work_stealing_queue->garbage = NULL;

      g_clear_pointer (&garbage, g_ptr_array_unref);
      g_clear_pointer (&array, dex_work_stealing_array_free);

      g_aligned_free (work_stealing_queue);
    }
}

typedef struct _DexWorkStealingQueueSource
{
  GSource               parent_instance;
  DexWorkStealingQueue *work_stealing_queue;
  guint                 batch_size;
} DexWorkStealingQueueSource;

static gboolean
dex_work_stealing_queue_source_prepare (GSource *source,
                                        int     *timeout)
{
  DexWorkStealingQueueSource *real_source = (DexWorkStealingQueueSource *)source;

  *timeout = -1;

  return !dex_work_stealing_queue_empty (real_source->work_stealing_queue);
}

static gboolean
dex_work_stealing_queue_source_check (GSource *source)
{
  DexWorkStealingQueueSource *real_source = (DexWorkStealingQueueSource *)source;

  return !dex_work_stealing_queue_empty (real_source->work_stealing_queue);
}

static gboolean
dex_work_stealing_queue_source_dispatch (GSource     *source,
                                         GSourceFunc  callback,
                                         gpointer     callback_data)
{
  DexWorkStealingQueueSource *real_source = (DexWorkStealingQueueSource *)source;
  DexWorkStealingQueue *work_stealing_queue = real_source->work_stealing_queue;

  for (guint i = 0; i < real_source->batch_size; i++)
    {
      DexWorkItem work_item;

      if (!dex_work_stealing_queue_pop (work_stealing_queue, &work_item))
        break;

      dex_work_item_invoke (&work_item);
    }

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs dex_work_stealing_queue_source_funcs = {
  .prepare = dex_work_stealing_queue_source_prepare,
  .check = dex_work_stealing_queue_source_check,
  .dispatch = dex_work_stealing_queue_source_dispatch,
};

GSource *
dex_work_stealing_queue_create_source (DexWorkStealingQueue *work_stealing_queue)
{
  DexWorkStealingQueueSource *real_source;
  GSource *source;

  g_return_val_if_fail (work_stealing_queue != NULL, NULL);

  source = g_source_new (&dex_work_stealing_queue_source_funcs, sizeof (DexWorkStealingQueueSource));
  real_source = (DexWorkStealingQueueSource *)source;

  _g_source_set_static_name (source, "[dex-work-stealing-queue]");
  real_source->work_stealing_queue = dex_work_stealing_queue_ref (work_stealing_queue);
  real_source->batch_size = DEFAULT_BATCH_SIZE;

  return source;
}
