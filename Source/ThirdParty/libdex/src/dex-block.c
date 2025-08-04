/*
 * dex-block.c
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

#include "dex-block-private.h"
#include "dex-thread-storage-private.h"

/**
 * DexBlock:
 *
 * #DexBlock represents a callback closure that can be scheduled to run
 * within a specific #GMainContext.
 *
 * You create these by chaining futures together using dex_future_then(),
 * dex_future_catch(), dex_future_finally() and similar.
 */

typedef struct _DexBlock
{
  DexFuture          parent_instance;
  DexScheduler      *scheduler;
  DexFuture         *awaiting;
  DexFutureCallback  callback;
  gpointer           callback_data;
  GDestroyNotify     callback_data_destroy;
  DexBlockKind       kind : 3;
  guint              handled : 1;
} DexBlock;

typedef struct _DexBlockClass
{
  DexFutureClass parent_class;
} DexBlockClass;

DEX_DEFINE_FINAL_TYPE (DexBlock, dex_block, DEX_TYPE_FUTURE)

G_DEFINE_ENUM_TYPE (DexBlockKind, dex_block_kind,
                    G_DEFINE_ENUM_VALUE (DEX_BLOCK_KIND_THEN, "then"),
                    G_DEFINE_ENUM_VALUE (DEX_BLOCK_KIND_CATCH, "catch"),
                    G_DEFINE_ENUM_VALUE (DEX_BLOCK_KIND_FINALLY, "finally"))

#undef DEX_TYPE_BLOCK
#define DEX_TYPE_BLOCK dex_block_type

static gboolean
dex_block_handles (DexBlock  *block,
                   DexFuture *future)
{
  g_assert (DEX_IS_BLOCK (block));
  g_assert (DEX_IS_FUTURE (future));

  switch (dex_future_get_status (future))
    {
    case DEX_FUTURE_STATUS_RESOLVED:
      return (block->kind & DEX_BLOCK_KIND_THEN) != 0;

    case DEX_FUTURE_STATUS_REJECTED:
      return (block->kind & DEX_BLOCK_KIND_CATCH) != 0;

    case DEX_FUTURE_STATUS_PENDING:
    default:
      return FALSE;
    }
}

typedef struct _PropagateState
{
  DexBlock *block;
  DexFuture *completed;
} PropagateState;

static gboolean
dex_block_propagate_within_scheduler_internal (PropagateState *state)
{
  DexFuture *delayed = state->block->callback (state->completed, state->block->callback_data);

  /* If we got a future then we need to chain to it so that we get
   * a second propagation callback with the resolved or rejected
   * value for resolution.
   */
  if (delayed != NULL)
    {
      dex_object_lock (state->block);
      state->block->awaiting = dex_ref (delayed);
      dex_object_unlock (state->block);

      dex_future_chain (delayed, DEX_FUTURE (state->block));
      dex_unref (delayed);

      return TRUE;
    }
  else
    {
      GDestroyNotify notify = NULL;
      gpointer notify_data = NULL;

      /* We can't asynchronously wait for more futures now so we should
       * aggressively release the callback data so that any reference cycles
       * are broken immediately.
       */
      dex_object_lock (state->block);
      notify = g_steal_pointer (&state->block->callback_data_destroy);
      notify_data = g_steal_pointer (&state->block->callback_data);
      state->block->callback = NULL;
      dex_object_unlock (state->block);

      if (notify != NULL)
        notify (notify_data);

      return FALSE;
    }
}

static void
dex_block_propagate_within_scheduler (gpointer data)
{
  PropagateState *state = data;

  g_assert (state != NULL);
  g_assert (DEX_IS_BLOCK (state->block));
  g_assert (DEX_IS_FUTURE (state->completed));

  if (!dex_block_propagate_within_scheduler_internal (state))
    dex_future_complete (DEX_FUTURE (state->block),
                         state->completed->rejected ? NULL : &state->completed->resolved,
                         state->completed->rejected ? g_error_copy (state->completed->rejected) : NULL);

  dex_clear (&state->block);
  dex_clear (&state->completed);
  g_free (state);
}

static gboolean
dex_block_propagate (DexFuture *future,
                     DexFuture *completed)
{
  DexBlock *block = DEX_BLOCK (future);
  DexFuture *awaiting;
  gboolean do_callback = FALSE;

  g_assert (DEX_IS_BLOCK (block));
  g_assert (DEX_IS_FUTURE (completed));
  g_assert (dex_future_get_status (completed) != DEX_FUTURE_STATUS_PENDING);

  /* Mark result as handled as we don't want to execute the callback
   * again when a possible secondary DexFuture propagates completion
   * to us. That would only happen if @callback returns a DexFuture
   * which completes.
   */
  dex_object_lock (future);
  if ((block->kind & DEX_BLOCK_KIND_LOOP) != 0)
    do_callback = TRUE;
  else if (!block->handled)
    do_callback = block->handled = TRUE;
  awaiting = g_steal_pointer (&block->awaiting);
  dex_object_unlock (future);

  /* Release the future we were waiting on */
  dex_clear (&awaiting);

  /* Run the callback, possibly getting a future back to delay further
   * processing until it's completed.
   */
  if (do_callback && dex_block_handles (block, completed))
    {
      PropagateState state = {block, completed};
      DexThreadStorage *storage = dex_thread_storage_get ();

      /* If we are on the same scheduler that created this block, then
       * we can execute it now.
       */
      if (block->scheduler == dex_scheduler_get_thread_default () &&
          storage->sync_dispatch_depth < DEX_DISPATCH_RECURSE_MAX)
        {
          gboolean ret;

          storage->sync_dispatch_depth++;
          ret = dex_block_propagate_within_scheduler_internal (&state);
          storage->sync_dispatch_depth--;

          return ret;
        }

      /* Otherwise we must defer it to the scheduler */
      dex_ref (block);
      dex_ref (completed);
      dex_scheduler_push (block->scheduler,
                          dex_block_propagate_within_scheduler,
                          g_memdup2 (&state, sizeof state));

      return TRUE;
    }

  return FALSE;
}

static void
dex_block_finalize (DexObject *object)
{
  DexBlock *block = DEX_BLOCK (object);

  if (block->callback_data_destroy)
    {
      block->callback_data_destroy (block->callback_data);
      block->callback_data_destroy = NULL;
      block->callback_data = NULL;
      block->callback = NULL;
    }

  dex_clear (&block->awaiting);
  dex_clear (&block->scheduler);

  DEX_OBJECT_CLASS (dex_block_parent_class)->finalize (object);
}

static void
dex_block_class_init (DexBlockClass *block_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (block_class);
  DexFutureClass *future_class = DEX_FUTURE_CLASS (block_class);

  object_class->finalize = dex_block_finalize;

  future_class->propagate = dex_block_propagate;
}

static void
dex_block_init (DexBlock *block)
{
}

/**
 * dex_block_new:
 * @future: (transfer full): a #DexFuture to process
 * @scheduler: (nullable): a #DexScheduler or %NULL
 * @kind: the kind of block
 * @callback: (scope notified): the callback for the block
 * @callback_data: the data for the callback
 * @callback_data_destroy: closure destroy for @callback_data
 *
 * Creates a new block that will process the result of @future.
 *
 * The result of @callback will be assigned to the future returned
 * from this method.
 *
 * Returns: (transfer full): a #DexBlock
 */
DexFuture *
dex_block_new (DexFuture         *future,
               DexScheduler      *scheduler,
               DexBlockKind       kind,
               DexFutureCallback  callback,
               gpointer           callback_data,
               GDestroyNotify     callback_data_destroy)
{
  DexBlock *block;

  g_return_val_if_fail (DEX_IS_FUTURE (future), NULL);

  block = (DexBlock *)dex_object_create_instance (dex_block_type);
  block->scheduler = scheduler ? dex_ref (scheduler) : dex_scheduler_ref_thread_default ();
  block->awaiting = future;
  block->kind = kind;
  block->callback = callback;
  block->callback_data = callback_data;
  block->callback_data_destroy = callback_data_destroy;

  dex_future_chain (future, DEX_FUTURE (block));

  return DEX_FUTURE (block);
}

/**
 * dex_block_get_kind:
 * @block: a #DexBlock
 *
 * Gets the kind of block.
 *
 * The kind of block relates to what situations the block would be
 * executed such as for handling a future resolution, rejection, or
 * both.
 *
 * Returns: a #DexBlockKind
 */
DexBlockKind
dex_block_get_kind (DexBlock *block)
{
  g_return_val_if_fail (DEX_IS_BLOCK (block), 0);

  return block->kind;
}

/**
 * dex_block_get_scheduler:
 * @block: a #DexBlock
 *
 * Gets the scheduler to use when executing a block.
 *
 * Returns: (transfer none): a #DexScheduler
 */
DexScheduler *
dex_block_get_scheduler (DexBlock *block)
{
  g_return_val_if_fail (DEX_IS_BLOCK (block), NULL);

  return block->scheduler;
}
