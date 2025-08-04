/* infinite-loop.c
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

#include <libdex.h>

static DexFuture *
infinite_loop_cb (DexFuture *future,
                  gpointer   user_data)
{
  const GValue *value;
  GMainLoop *main_loop = user_data;
  GError *error = NULL;

  if ((value = dex_future_get_value (future, &error)))
    {
      g_print ("\nCaught signal %u, exiting.\n",
               g_value_get_int (value));
      g_main_loop_quit (main_loop);
      return NULL;
    }

  g_print ("Looping ...\n");

  return dex_future_first (dex_timeout_new_seconds (1),
#ifdef G_OS_UNIX
                           dex_unix_signal_new (SIGINT),
#endif
                           NULL);
}

int
main (int   argc,
      char *argv[])
{
  GMainLoop *main_loop;
  DexFuture *loop;

  dex_init ();

  main_loop = g_main_loop_new (NULL, FALSE);
  loop = dex_future_finally_loop (dex_future_first (dex_timeout_new_seconds (1),
#ifdef G_OS_UNIX
                                                    dex_unix_signal_new (SIGINT),
#endif
                                                    NULL),
                                  infinite_loop_cb,
                                  g_main_loop_ref (main_loop),
                                  (GDestroyNotify)g_main_loop_unref);

  g_main_loop_run (main_loop);

  dex_unref (loop);
  g_main_loop_unref (main_loop);

  return 0;
}
