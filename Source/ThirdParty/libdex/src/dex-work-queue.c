/*
 * dex-work-queue.c
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

#include "dex-object-private.h"
#include "dex-semaphore-private.h"
#include "dex-work-queue-private.h"

struct _DexWorkQueue
{
  DexObject parent_instance;
  DexSemaphore *semaphore;
  GMutex mutex;
  GQueue queue;
};

typedef struct _DexWorkQueueClass
{
  DexObjectClass parent_class;
} DexWorkQueueClass;

DEX_DEFINE_FINAL_TYPE (DexWorkQueue, dex_work_queue, DEX_TYPE_OBJECT)

typedef struct _DexWorkQueueItem
{
  GList link;
  DexWorkItem work_item;
} DexWorkQueueItem;

static void
dex_work_queue_finalize (DexObject *object)
{
  DexWorkQueue *work_queue = DEX_WORK_QUEUE (object);

  if (work_queue->queue.length > 0)
    g_critical ("Work queue %p freed with %u items still in it!",
                work_queue, work_queue->queue.length);

  g_mutex_clear (&work_queue->mutex);
  dex_clear (&work_queue->semaphore);

  DEX_OBJECT_CLASS (dex_work_queue_parent_class)->finalize (object);
}

static void
dex_work_queue_class_init (DexWorkQueueClass *work_queue_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (work_queue_class);

  object_class->finalize = dex_work_queue_finalize;
}

static void
dex_work_queue_init (DexWorkQueue *work_queue)
{
  work_queue->semaphore = dex_semaphore_new ();
  g_mutex_init (&work_queue->mutex);
}

DexWorkQueue *
dex_work_queue_new (void)
{
  return (DexWorkQueue *)dex_object_create_instance (DEX_TYPE_WORK_QUEUE);
}

void
dex_work_queue_push (DexWorkQueue *work_queue,
                     DexWorkItem   work_item)
{
  DexWorkQueueItem *work_queue_item;

  g_return_if_fail (DEX_IS_WORK_QUEUE (work_queue));
  g_return_if_fail (work_item.func != NULL);

  work_queue_item = g_new0 (DexWorkQueueItem, 1);
  work_queue_item->link.data = work_queue_item;
  work_queue_item->work_item = work_item;

  g_mutex_lock (&work_queue->mutex);
  g_queue_push_tail_link (&work_queue->queue, &work_queue_item->link);
  g_mutex_unlock (&work_queue->mutex);

  dex_semaphore_post (work_queue->semaphore);
}

gboolean
dex_work_queue_try_pop (DexWorkQueue *work_queue,
                        DexWorkItem  *out_work_item)
{
  GList *link;

  g_return_val_if_fail (DEX_IS_WORK_QUEUE (work_queue), FALSE);
  g_return_val_if_fail (out_work_item != NULL, FALSE);

  g_mutex_lock (&work_queue->mutex);
  link = g_queue_pop_head_link (&work_queue->queue);
  g_mutex_unlock (&work_queue->mutex);

  if (link != NULL)
    {
      DexWorkQueueItem *item = link->data;
      *out_work_item = item->work_item;
      g_free (item);
    }

  return link != NULL;
}

static DexFuture *
dex_work_queue_run_cb (DexFuture *future,
                       gpointer   user_data)
{
  DexWorkQueue *work_queue = user_data;
  DexWorkItem work_item;

  g_assert (DEX_IS_WORK_QUEUE (work_queue));

  if (dex_work_queue_try_pop (work_queue, &work_item))
    dex_work_item_invoke (&work_item);

  return dex_semaphore_wait (work_queue->semaphore);
}

DexFuture *
dex_work_queue_run (DexWorkQueue *work_queue)
{
  DexFuture *future;

  g_return_val_if_fail (work_queue != NULL, NULL);

  future = dex_semaphore_wait (work_queue->semaphore);
  future = dex_future_finally_loop (future,
                                    dex_work_queue_run_cb,
                                    dex_ref (work_queue),
                                    dex_unref);

  return future;
}
