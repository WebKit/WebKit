/*
 * dex-thread-pool-scheduler.c
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

#include "config.h"

#include "dex-scheduler-private.h"
#include "dex-thread-pool-scheduler.h"
#include "dex-thread-pool-worker-private.h"
#include "dex-thread-storage-private.h"
#include "dex-work-queue-private.h"

/**
 * DexThreadPoolScheduler:
 *
 * #DexThreadPoolScheduler is a #DexScheduler that will dispatch work items
 * and fibers to sub-schedulers on a specific operating system thread.
 *
 * #DexFiber will never migrate from the thread they are created on to reduce
 * chances of safety issues involved in tracking state between CPU.
 *
 * New work items are placed into a global work queue and then dispatched
 * efficiently to a single thread pool worker using a specialized async
 * semaphore. On modern Linux using io_uring, this wakes up a single worker
 * thread and therefore is not subject to "thundering herd" common with
 * global work queues.
 *
 * When a worker creates a new work item, it is placed into a work stealing
 * queue owned by the thread. Other worker threads may steal work items when
 * they have exhausted their own work queue.
 */

struct _DexThreadPoolScheduler
{
  DexScheduler            parent_instance;
  DexWorkQueue           *global_work_queue;
  DexThreadPoolWorkerSet *set;
  GPtrArray              *workers;
  guint                   fiber_rrobin;
};

typedef struct _DexThreadPoolSchedulerClass
{
  DexSchedulerClass parent_class;
} DexThreadPoolSchedulerClass;

DEX_DEFINE_FINAL_TYPE (DexThreadPoolScheduler, dex_thread_pool_scheduler, DEX_TYPE_SCHEDULER)

#undef DEX_TYPE_THREAD_POOL_SCHEDULER
#define DEX_TYPE_THREAD_POOL_SCHEDULER dex_thread_pool_scheduler_type

static void
dex_thread_pool_scheduler_push (DexScheduler *scheduler,
                                DexWorkItem   work_item)
{
  DexThreadPoolScheduler *thread_pool_scheduler = DEX_THREAD_POOL_SCHEDULER (scheduler);
  DexThreadPoolWorker *worker = DEX_THREAD_POOL_WORKER_CURRENT;

  if (worker != NULL)
    DEX_SCHEDULER_GET_CLASS (worker)->push (DEX_SCHEDULER (worker), work_item);
  else
    dex_work_queue_push (thread_pool_scheduler->global_work_queue, work_item);
}

static GMainContext *
dex_thread_pool_scheduler_get_main_context (DexScheduler *scheduler)
{
  DexThreadPoolWorker *worker = DEX_THREAD_POOL_WORKER_CURRENT;

  /* Give the worker's main context if we're on a pooled thread */
  if (worker != NULL)
    return dex_scheduler_get_main_context (DEX_SCHEDULER (worker));

  /* Otherwise give the application default (main thread) context */
  return dex_scheduler_get_main_context (dex_scheduler_get_default ());
}

static DexAioContext *
dex_thread_pool_scheduler_get_aio_context (DexScheduler *scheduler)
{
  DexThreadPoolWorker *worker = DEX_THREAD_POOL_WORKER_CURRENT;

  /* Give the worker's aio context if we're on a pooled thread */
  if (worker != NULL)
    return dex_scheduler_get_aio_context (DEX_SCHEDULER (worker));

  /* Otherwise give the application default (main thread) aio context */
  return dex_scheduler_get_aio_context (dex_scheduler_get_default ());
}

static void
dex_thread_pool_scheduler_spawn (DexScheduler *scheduler,
                                 DexFiber     *fiber)
{
  DexThreadPoolScheduler *thread_pool_scheduler = (DexThreadPoolScheduler *)scheduler;
  guint worker_index = g_atomic_int_add (&thread_pool_scheduler->fiber_rrobin, 1) % thread_pool_scheduler->workers->len;
  DexThreadPoolWorker *worker = thread_pool_scheduler->workers->pdata[worker_index];

  /* TODO: This is just doing a dumb round robin for assigning a fiber to a
   * specific thread pool worker. We probably want something more interesting
   * than that so we can have weighted workers or even keep affinity to a small
   * number of them until latency reaches some threshold.
   */

  DEX_SCHEDULER_GET_CLASS (worker)->spawn (DEX_SCHEDULER (worker), fiber);
}

static void
dex_thread_pool_scheduler_finalize (DexObject *object)
{
  DexThreadPoolScheduler *thread_pool_scheduler = (DexThreadPoolScheduler *)object;

  dex_clear (&thread_pool_scheduler->global_work_queue);

  g_clear_pointer (&thread_pool_scheduler->set, dex_thread_pool_worker_set_unref);
  g_clear_pointer (&thread_pool_scheduler->workers, g_ptr_array_unref);

  DEX_OBJECT_CLASS (dex_thread_pool_scheduler_parent_class)->finalize (object);
}

static void
dex_thread_pool_scheduler_class_init (DexThreadPoolSchedulerClass *thread_pool_scheduler_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (thread_pool_scheduler_class);
  DexSchedulerClass *scheduler_class = DEX_SCHEDULER_CLASS (thread_pool_scheduler_class);

  object_class->finalize = dex_thread_pool_scheduler_finalize;

  scheduler_class->get_main_context = dex_thread_pool_scheduler_get_main_context;
  scheduler_class->get_aio_context = dex_thread_pool_scheduler_get_aio_context;
  scheduler_class->push = dex_thread_pool_scheduler_push;
  scheduler_class->spawn = dex_thread_pool_scheduler_spawn;
}

static void
dex_thread_pool_scheduler_init (DexThreadPoolScheduler *thread_pool_scheduler)
{
  thread_pool_scheduler->global_work_queue = dex_work_queue_new ();
  thread_pool_scheduler->set = dex_thread_pool_worker_set_new ();
  thread_pool_scheduler->workers = g_ptr_array_new_with_free_func (dex_unref);
}

/**
 * dex_thread_pool_scheduler_new:
 *
 * Creates a new #DexScheduler that executes work items on a thread pool.
 *
 * Returns: (transfer full): a #DexThreadPoolScheduler
 */
DexScheduler *
dex_thread_pool_scheduler_new (void)
{
  DexThreadPoolScheduler *thread_pool_scheduler;
  guint n_procs;
  guint n_workers;

  thread_pool_scheduler = (DexThreadPoolScheduler *)dex_object_create_instance (DEX_TYPE_THREAD_POOL_SCHEDULER);

  /* TODO: let this be dynamic and tunable, as well as thread pinning */

  n_procs = MIN (32, g_get_num_processors ());

  /* Couple things here, which we should take a look at in the future to
   * see how we can tune them correctly, but:
   *
   * Remove one as the main thread has an AIO context too and we don't want
   * to create contention there. Also, io_uring may limit us in the number
   * of io_uring we can create.
   *
   * g_get_num_processors() includes hyperthreads, so take the result and
   * cut it in half. It would be nicer to actually verify this on the system
   * for cases where we don't have that.
   *
   * Additionally, bail if the worker fails to be created.
   */
  n_workers = MAX (1, (n_procs/2));

  for (guint i = 0; i < n_workers; i++)
    {
      DexThreadPoolWorker *thread_pool_worker;

      thread_pool_worker = dex_thread_pool_worker_new (thread_pool_scheduler->global_work_queue,
                                                       thread_pool_scheduler->set);

      if (thread_pool_worker != NULL)
        g_ptr_array_add (thread_pool_scheduler->workers, g_steal_pointer (&thread_pool_worker));
      else
        break;
    }

  return DEX_SCHEDULER (thread_pool_scheduler);
}

/**
 * dex_thread_pool_scheduler_get_default:
 *
 * Gets the default thread pool scheduler for the instance.
 *
 * This function is useful to allow programs and libraries to share
 * an off-main-thread scheduler without having to coordinate on where
 * the scheduler instance is created or owned.
 *
 * Returns: (transfer none): a #DexScheduler
 */
DexScheduler *
dex_thread_pool_scheduler_get_default (void)
{
  static DexScheduler *default_thread_pool;

  if (g_once_init_enter (&default_thread_pool))
    {
      DexScheduler *instance = dex_thread_pool_scheduler_new ();
      g_once_init_leave (&default_thread_pool, instance);
    }

  return default_thread_pool;
}
