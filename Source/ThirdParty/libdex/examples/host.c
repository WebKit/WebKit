/* host.c
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

#include <libdex.h>

static DexFuture *
resolve_address (gpointer user_data)
{
  GResolver *resolver = g_resolver_get_default ();
  const char *addr = user_data;
  g_autoptr(GError) error = NULL;
  g_autolist(GInetAddress) addrs = NULL;

  addrs = dex_await_boxed (dex_resolver_lookup_by_name (resolver, addr), &error);

  if (error != NULL)
    {
      g_printerr ("%s: %s\n", addr, error->message);
      return NULL;
    }

  for (const GList *iter = addrs; iter; iter = iter->next)
    {
      GInetAddress *inet_address = iter->data;
      g_autofree char *str = g_inet_address_to_string (inet_address);

      g_print ("%s has address %s\n", addr, str);
    }

  return NULL;
}

static DexFuture *
quit_cb (DexFuture *completed,
         gpointer   user_data)
{
  GMainLoop *main_loop = user_data;
  g_main_loop_quit (main_loop);
  return NULL;
}

int
main (int   argc,
      char *argv[])
{
  g_autoptr(GMainLoop) main_loop = g_main_loop_new (NULL, FALSE);
  g_autoptr(GPtrArray) futures = g_ptr_array_new ();
  g_autoptr(DexFuture) future = NULL;

  dex_init ();

  if (argc < 2)
    {
      g_printerr ("usage: %s HOSTNAME...\n", argv[0]);
      return 1;
    }

  for (guint i = 1; i < argc; i++)
    {
      const char *addr = argv[i];

      g_ptr_array_add (futures,
                       dex_scheduler_spawn (NULL, 0, resolve_address, g_strdup (addr), g_free));
    }

  future = dex_future_allv ((DexFuture **)futures->pdata, futures->len);
  future = dex_future_finally (future, quit_cb, main_loop, NULL);

  g_main_loop_run (main_loop);

  return 0;
}
