/* allocs-by-size.c
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
#include <glib/gi18n.h>
#include <locale.h>
#include <stddef.h>

#include <sysprof-capture.h>

typedef struct
{
  gsize size;
  gsize count;
  gsize cmp;
} Item;

static gint
item_compare (gconstpointer a,
              gconstpointer b)
{
  const Item *item_a = a;
  const Item *item_b = b;

  if (item_a->cmp < item_b->cmp)
    return -1;
  else if (item_a->cmp > item_b->cmp)
    return 1;
  else
    return 0;
}

static void
allocs_by_size (SysprofCaptureReader *reader)
{
  SysprofCaptureFrameType type;
  g_autoptr(GHashTable) allocs = NULL;
  g_autoptr(GArray) ar = NULL;
  GHashTableIter iter;
  gpointer k,v;
  gsize *count;

  allocs = g_hash_table_new_full (NULL, NULL, NULL, g_free);
  ar = g_array_new (FALSE, FALSE, sizeof (Item));

  while (sysprof_capture_reader_peek_type (reader, &type))
    {
      if (type == SYSPROF_CAPTURE_FRAME_ALLOCATION)
        {
          const SysprofCaptureAllocation *ev = sysprof_capture_reader_read_allocation (reader);

          if (ev == NULL)
            break;

          /* Ignore frees */
          if (ev->alloc_size <= 0)
            continue;

          if (!(count = g_hash_table_lookup (allocs, GSIZE_TO_POINTER (ev->alloc_size))))
            {
              count = g_new0 (gsize, 1);
              g_hash_table_insert (allocs, GSIZE_TO_POINTER (ev->alloc_size), count);
            }

          (*count)++;
        }
      else
        {
          if (!sysprof_capture_reader_skip (reader))
            break;
        }
    }

  g_hash_table_iter_init (&iter, allocs);
  while (g_hash_table_iter_next (&iter, &k, &v))
    {
      const Item item = {
        .size = GPOINTER_TO_SIZE (k),
        .count = *(gsize *)v,
        .cmp = *(gsize *)v * GPOINTER_TO_SIZE (k),
      };

      g_array_append_val (ar, item);
    }

  g_array_sort (ar, item_compare);

  g_print ("alloc_size,total_alloc,n_allocs\n");

  for (guint i = 0; i < ar->len; i++)
    {
      const Item *item = &g_array_index (ar, Item, i);

      g_print ("%"G_GSIZE_FORMAT",%"G_GSIZE_FORMAT",%"G_GSIZE_FORMAT"\n",
               item->size, item->cmp, item->count);
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

  /* Set up gettext translations */
  setlocale (LC_ALL, "");
  bindtextdomain (GETTEXT_PACKAGE, LOCALEDIR);
  bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
  textdomain (GETTEXT_PACKAGE);

  if (!(reader = sysprof_capture_reader_new (filename)))
    {
      int errsv = errno;
      g_printerr ("%s\n", g_strerror (errsv));
      return EXIT_FAILURE;
    }

  allocs_by_size (reader);

  sysprof_capture_reader_unref (reader);

  return EXIT_SUCCESS;
}
