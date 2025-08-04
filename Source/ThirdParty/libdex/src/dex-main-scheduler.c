/*
 * dex-main-scheduler.c
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

#include "dex-aio-backend-private.h"
#include "dex-fiber-private.h"
#include "dex-main-scheduler-private.h"
#include "dex-scheduler-private.h"
#include "dex-work-queue-private.h"
#include "dex-thread-storage-private.h"

/**
 * DexMainScheduler:
 *
 * #DexMainScheduler is the scheduler used on the default thread of an
 * application. It is meant to integrate with your main loop.
 *
 * This scheduler does the bulk of the work in an application.
 *
 * Use #DexThreadPoolScheduler when you want to offload work to a thread
 * and still use future-based programming.
 */

typedef struct _DexMainWorkQueueItem
{
  DexWorkItem work_item;
  GList link;
} DexMainWorkQueueItem;

typedef struct _DexMainWorkQueueSource
{
  GSource    source;
  DexObject *object;
  GQueue    *queue;
} DexMainWorkQueueSource;

typedef struct _DexMainScheduler
{
  DexScheduler      parent_scheduler;
  GMainContext     *main_context;
  GSource          *aio_context;
  GSource          *fiber_scheduler;
  GSource          *work_queue_source;
  GQueue            work_queue;
} DexMainScheduler;

typedef struct _DexMainSchedulerClass
{
  DexSchedulerClass parent_class;
} DexMainSchedulerClass;

DEX_DEFINE_FINAL_TYPE (DexMainScheduler, dex_main_scheduler, DEX_TYPE_SCHEDULER)

#undef DEX_TYPE_MAIN_SCHEDULER
#define DEX_TYPE_MAIN_SCHEDULER dex_main_scheduler_type

static gboolean
dex_main_work_queue_check (GSource *source)
{
  DexMainWorkQueueSource *wqs = (DexMainWorkQueueSource *)source;
  gboolean ret;

  dex_object_lock (wqs->object);
  ret =  wqs->queue->length > 0;
  dex_object_unlock (wqs->object);

  return ret;
}

static gboolean
dex_main_work_queue_prepare (GSource *source,
                             int     *timeout)
{
  *timeout = -1;
  return dex_main_work_queue_check (source);
}

static gboolean
dex_main_work_queue_dispatch (GSource     *source,
                              GSourceFunc  callback,
                              gpointer     callback_data)
{
  DexMainWorkQueueSource *wqs = (DexMainWorkQueueSource *)source;
  GQueue queue;

  dex_object_lock (wqs->object);
  queue = *wqs->queue;
  *wqs->queue = (GQueue) {NULL, NULL, 0};
  dex_object_unlock (wqs->object);

  while (queue.length > 0)
    {
      DexMainWorkQueueItem *item = g_queue_pop_head_link (&queue)->data;
      dex_work_item_invoke (&item->work_item);
      g_free (item);
    }

  return G_SOURCE_CONTINUE;
}

static GSourceFuncs dex_main_work_queue_source_funcs = {
  .check = dex_main_work_queue_check,
  .dispatch = dex_main_work_queue_dispatch,
  .prepare = dex_main_work_queue_prepare,
};

static void
dex_main_scheduler_push (DexScheduler *scheduler,
                         DexWorkItem   work_item)
{
  DexMainScheduler *main_scheduler = DEX_MAIN_SCHEDULER (scheduler);
  DexMainWorkQueueItem *item;

  g_assert (DEX_IS_MAIN_SCHEDULER (main_scheduler));

  item = g_new0 (DexMainWorkQueueItem, 1);
  item->work_item = work_item;
  item->link.data = item;

  dex_object_lock (main_scheduler);
  g_queue_push_tail_link (&main_scheduler->work_queue, &item->link);
  dex_object_unlock (main_scheduler);

  if G_UNLIKELY (scheduler != dex_thread_storage_get ()->scheduler)
    g_main_context_wakeup (main_scheduler->main_context);
}

static GMainContext *
dex_main_scheduler_get_main_context (DexScheduler *scheduler)
{
  DexMainScheduler *main_scheduler = DEX_MAIN_SCHEDULER (scheduler);

  g_assert (DEX_IS_MAIN_SCHEDULER (main_scheduler));

  return main_scheduler->main_context;
}

static DexAioContext *
dex_main_scheduler_get_aio_context (DexScheduler *scheduler)
{
  DexMainScheduler *main_scheduler = DEX_MAIN_SCHEDULER (scheduler);

  g_assert (DEX_IS_MAIN_SCHEDULER (main_scheduler));

  return (DexAioContext *)main_scheduler->aio_context;
}

static void
dex_main_scheduler_spawn (DexScheduler *scheduler,
                          DexFiber     *fiber)
{
  DexMainScheduler *main_scheduler = DEX_MAIN_SCHEDULER (scheduler);

  g_assert (DEX_IS_MAIN_SCHEDULER (main_scheduler));

  dex_fiber_scheduler_register ((DexFiberScheduler *)main_scheduler->fiber_scheduler, fiber);
}

static void
dex_main_scheduler_finalize (DexObject *object)
{
  DexMainScheduler *main_scheduler = DEX_MAIN_SCHEDULER (object);

  /* Flush out any pending work items */
  while (main_scheduler->work_queue.length)
    {
      DexMainWorkQueueItem *item = g_queue_pop_head_link (&main_scheduler->work_queue)->data;
      dex_work_item_invoke (&item->work_item);
      g_free (item);
    }

  /* Clear DexAioBackend context */
  g_source_destroy (main_scheduler->aio_context);
  g_clear_pointer (&main_scheduler->aio_context, g_source_unref);

  /* Clear DexFiberScheduler context */
  g_source_destroy (main_scheduler->fiber_scheduler);
  g_clear_pointer (&main_scheduler->fiber_scheduler, g_source_unref);

  /* Clear work queue source */
  g_source_destroy (main_scheduler->work_queue_source);
  g_clear_pointer (&main_scheduler->work_queue_source, g_source_unref);

  /* Release our main context */
  g_clear_pointer (&main_scheduler->main_context, g_main_context_unref);

  DEX_OBJECT_CLASS (dex_main_scheduler_parent_class)->finalize (object);
}

static void
dex_main_scheduler_class_init (DexMainSchedulerClass *main_scheduler_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (main_scheduler_class);
  DexSchedulerClass *scheduler_class = DEX_SCHEDULER_CLASS (main_scheduler_class);

  object_class->finalize = dex_main_scheduler_finalize;

  scheduler_class->get_aio_context = dex_main_scheduler_get_aio_context;
  scheduler_class->get_main_context = dex_main_scheduler_get_main_context;
  scheduler_class->push = dex_main_scheduler_push;
  scheduler_class->spawn = dex_main_scheduler_spawn;
}

static void
dex_main_scheduler_init (DexMainScheduler *main_scheduler)
{
}

DexMainScheduler *
dex_main_scheduler_new (GMainContext *main_context)
{
  DexMainWorkQueueSource *work_queue_source;
  DexFiberScheduler *fiber_scheduler;
  DexMainScheduler *main_scheduler;
  DexAioBackend *aio_backend;
  DexAioContext *aio_context;

  if (main_context == NULL)
    main_context = g_main_context_default ();

  aio_backend = dex_aio_backend_get_default ();
  aio_context = dex_aio_backend_create_context (aio_backend);

  fiber_scheduler = dex_fiber_scheduler_new ();

  main_scheduler = (DexMainScheduler *)dex_object_create_instance (DEX_TYPE_MAIN_SCHEDULER);
  main_scheduler->main_context = g_main_context_ref (main_context);
  main_scheduler->aio_context = (GSource *)aio_context;
  main_scheduler->fiber_scheduler = (GSource *)fiber_scheduler;

  work_queue_source = (DexMainWorkQueueSource *)
    g_source_new (&dex_main_work_queue_source_funcs, sizeof *work_queue_source);
  work_queue_source->object = DEX_OBJECT (main_scheduler);
  work_queue_source->queue = &main_scheduler->work_queue;
  main_scheduler->work_queue_source = (GSource *)work_queue_source;

  dex_thread_storage_get ()->aio_context = aio_context;
  dex_thread_storage_get ()->scheduler = DEX_SCHEDULER (main_scheduler);

  g_source_attach (main_scheduler->aio_context, main_context);
  g_source_attach (main_scheduler->fiber_scheduler, main_context);
  g_source_attach (main_scheduler->work_queue_source, main_context);

  return main_scheduler;
}
