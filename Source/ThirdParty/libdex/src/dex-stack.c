/* dex-stack.c
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

#ifdef G_OS_UNIX
# include <errno.h>
# include <sys/mman.h>
#endif

#include "dex-platform.h"
#include "dex-stack-private.h"

#define DEFAULT_STACK_SIZE (MAX (4096*16, dex_get_min_stack_size()))
#define DEFAULT_MIN_POOL_SIZE 4
#define DEFAULT_MAX_POOL_SIZE 16

DexStackPool *
dex_stack_pool_new (gsize stack_size,
                    int   min_pool_size,
                    int   max_pool_size)
{
  DexStackPool *stack_pool;

  if (stack_size == 0)
    stack_size = DEFAULT_STACK_SIZE;

  if (min_pool_size < 0)
    min_pool_size = DEFAULT_MIN_POOL_SIZE;

  if (max_pool_size < 0)
    max_pool_size = DEFAULT_MAX_POOL_SIZE;

  g_return_val_if_fail (min_pool_size <= max_pool_size, NULL);

  stack_pool = g_new0 (DexStackPool, 1);
  stack_pool->min_pool_size = min_pool_size;
  stack_pool->max_pool_size = max_pool_size;
  stack_pool->stack_size = stack_size;
  g_mutex_init (&stack_pool->mutex);

  for (guint i = 0; i < stack_pool->min_pool_size; i++)
    {
      DexStack *stack = dex_stack_new (stack_size);
      g_queue_push_head_link (&stack_pool->stacks, &stack->link);
    }

  return stack_pool;
}

void
dex_stack_pool_free (DexStackPool *stack_pool)
{
  g_return_if_fail (stack_pool != NULL);

  while (stack_pool->stacks.length > 0)
    {
      DexStack *stack = g_queue_pop_head_link (&stack_pool->stacks)->data;
      dex_stack_free (stack);
    }

  g_mutex_clear (&stack_pool->mutex);
  g_free (stack_pool);
}

DexStack *
dex_stack_new (gsize size)
{
  gsize page_size = dex_get_page_size ();
  DexStack *stack;
#ifdef G_OS_UNIX
  gpointer map;
  gpointer guard;
  int flags = 0;
#endif

  if (size < dex_get_min_stack_size ())
    size = DEFAULT_STACK_SIZE;

  /* Round up to next full page size */
  if ((size & (page_size-1)) != 0)
    size = (size + page_size) & ~(page_size-1);

  /* Make sure the stack size we're about to allocate is reasonable */
  g_assert_cmpuint (size, >=, page_size);
  g_assert_cmpuint (size, <, G_MAXUINT32);

#ifdef G_OS_UNIX
  flags = MAP_PRIVATE | MAP_ANONYMOUS;
#if defined(__OpenBSD__)
  flags |= MAP_STACK;
#endif

  /* mmap() the stack with an extra page for our guard page */
  map = mmap (NULL, size + page_size, PROT_READ|PROT_WRITE, flags, -1, 0);

  if (MAP_FAILED == map)
    {
      int errsv = errno;
      g_error ("Failed to allocate stack: %s", g_strerror (errsv));
    }

#ifdef __IA64__
  /* Itanium has a "register stack", see
   * itanium-software-runtime-architecture-guide.pdf for details.
   */
  guard = (gpointer)((gintptr)map + ((size / 2) & ~page_size));
#elif G_HAVE_GROWING_STACK
  guard = (gpointer)((gintptr)map + size);
#else
  guard = map;
#endif

  /* Setup guard page to fault */
#if HAVE_MPROTECT
  if (mprotect (guard, page_size, PROT_NONE) != 0)
    {
      int errsv = errno;
      g_error ("Failed to protect stack guard page: %s",
               g_strerror (errsv));
    }
#endif
#endif

  stack = g_new0 (DexStack, 1);
  stack->link.data = stack;
  stack->size = size;

#ifdef G_OS_UNIX
  stack->base = map;
  stack->guard = guard;

  if (map == guard)
    stack->ptr = (gpointer)((gintptr)map + page_size);
  else
    stack->ptr = map;
#endif

  return stack;
}

void
dex_stack_free (DexStack *stack)
{
#ifdef G_OS_UNIX
  guint page_size = dex_get_page_size ();

  g_assert (stack->link.data == stack);
  g_assert (stack->link.prev == NULL);
  g_assert (stack->link.next == NULL);

  if (stack->base != MAP_FAILED)
    munmap (stack->base, stack->size + page_size);
  stack->base = MAP_FAILED;
  stack->guard = MAP_FAILED;

  stack->size = 0;
  stack->link.data = NULL;

  g_free (stack);
#endif
}

void
dex_stack_mark_unused (DexStack *stack)
{
  g_assert (stack != NULL);
  g_assert (stack->link.data == stack);

#ifdef HAVE_MADVISE
  madvise (stack->ptr, stack->size, MADV_DONTNEED);
#endif
}
