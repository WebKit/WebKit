/* dex-fiber.c
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

#include <glib.h>

#include "dex-compat-private.h"
#include "dex-error.h"
#include "dex-fiber-private.h"
#include "dex-object-private.h"
#include "dex-platform.h"
#include "dex-thread-storage-private.h"

/**
 * DexFiber:
 *
 * #DexFiber is a fiber (or coroutine) which itself is a #DexFuture.
 *
 * When the fiber completes execution it will either resolve or reject the
 * with the result or error.
 *
 * You may treat a #DexFiber like any other #DexFuture which makes it simple
 * to integrate fibers into other processing chains.
 *
 * #DexFiber are provided their own stack seperate from a threads main stack,
 * They are automatically scheduled as necessary.
 *
 * Use dex_await() and similar functions to await the result of another future
 * within the fiber and the fiber will be suspended allowing another fiber to
 * run and/or the rest of the applications main loop.
 *
 * Once a fiber is created, it is pinned to that scheduler. Use
 * dex_scheduler_spawn() to create a fiber on a specific scheduler.
 */

typedef struct _DexFiberClass
{
  DexFutureClass parent_class;
} DexFiberClass;

DEX_DEFINE_FINAL_TYPE (DexFiber, dex_fiber, DEX_TYPE_FUTURE)

#undef DEX_TYPE_FIBER
#define DEX_TYPE_FIBER dex_fiber_type

static void dex_fiber_start (DexFiber *fiber);

static DexFuture *cancelled_future;

static void
dex_fiber_discard (DexFuture *future)
{
  DexFiber *fiber = DEX_FIBER (future);

  g_assert (DEX_IS_FIBER (fiber));

  dex_object_lock (fiber);
  fiber->cancelled = TRUE;
  dex_object_unlock (fiber);
}

static gboolean
dex_fiber_propagate (DexFuture *future,
                     DexFuture *completed)
{
  DexFiber *fiber = DEX_FIBER (future);
  GSource *source = NULL;

  g_assert (DEX_IS_FIBER (fiber));
  g_assert (DEX_IS_FUTURE (completed));

  dex_object_lock (fiber);
  g_mutex_lock (&fiber->fiber_scheduler->mutex);

  g_assert (!fiber->runnable);
  g_assert (!fiber->exited);

  fiber->runnable = TRUE;

  g_queue_unlink (&fiber->fiber_scheduler->blocked, &fiber->link);
  g_queue_push_tail_link (&fiber->fiber_scheduler->runnable, &fiber->link);

  if (dex_thread_storage_get ()->fiber_scheduler != fiber->fiber_scheduler)
    source = g_source_ref ((GSource *)fiber->fiber_scheduler);

  g_mutex_unlock (&fiber->fiber_scheduler->mutex);
  dex_object_unlock (fiber);

  if (source != NULL)
    {
      g_main_context_wakeup (g_source_get_context (source));
      g_source_unref (source);
    }

  return TRUE;
}

static void
dex_fiber_finalize (DexObject *object)
{
  DexFiber *fiber = DEX_FIBER (object);

  g_assert (fiber->fiber_scheduler == NULL);
  g_assert (fiber->link.data == fiber);
  g_assert (fiber->link.prev == NULL);
  g_assert (fiber->link.next == NULL);
  g_assert (fiber->stack == NULL);

  dex_fiber_context_clear (&fiber->context);

  DEX_OBJECT_CLASS (dex_fiber_parent_class)->finalize (object);
}

static void
dex_fiber_class_init (DexFiberClass *fiber_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (fiber_class);
  DexFutureClass *future_class = DEX_FUTURE_CLASS (fiber_class);

  object_class->finalize = dex_fiber_finalize;

  future_class->discard = dex_fiber_discard;
  future_class->propagate = dex_fiber_propagate;

  if (cancelled_future == NULL)
    cancelled_future = dex_future_new_reject (DEX_ERROR,
                                              DEX_ERROR_FIBER_CANCELLED,
                                              "The fiber was cancelled");
}

static void
dex_fiber_init (DexFiber *fiber)
{
  fiber->link.data = fiber;
  fiber->hook.func = (GHookFunc)dex_fiber_start;
  fiber->hook.data = fiber;
}

static void
dex_fiber_start (DexFiber *fiber)
{
  DexFuture *future;

  future = fiber->func (fiber->func_data);

  g_assert (future != (DexFuture *)fiber);

  /* Await any future returned from the fiber */
  if (future != NULL)
    {
      dex_await_borrowed (future, NULL);
      dex_future_complete_from (DEX_FUTURE (fiber), future);
      dex_unref (future);
    }
  else
    {
      dex_future_complete (DEX_FUTURE (fiber),
                           NULL,
                           g_error_new_literal (DEX_ERROR,
                                                DEX_ERROR_FIBER_EXITED,
                                                "The fiber exited without a result"));
    }

  /* Mark the fiber as exited */
  fiber->exited = TRUE;

  /* Free func data if necessary
   *
   * TODO: We probably want to be able associate this data with a maincontext that
   * will free it up so things like GtkWidget are freed on main thread.
   */
  if (fiber->func_data_destroy)
    {
      GDestroyNotify func_data_destroy = fiber->func_data_destroy;
      gpointer func_data = fiber->func_data;

      fiber->func = NULL;
      fiber->func_data = NULL;
      fiber->func_data_destroy = NULL;

      func_data_destroy (func_data);
    }

  /* Now suspend, resuming the scheduler */
  dex_fiber_context_switch (&fiber->context, &fiber->fiber_scheduler->context);
}

DexFiber *
dex_fiber_new (DexFiberFunc   func,
               gpointer       func_data,
               GDestroyNotify func_data_destroy,
               gsize          stack_size)
{
  DexFiber *fiber;

  g_return_val_if_fail (func != NULL, NULL);

  fiber = (DexFiber *)dex_object_create_instance (DEX_TYPE_FIBER);
  fiber->func = func;
  fiber->func_data = func_data;
  fiber->func_data_destroy = func_data_destroy;
  fiber->stack_size = stack_size;

  return fiber;
}

static void
dex_fiber_ensure_stack (DexFiber          *fiber,
                        DexFiberScheduler *fiber_scheduler)
{
  g_assert (DEX_IS_FIBER (fiber));
  g_assert (fiber_scheduler != NULL);

  if (fiber->stack == NULL)
    {
      if (!fiber_scheduler->has_initialized)
        {
          fiber_scheduler->has_initialized = TRUE;
          dex_fiber_context_init_main (&fiber_scheduler->context);
        }

      if (fiber->stack_size == 0 ||
          fiber->stack_size == fiber_scheduler->stack_pool->stack_size)
        fiber->stack = dex_stack_pool_acquire (fiber_scheduler->stack_pool);
      else
        fiber->stack = dex_stack_new (fiber->stack_size);

      dex_fiber_context_init (&fiber->context, fiber->stack, &fiber->hook);
    }
}

static gboolean
dex_fiber_scheduler_check (GSource *source)
{
  DexFiberScheduler *fiber_scheduler = (DexFiberScheduler *)source;
  gboolean ret;

  g_mutex_lock (&fiber_scheduler->mutex);
  ret = fiber_scheduler->runnable.length != 0;
  g_mutex_unlock (&fiber_scheduler->mutex);

  return ret;
}

static gboolean
dex_fiber_scheduler_prepare (GSource *source,
                             int     *timeout)
{
  *timeout = -1;
  return dex_fiber_scheduler_check (source);
}

static gboolean
dex_fiber_scheduler_iteration (DexFiberScheduler *fiber_scheduler)
{
  DexFiber *fiber;

  g_assert (fiber_scheduler != NULL);

  g_mutex_lock (&fiber_scheduler->mutex);
  if ((fiber = g_queue_peek_head (&fiber_scheduler->runnable)))
    {
      dex_ref (fiber);

      g_assert (fiber->fiber_scheduler == fiber_scheduler);

      fiber->running = TRUE;
      fiber_scheduler->running = fiber;

      dex_fiber_ensure_stack (fiber, fiber_scheduler);
    }
  g_mutex_unlock (&fiber_scheduler->mutex);

  if (fiber == NULL)
    return FALSE;

  dex_fiber_context_switch (&fiber_scheduler->context, &fiber->context);

  g_mutex_lock (&fiber_scheduler->mutex);
  fiber->running = FALSE;
  fiber_scheduler->running = NULL;
  if (!fiber->released && fiber->exited)
    {
      g_queue_unlink (&fiber_scheduler->runnable, &fiber->link);

      if (fiber->stack->size == fiber_scheduler->stack_pool->stack_size)
        dex_stack_pool_release (fiber_scheduler->stack_pool,
                                g_steal_pointer (&fiber->stack));
      else
        g_clear_pointer (&fiber->stack, dex_stack_free);

      fiber->fiber_scheduler = NULL;
      fiber->released = TRUE;

      dex_unref (fiber);
    }
  g_mutex_unlock (&fiber_scheduler->mutex);

  dex_unref (fiber);

  return TRUE;
}

static gboolean
dex_fiber_scheduler_dispatch (GSource     *source,
                              GSourceFunc  callback,
                              gpointer     user_data)
{
  DexFiberScheduler *fiber_scheduler = (DexFiberScheduler *)source;
  guint max_iterations;

  g_assert (fiber_scheduler != NULL);

  /* Only process up to as many fibers that are currently in
   * the queue so that we don't exhaust the main loop endlessly
   * processing completed fibers w/o yielding to other GSource.
   */
  max_iterations = MAX (1, fiber_scheduler->runnable.length);

  dex_thread_storage_get ()->fiber_scheduler = fiber_scheduler;
  while (max_iterations && dex_fiber_scheduler_iteration (fiber_scheduler))
    max_iterations--;
  dex_thread_storage_get ()->fiber_scheduler = NULL;

  return G_SOURCE_CONTINUE;
}

static void
dex_fiber_scheduler_finalize (GSource *source)
{
  DexFiberScheduler *fiber_scheduler = (DexFiberScheduler *)source;

  g_clear_pointer (&fiber_scheduler->stack_pool, dex_stack_pool_free);
  g_mutex_clear (&fiber_scheduler->mutex);

  if (fiber_scheduler->has_initialized)
    {
      fiber_scheduler->has_initialized = FALSE;
      dex_fiber_context_clear_main (&fiber_scheduler->context);
    }
}

/**
 * dex_fiber_scheduler_new:
 *
 * Creates a #DexFiberScheduler.
 *
 * #DexFiberScheduler is a sub-scheduler to a #DexScheduler that can swap
 * into and schedule runnable #DexFiber.
 *
 * A #DexScheduler should have one of these #GSource attached to its
 * #GMainContext so that fibers can be executed there. When a thread
 * exits, its fibers may need to be migrated. Currently that is not
 * implemented as we do not yet destroy #DexThreadPoolWorker.
 */
DexFiberScheduler *
dex_fiber_scheduler_new (void)
{
  DexFiberScheduler *fiber_scheduler;
  static GSourceFuncs funcs = {
    .check = dex_fiber_scheduler_check,
    .prepare = dex_fiber_scheduler_prepare,
    .dispatch = dex_fiber_scheduler_dispatch,
    .finalize = dex_fiber_scheduler_finalize,
  };

  fiber_scheduler = (DexFiberScheduler *)g_source_new (&funcs, sizeof *fiber_scheduler);
  _g_source_set_static_name ((GSource *)fiber_scheduler, "[dex-fiber-scheduler]");
  g_mutex_init (&fiber_scheduler->mutex);
  fiber_scheduler->stack_pool = dex_stack_pool_new (0, 0, 0);

  return fiber_scheduler;
}

void
dex_fiber_scheduler_register (DexFiberScheduler *fiber_scheduler,
                              DexFiber          *fiber)
{
  g_assert (fiber_scheduler != NULL);
  g_assert (DEX_IS_FIBER (fiber));

  dex_ref (fiber);

  g_mutex_lock (&fiber_scheduler->mutex);

  g_assert (fiber->link.data == fiber);
  g_assert (fiber->fiber_scheduler == NULL);
  g_assert (fiber->exited == FALSE);
  g_assert (fiber->running == FALSE);
  g_assert (fiber->runnable == FALSE);
  g_assert (fiber->released == FALSE);

  fiber->fiber_scheduler = fiber_scheduler;
  fiber->runnable = TRUE;
  g_queue_push_tail_link (&fiber_scheduler->runnable, &fiber->link);

  g_mutex_unlock (&fiber_scheduler->mutex);

  if (dex_thread_storage_get ()->fiber_scheduler != fiber_scheduler)
    g_main_context_wakeup (g_source_get_context ((GSource *)fiber_scheduler));
}

static DexFiber *
dex_fiber_current (void)
{
  DexFiberScheduler *fiber_scheduler = dex_thread_storage_get ()->fiber_scheduler;
  return fiber_scheduler ? fiber_scheduler->running : NULL;
}

static inline void
dex_fiber_await (DexFiber  *fiber,
                 DexFuture *future)
{
  DexFiberScheduler *fiber_scheduler = fiber->fiber_scheduler;
  gboolean cancelled;

  g_assert (DEX_IS_FIBER (fiber));

  /* Move fiber from runnable to blocked queue */
  g_mutex_lock (&fiber_scheduler->mutex);
  fiber->runnable = FALSE;
  cancelled = fiber->cancelled;
  g_queue_unlink (&fiber_scheduler->runnable, &fiber->link);
  g_queue_push_tail_link (&fiber_scheduler->blocked, &fiber->link);
  g_mutex_unlock (&fiber_scheduler->mutex);

  /* Now request the future notify us of completion */
  if (cancelled)
    dex_future_chain (cancelled_future, DEX_FUTURE (fiber));
  else
    dex_future_chain (future, DEX_FUTURE (fiber));

  /* Swap to the scheduler to continue processing fibers */
  dex_fiber_context_switch (&fiber->context, &fiber_scheduler->context);
}

const GValue *
dex_await_borrowed (DexFuture  *future,
                    GError    **error)
{
  DexFiber *fiber;

  g_return_val_if_fail (DEX_IS_FUTURE (future), NULL);

  if (dex_future_get_status (future) != DEX_FUTURE_STATUS_PENDING)
    return dex_future_get_value (future, error);

  if G_UNLIKELY (!(fiber = dex_fiber_current ()))
    {
      g_set_error_literal (error,
                           DEX_ERROR,
                           DEX_ERROR_NO_FIBER,
                           "Not running on a fiber, cannot await");
      return NULL;
    }

  dex_fiber_await (fiber, future);

  return dex_future_get_value (future, error);
}

/**
 * dex_await: (method)
 * @future: (transfer full): a #DexFuture
 * @error: a location for a #GError, or %NULL
 *
 * Suspends the current #DexFiber and resumes when @future has completed.
 *
 * If @future is completed when this function is called, the fiber will handle
 * the result immediately.
 *
 * This function may only be called within a #DexFiber. To do otherwise will
 * return %FALSE and @error set to %DEX_ERROR_NO_FIBER.
 *
 * It is an error to call this function in a way that would cause
 * intermediate code to become invalid when resuming the stack. For example,
 * if a foreach-style function taking a callback was to suspend from the
 * callback, undefined behavior may occur such as thread-local-storage
 * having changed.
 *
 * Returns: %TRUE if the future resolved, otherwise %FALSE
 *   and @error is set.
 */
gboolean
dex_await (DexFuture  *future,
           GError    **error)
{
  const GValue *value = dex_await_borrowed (future, error);
  dex_unref (future);
  return value != NULL;
}
