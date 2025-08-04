/*
 * test-async-result.c
 *
 * Copyright 2022 Christian Hergert <chergert@redhat.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library; if not, see <http://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include <libdex.h>

typedef struct
{
  GMainLoop *main_loop;
  GObject *object;
  GError *error;
  int value;
} State;

static void
test_async_result_await_cb (GObject      *object,
                            GAsyncResult *result,
                            gpointer      user_data)
{
  State *state = user_data;

  g_assert_true (G_IS_MENU (object));
  g_assert_true (DEX_IS_ASYNC_RESULT (result));
  g_assert_nonnull (state);
  g_assert_true (object == state->object);

  state->value = dex_async_result_propagate_int (DEX_ASYNC_RESULT (result), &state->error);

  g_main_loop_quit (state->main_loop);
}

static void
test_async_result_await (void)
{
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE);
  DexPromise *future = dex_promise_new ();
  GMenu *obj = g_menu_new ();
  State state = {0};
  DexAsyncResult *async_result = dex_async_result_new (obj, NULL, test_async_result_await_cb, &state);

  state.main_loop = main_loop;
  state.object = G_OBJECT (obj);

  dex_async_result_await (async_result, dex_ref (future));
  g_clear_object (&async_result);

  dex_promise_resolve_int (future, 123);

  g_main_loop_run (main_loop);

  g_assert_cmpint (state.value, ==, 123);

  g_object_unref (obj);
  dex_unref (future);

  while (g_main_context_pending (NULL))
    g_main_context_iteration (NULL, FALSE);

  g_main_loop_unref (main_loop);
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Dex/TestSuite/AsyncResult/await", test_async_result_await);
  return g_test_run ();
}
