/* test-semaphore.c
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

#include "dex-aio-backend-private.h"
#include "dex-main-scheduler-private.h"
#include "dex-semaphore-private.h"
#include "dex-thread-storage-private.h"

#define N_THREADS 32

static guint total_count;
static gboolean in_shutdown;
static guint done;

typedef struct
{
  DexSemaphore *semaphore;
  GThread *thread;
  GSource *source;
  int threadid;
  int handled;
} WorkerState;

static DexFuture *
worker_thread_callback (DexFuture *future,
                        gpointer   user_data)
{
  WorkerState *state = user_data;

  g_atomic_int_inc (&total_count);
  state->handled++;

  if (!g_atomic_int_get (&in_shutdown))
    return dex_semaphore_wait (state->semaphore);

  return NULL;
}

static gpointer
worker_thread_func (gpointer data)
{
  WorkerState *state = data;
  GMainContext *main_context = g_main_context_new ();
  DexMainScheduler *main_scheduler = dex_main_scheduler_new (main_context);
  DexFuture *future;

  future = dex_semaphore_wait (state->semaphore);
  future = dex_future_then_loop (future, worker_thread_callback, state, NULL);

  while (!g_atomic_int_get (&in_shutdown))
    g_main_context_iteration (main_context, TRUE);

  dex_unref (future);

  while (g_main_context_pending (main_context))
    g_main_context_iteration (main_context, FALSE);

  dex_unref (main_scheduler);
  g_main_context_unref (main_context);

  g_atomic_int_inc (&done);

  return NULL;
}

static void
test_semaphore_threaded (void)
{
  DexSemaphore *semaphore = dex_semaphore_new ();
  WorkerState state[N_THREADS] = {{0}};

  for (guint i = 0; i < G_N_ELEMENTS (state); i++)
    {
      char *name = g_strdup_printf ("test-semaphore-%u", i);

      state[i].semaphore = semaphore;
      state[i].threadid = i;
      state[i].thread = g_thread_new (name, worker_thread_func, &state[i]);

      g_free (name);
    }

  g_usleep (G_USEC_PER_SEC/2);

  for (guint i = 0; i < 3; i++)
    {
      int count = 10000;
      g_atomic_int_set (&total_count, 0);
      dex_semaphore_post_many (semaphore, count);
      g_usleep (G_USEC_PER_SEC);
      g_test_message ("Expected %u, got %u", count, g_atomic_int_get (&total_count));
      g_assert_cmpint (g_atomic_int_get (&total_count), ==, count);
    }

  g_atomic_int_set (&in_shutdown, TRUE);

  while (g_atomic_int_get (&done) < N_THREADS)
    dex_semaphore_post (semaphore);

  dex_semaphore_close (semaphore);

  for (guint i = 0; i < G_N_ELEMENTS (state); i++)
    {
      g_thread_join (state[i].thread);

      g_test_message ("Thread %d handled %d items", i, state[i].handled);
    }
}

int
main (int argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Dex/TestSuite/Semaphore/threaded", test_semaphore_threaded);
  return g_test_run ();
}
