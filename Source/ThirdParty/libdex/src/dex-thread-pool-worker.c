/*
 * dex-thread-pool-worker.c
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

#include <stdatomic.h>

#include "dex-aio-backend-private.h"
#include "dex-compat-private.h"
#include "dex-fiber-private.h"
#include "dex-thread-pool-worker-private.h"
#include "dex-thread-storage-private.h"
#include "dex-work-stealing-queue-private.h"
#include "dex-work-queue-private.h"

typedef enum _DexThreadPoolWorkerStatus
{
  DEX_THREAD_POOL_WORKER_INITIAL,
  DEX_THREAD_POOL_WORKER_RUNNING,
  DEX_THREAD_POOL_WORKER_STOPPING,
  DEX_THREAD_POOL_WORKER_FINISHED,
} DexThreadPoolWorkerStatus;

struct _DexThreadPoolWorker
{
  DexScheduler               parent_instance;

  GList                      set_link;
  DexThreadPoolWorkerSet    *set;

  GThread                   *thread;
  GMainContext              *main_context;
  GMainLoop                 *main_loop;
  DexAioContext             *aio_context;
  DexWorkQueue              *global_work_queue;
  DexWorkStealingQueue      *work_stealing_queue;
  GSource                   *set_source;
  GSource                   *local_source;
  GSource                   *fiber_scheduler;

  GMutex                     setup_mutex;
  GCond                      setup_cond;

  DexThreadPoolWorkerStatus  status : 2;
};

typedef struct _DexThreadPoolWorkerClass
{
  DexSchedulerClass parent_class;
} DexThreadPoolWorkerClass;

DEX_DEFINE_FINAL_TYPE (DexThreadPoolWorker, dex_thread_pool_worker, DEX_TYPE_SCHEDULER)

#undef DEX_TYPE_THREAD_POOL_WORKER
#define DEX_TYPE_THREAD_POOL_WORKER dex_thread_pool_worker_type

static void     dex_thread_pool_worker_set_add           (DexThreadPoolWorkerSet *set,
                                                          DexThreadPoolWorker    *thread_pool_worker);
static void     dex_thread_pool_worker_set_remove        (DexThreadPoolWorkerSet *set,
                                                          DexThreadPoolWorker    *thread_pool_worker);
static GSource *dex_thread_pool_worker_set_create_source (DexThreadPoolWorkerSet *set,
                                                          DexThreadPoolWorker    *thread_pool_worker);

static gboolean
dex_thread_pool_worker_work_item_cb (gpointer user_data)
{
  DexWorkItem *work_item = user_data;
  dex_work_item_invoke (work_item);
  return G_SOURCE_REMOVE;
}

static void
dex_thread_pool_worker_push (DexScheduler *scheduler,
                             DexWorkItem   work_item)
{
  DexThreadPoolWorker *thread_pool_worker = DEX_THREAD_POOL_WORKER (scheduler);

  g_assert (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker));

  if G_LIKELY (g_thread_self () == thread_pool_worker->thread &&
               thread_pool_worker->status == DEX_THREAD_POOL_WORKER_RUNNING)
    dex_work_stealing_queue_push (thread_pool_worker->work_stealing_queue, work_item);
  else
    {
      GSource *source;

      /* Pushing a work item directly onto a worker is generally going
       * to be related to completing work items. Treat those as extremely
       * high priority as they will delay further processing of futures.
       *
       * This is currently a workaround to improve the situation with
       * issue #17 when we're on fallback configurations. If we want
       * to improve that situation further, we can reduce some overhead
       * here by creating a dedicated GSource like DexMainScheduler does
       * for work items instead of a 1:1 GSource creation.
       *
       * But this at least improves the situation immediately that is
       * causing potential lock-ups.
       */

      source = g_idle_source_new ();
      g_source_set_priority (source, G_MININT);
      g_source_set_callback (source,
                             dex_thread_pool_worker_work_item_cb,
                             g_memdup2 (&work_item, sizeof work_item),
                             g_free);
      g_source_attach (source, thread_pool_worker->main_context);
      g_source_unref (source);
    }
}

static gboolean
dex_thread_pool_worker_finalize_cb (gpointer data)
{
  DexThreadPoolWorker *thread_pool_worker = DEX_THREAD_POOL_WORKER (data);
  DexWorkItem work_item;

  /* First set the status to ensure that our check/dispatch will bail */
  thread_pool_worker->status = DEX_THREAD_POOL_WORKER_STOPPING;
  g_main_loop_quit (thread_pool_worker->main_loop);

  /* Now flush out the rest of the work items if there are any */
  while (dex_work_stealing_queue_pop (thread_pool_worker->work_stealing_queue, &work_item))
    dex_work_item_invoke (&work_item);

  return G_SOURCE_REMOVE;
}

static void
dex_thread_pool_worker_finalize (DexObject *object)
{
  DexThreadPoolWorker *thread_pool_worker = DEX_THREAD_POOL_WORKER (object);
  GSource *idle_source;

  g_assert (thread_pool_worker->thread != g_thread_self ());

  /* To finalize the worker, we need to push a work item to the thread
   * that will cause it to stop processing and then wait for the thread
   * to exit after processing the rest of the queue. We change the finalizing
   * bit on the worker thread so the common case (it's FALSE) doesn't require
   * any sort of synchronization/atomic operations every loop.
   */
  idle_source = g_idle_source_new ();
  _g_source_set_static_name (idle_source, "[dex-thread-pool-worker-finalize]");
  g_source_set_priority (idle_source, G_MININT);
  g_source_set_callback (idle_source,
                         dex_thread_pool_worker_finalize_cb,
                         thread_pool_worker, NULL);
  g_source_attach (idle_source, thread_pool_worker->main_context);
  g_source_unref (idle_source);

  /* Now wait for the thread to process items and exit the thread */
  g_thread_join (thread_pool_worker->thread);

#ifdef G_ENABLE_DEBUG
  atomic_thread_fence (memory_order_seq_cst);

  g_assert (thread_pool_worker->status == DEX_THREAD_POOL_WORKER_FINISHED);
  g_assert (dex_work_stealing_queue_empty (thread_pool_worker->queue));
#endif

  /* These are all destroyed during thread shutdown */
  g_clear_pointer (&thread_pool_worker->set_source, g_source_unref);
  g_clear_pointer (&thread_pool_worker->local_source, g_source_unref);
  g_clear_pointer (&thread_pool_worker->fiber_scheduler, g_source_unref);

  g_clear_pointer (&thread_pool_worker->thread, g_thread_unref);
  g_clear_pointer (&thread_pool_worker->main_context, g_main_context_unref);
  g_clear_pointer (&thread_pool_worker->main_loop, g_main_loop_unref);
  g_clear_pointer (&thread_pool_worker->work_stealing_queue, dex_work_stealing_queue_unref);

  dex_clear (&thread_pool_worker->global_work_queue);

  g_assert (thread_pool_worker->set_link.prev == NULL);
  g_assert (thread_pool_worker->set_link.next == NULL);

  g_mutex_clear (&thread_pool_worker->setup_mutex);
  g_cond_clear (&thread_pool_worker->setup_cond);

  DEX_OBJECT_CLASS (dex_thread_pool_worker_parent_class)->finalize (object);
}

static GMainContext *
dex_thread_pool_worker_get_main_context (DexScheduler *scheduler)
{
  DexThreadPoolWorker *thread_pool_worker = DEX_THREAD_POOL_WORKER (scheduler);

  g_assert (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker));

  return thread_pool_worker->main_context;
}

static DexAioContext *
dex_thread_pool_worker_get_aio_context (DexScheduler *scheduler)
{
  DexThreadPoolWorker *thread_pool_worker = DEX_THREAD_POOL_WORKER (scheduler);

  g_assert (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker));

  return thread_pool_worker->aio_context;
}

static void
dex_thread_pool_worker_spawn (DexScheduler *scheduler,
                              DexFiber     *fiber)
{
  DexThreadPoolWorker *thread_pool_worker = DEX_THREAD_POOL_WORKER (scheduler);

  g_assert (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker));

  dex_fiber_scheduler_register ((DexFiberScheduler *)thread_pool_worker->fiber_scheduler, fiber);
}

static void
dex_thread_pool_worker_class_init (DexThreadPoolWorkerClass *thread_pool_worker_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (thread_pool_worker_class);
  DexSchedulerClass *scheduler_class = DEX_SCHEDULER_CLASS (thread_pool_worker_class);

  object_class->finalize = dex_thread_pool_worker_finalize;

  scheduler_class->push = dex_thread_pool_worker_push;
  scheduler_class->get_main_context = dex_thread_pool_worker_get_main_context;
  scheduler_class->spawn = dex_thread_pool_worker_spawn;
  scheduler_class->get_aio_context = dex_thread_pool_worker_get_aio_context;
}

static void
dex_thread_pool_worker_init (DexThreadPoolWorker *thread_pool_worker)
{
  thread_pool_worker->set_link.data = thread_pool_worker;

  g_mutex_init (&thread_pool_worker->setup_mutex);
  g_cond_init (&thread_pool_worker->setup_cond);
}

static gpointer
dex_thread_pool_worker_thread_func (gpointer data)
{
  DexThreadPoolWorker *thread_pool_worker = DEX_THREAD_POOL_WORKER (data);
  DexThreadStorage *storage = dex_thread_storage_get ();
  DexAioBackend *aio_backend;
  DexAioContext *aio_context;
  DexFuture *global_work_queue_loop;
  GSource *source;

  g_mutex_lock (&thread_pool_worker->setup_mutex);

  aio_backend = dex_aio_backend_get_default ();
  aio_context = dex_aio_backend_create_context (aio_backend);

  /* If we fail to setup an AIO context, then there is no point in
   * adding this thread pool worker. Just bail immediately and notify
   * the creator of the issue.
   */
  if (aio_context == NULL)
    {
      thread_pool_worker->status = DEX_THREAD_POOL_WORKER_FINISHED;
      g_cond_signal (&thread_pool_worker->setup_cond);
      g_mutex_unlock (&thread_pool_worker->setup_mutex);
      return NULL;
    }

  thread_pool_worker->aio_context = g_steal_pointer (&aio_context);

  /* Attach our AIO source to complete AIO work */
  g_source_attach ((GSource *)thread_pool_worker->aio_context,
                   thread_pool_worker->main_context);

  /* Attach a GSource that will process items from the worker threads
   * work queue.
   */
  source = dex_work_stealing_queue_create_source (thread_pool_worker->work_stealing_queue);
  g_source_set_priority (source, G_PRIORITY_DEFAULT);
  g_source_attach (source, thread_pool_worker->main_context);
  thread_pool_worker->local_source = g_steal_pointer (&source);

  /* Create a source to steal work items from other thread pool threads.
   * This is slightly higher priority than the global queue because we
   * want to steal items from the peers before the global queue.
   */
  source = dex_thread_pool_worker_set_create_source (thread_pool_worker->set, thread_pool_worker);
  g_source_set_priority (source, G_PRIORITY_DEFAULT_IDLE-1);
  g_source_attach (source, thread_pool_worker->main_context);
  thread_pool_worker->set_source = g_steal_pointer (&source);

  /* Setup fiber scheduler source */
  source = (GSource *)dex_fiber_scheduler_new ();
  g_source_attach (source, thread_pool_worker->main_context);
  thread_pool_worker->fiber_scheduler = g_steal_pointer (&source);

  storage->scheduler = DEX_SCHEDULER (thread_pool_worker);
  storage->worker = thread_pool_worker;
  storage->aio_context = thread_pool_worker->aio_context;

  g_main_context_push_thread_default (thread_pool_worker->main_context);
  thread_pool_worker->status = DEX_THREAD_POOL_WORKER_RUNNING;

  /* Add to set so others may steal work items from us */
  dex_thread_pool_worker_set_add (thread_pool_worker->set, thread_pool_worker);

  /* Async processing global work-queue items until we're told to shutdown */
  global_work_queue_loop = dex_work_queue_run (thread_pool_worker->global_work_queue);

  /* Notify the caller that we are all setup */
  g_cond_signal (&thread_pool_worker->setup_cond);
  g_mutex_unlock (&thread_pool_worker->setup_mutex);

  /* Process main context until we are told to shutdown */
  g_main_loop_run (thread_pool_worker->main_loop);

  /* Discard our work queue loop */
  dex_clear (&global_work_queue_loop);

  /* Flush out any pending operations */
  while (g_main_context_pending (thread_pool_worker->main_context))
    g_main_context_iteration (thread_pool_worker->main_context, FALSE);

  /* Remove from set now so others will not try to steal from us */
  dex_thread_pool_worker_set_remove (thread_pool_worker->set, thread_pool_worker);

  /* Ensure our sources will not continue on */
  g_source_destroy (thread_pool_worker->set_source);
  g_source_destroy (thread_pool_worker->local_source);
  g_source_destroy (thread_pool_worker->fiber_scheduler);

  thread_pool_worker->status = DEX_THREAD_POOL_WORKER_FINISHED;
  g_main_context_pop_thread_default (thread_pool_worker->main_context);

  storage->worker = NULL;
  storage->scheduler = NULL;
  storage->aio_context = NULL;

  return NULL;
}

static gboolean
dex_thread_pool_worker_maybe_steal (DexThreadPoolWorker *thread_pool_worker,
                                    DexThreadPoolWorker *neighbor)
{
  DexWorkItem work_item;

  g_assert (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker));
  g_assert (DEX_IS_THREAD_POOL_WORKER (neighbor));

  if (dex_work_stealing_queue_steal (neighbor->work_stealing_queue, &work_item))
    {
      dex_work_item_invoke (&work_item);
      return TRUE;
    }

  return FALSE;
}

typedef struct _DexThreadPoolWorkerSet
{
  GQueue  queue;
  GRWLock rwlock;
} DexThreadPoolWorkerSet;

DexThreadPoolWorkerSet *
dex_thread_pool_worker_set_new (void)
{
  DexThreadPoolWorkerSet *set;

  set = g_atomic_rc_box_new0 (DexThreadPoolWorkerSet);
  g_rw_lock_init (&set->rwlock);

  return set;
}

static void
dex_thread_pool_worker_set_add (DexThreadPoolWorkerSet *set,
                                DexThreadPoolWorker    *thread_pool_worker)
{
  g_return_if_fail (set != NULL);
  g_return_if_fail (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker));
  g_return_if_fail (thread_pool_worker->set_link.prev == NULL);
  g_return_if_fail (thread_pool_worker->set_link.next == NULL);

  g_rw_lock_writer_lock (&set->rwlock);
  g_queue_push_tail_link (&set->queue, &thread_pool_worker->set_link);
  g_rw_lock_writer_unlock (&set->rwlock);
}

static void
dex_thread_pool_worker_set_remove (DexThreadPoolWorkerSet *set,
                                   DexThreadPoolWorker    *thread_pool_worker)
{
  g_return_if_fail (set != NULL);
  g_return_if_fail (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker));

  g_rw_lock_writer_lock (&set->rwlock);
  g_queue_unlink (&set->queue, &thread_pool_worker->set_link);
  g_rw_lock_writer_unlock (&set->rwlock);
}

DexThreadPoolWorkerSet *
dex_thread_pool_worker_set_ref (DexThreadPoolWorkerSet *set)
{
  g_atomic_rc_box_acquire (set);
  return set;
}

static void
dex_thread_pool_worker_set_finalize (gpointer data)
{
  DexThreadPoolWorkerSet *set = data;

  while (set->queue.length > 0)
    dex_thread_pool_worker_set_remove (set, g_queue_peek_head (&set->queue));

  g_rw_lock_clear (&set->rwlock);
}

void
dex_thread_pool_worker_set_unref (DexThreadPoolWorkerSet *set)
{
  g_atomic_rc_box_release_full (set, dex_thread_pool_worker_set_finalize);
}

static inline void
dex_thread_pool_worker_set_foreach (DexThreadPoolWorkerSet *set,
                                    DexThreadPoolWorker    *head)
{
  g_rw_lock_reader_lock (&set->rwlock);

  for (const GList *iter = head->set_link.next; iter; iter = iter->next)
    {
      if (dex_thread_pool_worker_maybe_steal (head, iter->data))
        goto unlock;
    }

  for (const GList *iter = set->queue.head; iter->data != head; iter = iter->next)
    {
      if (dex_thread_pool_worker_maybe_steal (head, iter->data))
        goto unlock;
    }

unlock:
  g_rw_lock_reader_unlock (&set->rwlock);
}

typedef struct _DexThreadPoolWorkerSetSource
{
  GSource                 parent_source;
  DexThreadPoolWorkerSet *set;
  DexThreadPoolWorker    *thread_pool_worker;
} DexThreadPoolWorkerSetSource;

static gboolean
dex_thread_pool_worker_set_source_check (GSource *source)
{
  /* We always check the peers for work */
  return TRUE;
}

static gboolean
dex_thread_pool_worker_set_source_dispatch (GSource     *source,
                                            GSourceFunc  callback,
                                            gpointer     callback_data)
{
  DexThreadPoolWorkerSetSource *real_source = (DexThreadPoolWorkerSetSource *)source;
  dex_thread_pool_worker_set_foreach (real_source->set, real_source->thread_pool_worker);
  return G_SOURCE_CONTINUE;
}

static GSourceFuncs dex_thread_pool_worker_set_source_funcs = {
  .check = dex_thread_pool_worker_set_source_check,
  .dispatch = dex_thread_pool_worker_set_source_dispatch,
};

static GSource *
dex_thread_pool_worker_set_create_source (DexThreadPoolWorkerSet *set,
                                          DexThreadPoolWorker    *thread_pool_worker)
{
  DexThreadPoolWorkerSetSource *source;

  g_return_val_if_fail (set != NULL, NULL);
  g_return_val_if_fail (DEX_IS_THREAD_POOL_WORKER (thread_pool_worker), NULL);

  source = (DexThreadPoolWorkerSetSource *)
    g_source_new (&dex_thread_pool_worker_set_source_funcs, sizeof *source);
  _g_source_set_static_name ((GSource *)source, "[dex-thread-pool-worker-set]");
  source->set = set;
  source->thread_pool_worker = thread_pool_worker;

  return (GSource *)source;
}

DexThreadPoolWorker *
dex_thread_pool_worker_new (DexWorkQueue           *work_queue,
                            DexThreadPoolWorkerSet *set)
{
  DexThreadPoolWorker *thread_pool_worker;
  gboolean failed;

  g_return_val_if_fail (work_queue != NULL, NULL);
  g_return_val_if_fail (set != NULL, NULL);

  thread_pool_worker = (DexThreadPoolWorker *)dex_object_create_instance (DEX_TYPE_THREAD_POOL_WORKER);
  thread_pool_worker->main_context = g_main_context_new ();
  thread_pool_worker->main_loop = g_main_loop_new (thread_pool_worker->main_context, FALSE);
  thread_pool_worker->global_work_queue = dex_ref (work_queue);
  thread_pool_worker->work_stealing_queue = dex_work_stealing_queue_new (255);
  thread_pool_worker->set = set;

  /* Now spawn our thread to process events via GSource */
  g_mutex_lock (&thread_pool_worker->setup_mutex);
  thread_pool_worker->thread = g_thread_new ("dex-thread-pool-worker",
                                             dex_thread_pool_worker_thread_func,
                                             thread_pool_worker);
  g_cond_wait (&thread_pool_worker->setup_cond, &thread_pool_worker->setup_mutex);
  failed = thread_pool_worker->status == DEX_THREAD_POOL_WORKER_FINISHED;
  g_mutex_unlock (&thread_pool_worker->setup_mutex);

  if (failed)
    dex_clear (&thread_pool_worker);

  return thread_pool_worker;
}
