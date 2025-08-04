/*
 * dex-scheduler.c
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

#include "dex-aio-backend-private.h"
#include "dex-fiber-private.h"
#include "dex-scheduler-private.h"
#include "dex-thread-storage-private.h"

/**
 * DexScheduler:
 *
 * #DexScheduler is the base class used by schedulers.
 *
 * Schedulers are responsible for ensuring asynchronous IO requests and
 * completions are processed. They also schedule closures to be run as part
 * of future result propagation. Additionally, they manage #DexFiber execution
 * and suspension.
 *
 * Specialized schedulers such as #DexThreadPoolScheduler will do this for a
 * number of threads and dispatch new work between them.
 */

static DexScheduler *default_scheduler;

DEX_DEFINE_ABSTRACT_TYPE (DexScheduler, dex_scheduler, DEX_TYPE_OBJECT)

#undef DEX_TYPE_SCHEDULER
#define DEX_TYPE_SCHEDULER dex_scheduler_type

static void
dex_scheduler_class_init (DexSchedulerClass *scheduler_class)
{
}

static void
dex_scheduler_init (DexScheduler *scheduler)
{
}

/**
 * dex_scheduler_get_default:
 *
 * Gets the default scheduler for the process.
 *
 * The default scheduler executes tasks within the default #GMainContext.
 * Typically that is the main thread of the application.
 *
 * Returns: (transfer none) (not nullable): a #DexScheduler
 */
DexScheduler *
dex_scheduler_get_default (void)
{
  return default_scheduler;
}

void
dex_scheduler_set_default (DexScheduler *scheduler)
{
  g_return_if_fail (default_scheduler == NULL);
  g_return_if_fail (scheduler != NULL);

  default_scheduler = scheduler;
}

/**
 * dex_scheduler_get_thread_default:
 *
 * Gets the default scheduler for the thread.
 *
 * Returns: (transfer none) (nullable): a #DexScheduler or %NULL
 */
DexScheduler *
dex_scheduler_get_thread_default (void)
{
  return dex_thread_storage_get ()->scheduler;
}

void
dex_scheduler_set_thread_default (DexScheduler *scheduler)
{
  dex_thread_storage_get ()->scheduler = scheduler;
}

/**
 * dex_scheduler_ref_thread_default:
 *
 * Gets the thread default scheduler with the reference count incremented.
 *
 * Returns: (transfer full) (nullable): a #DexScheduler or %NULL
 */
DexScheduler *
dex_scheduler_ref_thread_default (void)
{
  DexScheduler *scheduler = dex_scheduler_get_thread_default ();

  if (scheduler != NULL)
    return dex_ref (scheduler);

  return NULL;
}

/**
 * dex_scheduler_push:
 * @scheduler: a #DexScheduler
 * @func: (scope async): the function callback
 * @func_data: the closure data for @func
 *
 * Queues @func to run on @scheduler.
 */
void
dex_scheduler_push (DexScheduler     *scheduler,
                    DexSchedulerFunc  func,
                    gpointer          func_data)
{
  g_return_if_fail (DEX_IS_SCHEDULER (scheduler));
  g_return_if_fail (func != NULL);

  DEX_SCHEDULER_GET_CLASS (scheduler)->push (scheduler, (DexWorkItem) {func, func_data});
}

/**
 * dex_scheduler_get_main_context:
 * @scheduler: a #DexScheduler
 *
 * Gets the default main context for a scheduler.
 *
 * This may be a different value depending on the calling thread.
 *
 * For example, calling this on the #DexThreadPoolScheduer from outside
 * a worker thread may result in getting a shared #GMainContext for the
 * process.
 *
 * However, calling from a worker thread may give you a #GMainContext
 * specifically for that thread.
 *
 * Returns: (transfer none): a #GMainContext
 */
GMainContext *
dex_scheduler_get_main_context (DexScheduler *scheduler)
{
  return DEX_SCHEDULER_GET_CLASS (scheduler)->get_main_context (scheduler);
}

/**
 * dex_scheduler_get_aio_context: (skip)
 * @scheduler: a #DexScheduler
 *
 * Gets a #DexAioContext for the scheduler.
 *
 * This context can be used to execute asyncronous operations within the
 * context of the scheduler. Generally this is done using asynchronous
 * operations and submission/completions managed by the threads scheduler.
 */
DexAioContext *
dex_scheduler_get_aio_context (DexScheduler *scheduler)
{
  return DEX_SCHEDULER_GET_CLASS (scheduler)->get_aio_context (scheduler);
}

/**
 * dex_scheduler_spawn:
 * @scheduler: (nullable): a #DexScheduler
 * @stack_size: stack size in bytes or 0
 * @func: (scope notified) (closure func_data) (destroy func_data_destroy): a #DexFiberFunc
 * @func_data: closure data for @func
 * @func_data_destroy: closure notify for @func_data
 *
 * Request @scheduler to spawn a #DexFiber.
 *
 * The fiber will have its own stack and cooperatively schedules among other
 * fibers sharing the schaeduler.
 *
 * If @stack_size is 0, it will set to a sensible default. Otherwise, it is
 * rounded up to the nearest page size.
 *
 * Returns: (transfer full): a #DexFuture that will resolve or reject when
 *   @func completes (or its resulting #DexFuture completes).
 */
DexFuture *
dex_scheduler_spawn (DexScheduler   *scheduler,
                     gsize           stack_size,
                     DexFiberFunc    func,
                     gpointer        func_data,
                     GDestroyNotify  func_data_destroy)
{
  DexFiber *fiber;

  g_return_val_if_fail (!scheduler || DEX_IS_SCHEDULER (scheduler), NULL);
  g_return_val_if_fail (func != NULL, NULL);

  if (scheduler == NULL)
    scheduler = dex_scheduler_get_default ();

  fiber = dex_fiber_new (func, func_data, func_data_destroy, stack_size);
  DEX_SCHEDULER_GET_CLASS (scheduler)->spawn (scheduler, fiber);
  return DEX_FUTURE (fiber);
}
