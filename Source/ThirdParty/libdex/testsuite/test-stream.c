/* test-stream.c
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

#include <config.h>

#include <libdex.h>

#include <gio/gio.h>

#include "dex-future-private.h"

#define ASSERT_STATUS(f,status) g_assert_cmpint(status, ==, dex_future_get_status(DEX_FUTURE(f)))

static DexFuture *
quit_cb (DexFuture *future,
         gpointer   user_data)
{
  g_main_loop_quit (user_data);
  return NULL;
}

static void
test_read_bytes (void)
{
  GFile *file = g_file_new_for_path ("/etc/os-release");
  GError *error = NULL;
  GInputStream *stream = G_INPUT_STREAM (g_file_read (file, NULL, &error));
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE);
  DexFuture *future;
  const GValue *value;
  GBytes *bytes;

  if (stream == NULL)
    {
      g_clear_error (&error);
      g_test_skip ("/etc/os-release not available");
      return;
    }

  future = dex_input_stream_read_bytes (stream, 4096, 0);
  future = dex_future_then (future, quit_cb, main_loop, NULL);

  g_main_loop_run (main_loop);

  value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS (value, G_TYPE_BYTES));

  bytes = g_value_get_boxed (value);
  g_assert_nonnull (bytes);

  g_main_loop_quit (main_loop);
  g_main_loop_unref (main_loop);
  dex_unref (future);
}

int
main (int   argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Dex/TestSuite/InputStream/read_bytes", test_read_bytes);
  return g_test_run ();
}
