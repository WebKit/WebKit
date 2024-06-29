/* cross-thread-frees.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "config.h"

#include <errno.h>
#include <glib.h>
#include <stddef.h>

#include <sysprof-capture.h>

typedef struct
{
  gint tid;
  guint n_addrs;
  gint64 size;
  SysprofCaptureAddress addrs[0];
} Stack;

static void
stack_free (gpointer ptr)
{
  Stack *stack = ptr;
  gsize size = sizeof *stack + (stack->n_addrs * sizeof (SysprofCaptureAddress));
  g_slice_free1 (size, stack);
}

static Stack *
stack_new (gint                         tid,
           gint64                       size,
           guint                        n_addrs,
           const SysprofCaptureAddress *addrs)
{
  Stack *stack;

  stack = g_slice_alloc (sizeof *stack + (n_addrs * sizeof (SysprofCaptureAddress)));
  stack->tid = tid;
  stack->size = size;
  stack->n_addrs = n_addrs;
  for (guint i = 0; i < n_addrs; i++)
    stack->addrs[i] = addrs[i];

  return stack;
}

static void
cross_thread_frees (SysprofCaptureReader *reader)
{
  SysprofCaptureFrameType type;
  g_autoptr(GHashTable) stacks = NULL;

  stacks = g_hash_table_new_full (NULL, NULL, NULL, stack_free);

  while (sysprof_capture_reader_peek_type (reader, &type))
    {
      if (type == SYSPROF_CAPTURE_FRAME_ALLOCATION)
        {
          const SysprofCaptureAllocation *ev = sysprof_capture_reader_read_allocation (reader);
          gpointer key;

          if (ev == NULL)
            break;

          key = GINT_TO_POINTER (ev->alloc_addr);

          if (ev->alloc_size > 0)
            {
              g_hash_table_insert (stacks,
                                   key,
                                   stack_new (ev->tid, ev->alloc_size, ev->n_addrs, ev->addrs));
            }
          else
            {
              Stack *stack;

              stack = g_hash_table_lookup (stacks, key);
              if (stack == NULL)
                continue;

              if (ev->tid != stack->tid)
                {
                  g_print ("Alloc-Thread=%d Free-Thread=%d Size=%"G_GUINT64_FORMAT"\n",
                           stack->tid, ev->tid, stack->size);
                }

              g_hash_table_remove (stacks, key);
            }
        }
      else
        {
          if (!sysprof_capture_reader_skip (reader))
            break;
        }
    }
}

gint
main (gint   argc,
      gchar *argv[])
{
  SysprofCaptureReader *reader;
  const gchar *filename = argv[1];

  if (argc < 2)
    {
      g_printerr ("usage: %s FILENAME\n", argv[0]);
      return EXIT_FAILURE;
    }

  if (!(reader = sysprof_capture_reader_new (filename)))
    {
      int errsv = errno;
      g_printerr ("%s\n", g_strerror (errsv));
      return EXIT_FAILURE;
    }

  cross_thread_frees (reader);

  sysprof_capture_reader_unref (reader);

  return EXIT_SUCCESS;
}
