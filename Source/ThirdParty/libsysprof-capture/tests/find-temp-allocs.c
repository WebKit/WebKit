/* find-temp-allocs.c
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
#include <sysprof-capture.h>

static struct {
  gint64 total;
  gint64 temp;
  gint64 leaked;
} allocinfo;

typedef struct
{
  gint                  pid;
  gint                  tid;
  gint64                time;
  SysprofCaptureAddress addr;
  gint64                size;
} Alloc;

static gint
compare_alloc (gconstpointer a,
               gconstpointer b)
{
  const Alloc *aptr = a;
  const Alloc *bptr = b;

  if (aptr->pid < bptr->pid)
    return -1;
  else if (aptr->pid > bptr->pid)
    return 1;

  if (aptr->tid < bptr->tid)
    return -1;
  else if (aptr->tid > bptr->tid)
    return 1;

  if (aptr->time < bptr->time)
    return -1;
  else if (aptr->time > bptr->time)
    return 1;

  if (aptr->addr < bptr->addr)
    return -1;
  else if (aptr->addr > bptr->addr)
    return 1;
  else
    return 0;
}

static void
find_temp_allocs (SysprofCaptureReader *reader)
{
  g_autoptr(GArray) ar = NULL;
  SysprofCaptureFrameType type;
  SysprofCaptureAddress last_addr = 0;

  g_assert (reader != NULL);

  ar = g_array_new (FALSE, FALSE, sizeof (Alloc));

  while (sysprof_capture_reader_peek_type (reader, &type))
    {
      if (type == SYSPROF_CAPTURE_FRAME_ALLOCATION)
        {
          const SysprofCaptureAllocation *ev;
          Alloc a;

          if (!(ev = sysprof_capture_reader_read_allocation (reader)))
            break;

          a.pid = ev->frame.pid;
          a.tid = ev->tid;
          a.time = ev->frame.time;
          a.addr = ev->alloc_addr;
          a.size = ev->alloc_size;

          g_array_append_val (ar, a);
        }
      else
        {
          if (!sysprof_capture_reader_skip (reader))
            break;
        }
    }

  /* Ensure items are in order because threads may have
   * reordered things and raced to write out malloc data.
   */
  g_array_sort (ar, compare_alloc);

  for (guint i = 0; i < ar->len; i++)
    {
      const Alloc *a = &g_array_index (ar, Alloc, i);

      if (a->size <= 0)
        {
          if (last_addr == a->addr)
            allocinfo.temp++;

          allocinfo.leaked--;
          last_addr = 0;
        }
      else
        {
          allocinfo.total++;
          allocinfo.leaked++;
          last_addr = a->addr;
        }
    }

  g_printerr ("Allocations: %"G_GINT64_FORMAT"\n", allocinfo.total);
  g_printerr ("  Temporary: %"G_GINT64_FORMAT" (%lf%%)\n",
              allocinfo.temp, allocinfo.temp / (gdouble)allocinfo.total * 100.0);
  g_printerr ("     Leaked: %"G_GINT64_FORMAT"\n", allocinfo.leaked);
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

  find_temp_allocs (reader);

  sysprof_capture_reader_unref (reader);

  return EXIT_SUCCESS;
}
