/* dex-stack-private.h
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

#include <glib.h>

G_BEGIN_DECLS

typedef struct _DexStack     DexStack;
typedef struct _DexStackPool DexStackPool;

struct _DexStack
{
  GList    link;
  gsize    size;
#ifdef G_OS_UNIX
  gpointer base;
  gpointer guard;
  gpointer ptr;
#endif
};

struct _DexStackPool
{
  GMutex mutex;
  GQueue stacks;
  gsize  stack_size;
  guint  min_pool_size;
  guint  max_pool_size;
  guint  mark_unused : 1;
};

DexStackPool *dex_stack_pool_new    (gsize         stack_size,
                                     int           min_pool_size,
                                     int           max_pool_size);
void          dex_stack_pool_free   (DexStackPool *stack_pool);
DexStack     *dex_stack_new         (gsize         size);
void          dex_stack_free        (DexStack     *stack);
void          dex_stack_mark_unused (DexStack     *stack);

static inline DexStack *
dex_stack_pool_acquire (DexStackPool *stack_pool)
{
  DexStack *ret;

  g_assert (stack_pool != NULL);

  g_mutex_lock (&stack_pool->mutex);
  if (stack_pool->stacks.length > 0)
    {
      ret = g_queue_pop_head_link (&stack_pool->stacks)->data;
      g_mutex_unlock (&stack_pool->mutex);
    }
  else
    {
      g_mutex_unlock (&stack_pool->mutex);
      ret = dex_stack_new (stack_pool->stack_size);
    }

  return ret;
}

static inline void
dex_stack_pool_release (DexStackPool *stack_pool,
                        DexStack     *stack)
{
  g_assert (stack_pool != NULL);
  g_assert (stack->link.data == stack);
  g_assert (stack->link.prev == NULL);
  g_assert (stack->link.next == NULL);

  g_mutex_lock (&stack_pool->mutex);
  if (stack_pool->stacks.length > stack_pool->max_pool_size)
    {
      g_mutex_unlock (&stack_pool->mutex);
      dex_stack_free (stack);
    }
  else
    {
      if (stack_pool->mark_unused)
        dex_stack_mark_unused (stack);
      g_queue_push_head_link (&stack_pool->stacks, &stack->link);
      g_mutex_unlock (&stack_pool->mutex);
    }
}

G_END_DECLS
