/*
 * dex-work-stealing-queue-private.h
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

/* This code is heavily based upon wsq.hpp¹ from Tsung-Wei Huang, under
 * the MIT license. Its original license is provided below.
 *
 * ¹ https://github.com/taskflow/work-stealing-queue/tree/master/wsq.hpp
 *
 * MIT License
 *
 * Copyright (c) 2020 T.-W. Huang
 *
 * University of Utah, Salt Lake City, UT, USA
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#pragma once

#include <stdatomic.h>

#include <glib.h>

#include "dex-scheduler-private.h"

/**
 * SECTION:dex-work-stealing-queue
 * @title: DexWorkStealinQueue
 * @short_description: Lock-free unbounded single-producer multiple consumer queue
 *
 * This class implements the work stealing queue described in the paper,
 * "Correct and Efficient Work-Stealing for Weak Memory Models," available at
 * https://www.di.ens.fr/~zappa/readings/ppopp13.pdf
 *
 * Only the queue owner can perform pop and push operations, while others can
 * steal data from the queue.
 */

G_BEGIN_DECLS

#ifndef DEX_CACHELINE_SIZE
# define DEX_CACHELINE_SIZE 64
#endif

typedef struct _DexWorkStealingArray
{
  gint64               C;
  gint64               M;
  _Atomic(DexWorkItem) S[];
} DexWorkStealingArray;

typedef struct _DexWorkStealingQueue
{
  _Alignas(DEX_CACHELINE_SIZE) _Atomic(gint64)                top;
  _Alignas(DEX_CACHELINE_SIZE) _Atomic(gint64)                bottom;
  _Alignas(DEX_CACHELINE_SIZE) _Atomic(DexWorkStealingArray*) array;
                                       GPtrArray             *garbage;
                                       gatomicrefcount        ref_count;
} DexWorkStealingQueue;

DexWorkStealingQueue *dex_work_stealing_queue_new           (gint64                capacity);
DexWorkStealingQueue *dex_work_stealing_queue_ref           (DexWorkStealingQueue *work_stealing_queue);
void                  dex_work_stealing_queue_unref         (DexWorkStealingQueue *work_stealing_queue);
GSource              *dex_work_stealing_queue_create_source (DexWorkStealingQueue *work_stealing_queue);

static inline DexWorkStealingArray *
dex_work_stealing_array_new (gint64 c)
{
  DexWorkStealingArray *work_stealing_array;

  work_stealing_array = g_aligned_alloc0 (1,
                                          sizeof (DexWorkStealingArray) + (c * sizeof (DexWorkItem)),
                                          G_ALIGNOF (DexWorkStealingArray));
  work_stealing_array->C = c;
  work_stealing_array->M = c-1;

  return work_stealing_array;
}

static inline gint64
dex_work_stealing_array_capacity (DexWorkStealingArray *work_stealing_array)
{
  return work_stealing_array->C;
}

static inline void
dex_work_stealing_array_push (DexWorkStealingArray *work_stealing_array,
                              gint64                i,
                              DexWorkItem           work_item)
{
  atomic_store_explicit (&work_stealing_array->S[i & work_stealing_array->M],
                         work_item,
                         memory_order_relaxed);
}

static inline DexWorkItem
dex_work_stealing_array_pop (DexWorkStealingArray *work_stealing_array,
                             gint64                i)
{
  return atomic_load_explicit (&work_stealing_array->S[i & work_stealing_array->M],
                               memory_order_relaxed);
}

static inline DexWorkStealingArray *
dex_work_stealing_array_resize (DexWorkStealingArray *work_stealing_array,
                                gint64                b,
                                gint64                t)
{
  DexWorkStealingArray *ptr = dex_work_stealing_array_new (work_stealing_array->C * 2);
  for (gint64 i = t; i != b; i++)
    dex_work_stealing_array_push (ptr, i, dex_work_stealing_array_pop (work_stealing_array, i));
  return ptr;
}

static inline void
dex_work_stealing_array_free (DexWorkStealingArray *work_stealing_array)
{
  g_aligned_free (work_stealing_array);
}

static inline gboolean
dex_work_stealing_queue_empty (DexWorkStealingQueue *work_stealing_queue)
{
  gint64 b = atomic_load_explicit (&work_stealing_queue->bottom, memory_order_relaxed);
  gint64 t = atomic_load_explicit (&work_stealing_queue->top, memory_order_relaxed);
  return b <= t;
}

static inline gsize
dex_work_stealing_queue_size (DexWorkStealingQueue *work_stealing_queue)
{
  gint64 b = atomic_load_explicit (&work_stealing_queue->bottom, memory_order_relaxed);
  gint64 t = atomic_load_explicit (&work_stealing_queue->top, memory_order_relaxed);
  return b >= t ? b - t : 0;
}

/**
 * dex_work_stealing_queue_push:
 * @work_stealing_queue: a #DexWorkStealingQueue
 * @work_item: the work item to push
 *
 * This adds @work_item to the queue so that it can be processed by the
 * local worker, or optionally, stolen by another worker after there are
 * no more items to process in its queue or the global queue.
 *
 * This may _ONLY_ be called by the thread that owns @work_stealing_queue.
 *
 * Work items that originate outside of a worker thread should be pushed into
 * a global queue shared among workers.
 */
static inline void
dex_work_stealing_queue_push (DexWorkStealingQueue *work_stealing_queue,
                              DexWorkItem           work_item)
{
  gint64 b = atomic_load_explicit (&work_stealing_queue->bottom, memory_order_relaxed);
  gint64 t = atomic_load_explicit (&work_stealing_queue->top, memory_order_acquire);
  DexWorkStealingArray *a = atomic_load_explicit (&work_stealing_queue->array, memory_order_relaxed);

  /* queue is full */
  if (dex_work_stealing_array_capacity (a) - 1 < (b - t))
    {
      DexWorkStealingArray *tmp = dex_work_stealing_array_resize (a, b, t);
      g_ptr_array_add (work_stealing_queue->garbage, a);
      a = tmp;
      atomic_store_explicit (&work_stealing_queue->array, a, memory_order_relaxed);
    }

  dex_work_stealing_array_push (a, b, work_item);
  atomic_thread_fence (memory_order_release);
  atomic_store_explicit (&work_stealing_queue->bottom, b + 1, memory_order_relaxed);
}

/**
 * dex_work_stealing_queue_pop:
 * @work_stealing_queue: a #DexWorkStealingQueue
 * @out_work_item: (out): a location for a #DexWorkItem
 *
 * Pops the next work item from the queue without synchronization.
 *
 * This may _ONLY_ be called by the thread that owns @work_stealing_queue.
 *
 * Returns: %TRUE if @out_work_item was set, otherwise %FALSE and the queue
 *   is currently empty.
 */
static inline gboolean
dex_work_stealing_queue_pop (DexWorkStealingQueue *work_stealing_queue,
                             DexWorkItem          *out_work_item)
{
  DexWorkStealingArray *a;
  gboolean ret;
  gint64 b;
  gint64 t;

  b = atomic_load_explicit (&work_stealing_queue->bottom, memory_order_relaxed) - 1;
  a = atomic_load_explicit (&work_stealing_queue->array, memory_order_relaxed);
  atomic_store_explicit (&work_stealing_queue->bottom, b, memory_order_relaxed);
  atomic_thread_fence (memory_order_seq_cst);
  t = atomic_load_explicit (&work_stealing_queue->top, memory_order_relaxed);

  ret = t <= b;

  if (ret)
    {
      *out_work_item = dex_work_stealing_array_pop (a, b);

      if (t == b)
        {
          /* the last item just got stolen */
          if (!atomic_compare_exchange_strong_explicit (&work_stealing_queue->top,
                                                        &t,
                                                        t + 1,
                                                        memory_order_seq_cst,
                                                        memory_order_relaxed))
            ret = FALSE;

          atomic_store_explicit (&work_stealing_queue->bottom, b + 1, memory_order_relaxed);
        }
    }
  else
    {
      atomic_store_explicit (&work_stealing_queue->bottom, b + 1, memory_order_relaxed);
    }

  return ret;
}

/**
 * dex_work_stealing_queue_steal:
 * @work_stealing_queue: a #DexWorkStealingQueue
 * @out_work_item: (out): a location to store the work item
 *
 * Attempts to steal a #DexWorkItem from @work_stealing_queue with lock-free
 * synchronization among workers.
 *
 * This function is _ONLY_ be called by threads other than the thread owning
 * @work_stealing_queue.
 *
 * Returns: %TRUE if @out_work_item is set; otherwise %FALSE
 */
static inline gboolean
dex_work_stealing_queue_steal (DexWorkStealingQueue *work_stealing_queue,
                               DexWorkItem          *out_work_item)
{
  gint64 t;
  gint64 b;

  t = atomic_load_explicit (&work_stealing_queue->top, memory_order_acquire);
  atomic_thread_fence (memory_order_seq_cst);
  b = atomic_load_explicit (&work_stealing_queue->bottom, memory_order_acquire);

  if (t < b)
    {
      DexWorkStealingArray *a = atomic_load_explicit (&work_stealing_queue->array, memory_order_consume);

      *out_work_item = dex_work_stealing_array_pop (a, t);

      if (atomic_compare_exchange_strong_explicit (&work_stealing_queue->top,
                                                   &t,
                                                   t + 1,
                                                   memory_order_seq_cst,
                                                   memory_order_relaxed))
        return TRUE;
    }

  return FALSE;
}

static inline gint64
dex_work_stealing_queue_capacity (DexWorkStealingQueue *work_stealing_queue)
{
  return dex_work_stealing_array_capacity (
      atomic_load_explicit (&work_stealing_queue->array, memory_order_relaxed));
}

G_END_DECLS
