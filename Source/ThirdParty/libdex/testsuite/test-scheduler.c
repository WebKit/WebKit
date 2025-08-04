/* test-scheduler.c
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

static DexScheduler *thread_pool;
static GMainLoop *main_loop;

static void
test_main_scheduler_simple_cb (gpointer data)
{
  gboolean *count = data;
  *count = 123;
  g_main_loop_quit (main_loop);
}

static void
test_main_scheduler_simple (void)
{
  DexScheduler *scheduler = dex_scheduler_get_default ();
  gboolean count = 0;

  g_assert_nonnull (scheduler);
  g_assert_true (DEX_IS_MAIN_SCHEDULER (scheduler));

  main_loop = g_main_loop_new (NULL, FALSE);
  dex_scheduler_push (scheduler,
                      test_main_scheduler_simple_cb,
                      &count);
  g_main_loop_run (main_loop);
  g_clear_pointer (&main_loop, g_main_loop_unref);

  g_assert_cmpint (count, ==, 123);
}

static DexFuture *
test_fiber2_func (gpointer user_data)
{
  guint *count = user_data;
  g_atomic_int_inc (count);
  return dex_future_new_for_boolean (TRUE);
}

static DexFuture *
test_fiber_func (gpointer user_data)
{
  GPtrArray *all = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < 10; i++)
    g_ptr_array_add (all, dex_scheduler_spawn (dex_scheduler_get_thread_default (),
                                               dex_get_min_stack_size (),
                                               test_fiber2_func, user_data, NULL));

  dex_await (dex_future_allv ((DexFuture **)all->pdata, all->len), NULL);

  g_ptr_array_unref (all);

  return NULL;
}

static DexFuture *
spawner (gpointer user_data)
{
  GPtrArray *all = g_ptr_array_new_with_free_func (dex_unref);

  for (guint i = 0; i < 1000; i++)
    g_ptr_array_add (all, dex_scheduler_spawn (thread_pool,
                                               dex_get_min_stack_size (),
                                               test_fiber_func, user_data, NULL));

  dex_await (dex_future_allv ((DexFuture **)(gpointer)all->pdata, all->len), NULL);

  g_ptr_array_unref (all);

  return NULL;
}

static DexFuture *
quit_cb (DexFuture *completed,
         gpointer   user_data)
{
  g_test_message ("Quiting main loop");
  g_main_loop_quit (main_loop);
  return NULL;
}

static void
test_thread_pool_scheduler_spawn (void)
{
  DexFuture *future;
  guint count = 0;

  thread_pool = dex_thread_pool_scheduler_new ();
  main_loop = g_main_loop_new (NULL, FALSE);

  g_test_message ("Spawning with stack size %u",
                  (guint)dex_get_min_stack_size ());

  future = dex_scheduler_spawn (NULL, 0, spawner, &count, NULL);
  future = dex_future_finally (future, quit_cb, NULL, NULL);

  g_test_message ("Running main loop");
  g_main_loop_run (main_loop);

  g_assert_cmpint (count, ==, 10*1000);

  dex_unref (future);
  dex_unref (thread_pool);
}

static void
test_thread_pool_scheduler_push_cb (gpointer data)
{
  struct {
    GMutex mutex;
    GCond cond;
  } *syncobj = data;

  g_mutex_lock (&syncobj->mutex);
  g_cond_signal (&syncobj->cond);
  g_mutex_unlock (&syncobj->mutex);
}

static void
test_thread_pool_scheduler_push (void)
{
  struct {
    GMutex mutex;
    GCond cond;
  } syncobj;

  g_mutex_init (&syncobj.mutex);
  g_cond_init (&syncobj.cond);

  g_mutex_lock (&syncobj.mutex);
  dex_scheduler_push (dex_thread_pool_scheduler_get_default (),
                      test_thread_pool_scheduler_push_cb,
                      &syncobj);
  g_cond_wait (&syncobj.cond, &syncobj.mutex);
  g_mutex_unlock (&syncobj.mutex);

  g_mutex_clear (&syncobj.mutex);
  g_cond_clear (&syncobj.cond);
}

int
main (int   argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Dex/TestSuite/MainScheduler/simple", test_main_scheduler_simple);
  g_test_add_func ("/Dex/TestSuite/ThreadPoolScheduler/10_000_fibers", test_thread_pool_scheduler_spawn);
  g_test_add_func ("/Dex/TestSuite/ThreadPoolScheduler/push", test_thread_pool_scheduler_push);
  return g_test_run ();
}
