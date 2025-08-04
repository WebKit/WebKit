/* test-object.c
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

#include <stdatomic.h>

#include <libdex.h>

#include "dex-object-private.h"

static guint finalize_count;

#define TEST_TYPE_OBJECT (test_object_get_type())
#define TEST_IS_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, TEST_TYPE_OBJECT))
#define TEST_OBJECT(obj) (G_TYPE_CHECK_INSTANCE_CAST(obj, TEST_TYPE_OBJECT, TestObject))

GType test_object_get_type (void) G_GNUC_CONST;

typedef struct _TestObject
{
  DexObject parent_instance;
  int field1;
  guint field2;
  double field3;
  const char *field4;
} TestObject;

typedef struct _TestObjectClass
{
  DexObjectClass parent_class;
} TestObjectClass;

DEX_DEFINE_FINAL_TYPE (TestObject, test_object, DEX_TYPE_OBJECT)

static void
test_object_finalize (DexObject *object)
{
  g_atomic_int_inc (&finalize_count);
  DEX_OBJECT_CLASS (test_object_parent_class)->finalize (object);
}

static void
test_object_class_init (TestObjectClass *test_object_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (test_object_class);

  object_class->finalize = test_object_finalize;
}

static void
test_object_init (TestObject *test_object)
{
}

static TestObject *
test_object_new (void)
{
  return (TestObject *)g_type_create_instance (TEST_TYPE_OBJECT);
}

static void
test_weak_ref_st (void)
{
  TestObject *to, *to2;
  DexWeakRef wr;
  DexWeakRef wr2;
  DexWeakRef wr3;

  finalize_count = 0;

  dex_weak_ref_init (&wr, NULL);
  g_assert_null (dex_weak_ref_get (&wr));

  to = test_object_new ();
  dex_weak_ref_set (&wr, to);

  dex_ref (to);
  dex_unref (to);

  /* Add second weak-ref to test removing */
  dex_weak_ref_init (&wr2, to);
  dex_weak_ref_set (&wr2, to);

  /* And a third weak-ref to test set API */
  dex_weak_ref_init (&wr3, NULL);
  dex_weak_ref_set (&wr3, to);
  dex_weak_ref_set (&wr3, NULL);
  dex_weak_ref_set (&wr3, to);

  dex_weak_ref_clear (&wr2);
  dex_weak_ref_clear (&wr3);

  to2 = dex_weak_ref_get (&wr);
  g_assert_true (to == to2);
  dex_clear (&to2);
  dex_clear (&to);
  g_assert_cmpint (finalize_count, ==, 1);
  g_assert_null (dex_weak_ref_get (&wr));

  dex_weak_ref_clear (&wr);
}

typedef struct
{
  TestObject *to;
  DexWeakRef wr;
  GMutex wait1;
  GMutex wait2;
  GCond cond1;
  GCond cond2;
} TestDexWeakRefMt;

static gpointer
test_weak_ref_mt_worker (gpointer data)
{
  TestDexWeakRefMt *state = data;
  TestObject *to;

  g_assert_nonnull (state);
  g_assert_nonnull (state->to);

  /* Test that we can get a full ref from the weak, as we
   * should be certain to is still alive.
   */
  g_mutex_lock (&state->wait1);
  to = dex_weak_ref_get (&state->wr);
  g_assert_nonnull (to);
  g_clear_pointer (&to, dex_unref);
  g_cond_signal (&state->cond1);
  g_mutex_unlock (&state->wait1);

  /* Acquire the second lock (which is held by thread-1 until
   * we signaled above, at which point it's waiting for a
   * second condition to signal. The first thread should have
   * dropped the final ref, and therefore our attempt to get
   * a ref from the weak ref should fail.
   */
  g_mutex_lock (&state->wait2);
  to = dex_weak_ref_get (&state->wr);
  g_assert_null (to);
  g_cond_signal (&state->cond2);
  g_mutex_unlock (&state->wait2);

  return NULL;
}

static void
test_weak_ref_mt (void)
{
  TestDexWeakRefMt state;
  GThread *thread;

  finalize_count = 0;

  state.to = test_object_new ();
  dex_weak_ref_init (&state.wr, state.to);

  g_mutex_init (&state.wait1);
  g_mutex_init (&state.wait2);

  g_cond_init (&state.cond1);
  g_cond_init (&state.cond2);

  g_mutex_lock (&state.wait1);
  g_mutex_lock (&state.wait2);

  thread = g_thread_new ("test_weak_ref_mt_worker", test_weak_ref_mt_worker, &state);

  /* Now wait for thread-2 to signal us */
  g_cond_wait (&state.cond1, &state.wait1);

  /* We were signaled, now clear our ref, then allow thread-2
   * to acquire wait2 while we wait again to be signaled.
   */
  g_clear_pointer (&state.to, dex_unref);
  g_cond_wait (&state.cond2, &state.wait2);

  g_mutex_unlock (&state.wait1);
  g_mutex_unlock (&state.wait2);

  g_thread_join (thread);

  dex_weak_ref_clear (&state.wr);

  g_mutex_clear (&state.wait1);
  g_mutex_clear (&state.wait2);

  g_cond_clear (&state.cond1);
  g_cond_clear (&state.cond2);
}

typedef struct
{
  TestObject *to;
  DexWeakRef wr;
  GMutex mutex;
  GCond cond;
} TestDexWeakRefExtendLiveness;

static gpointer
test_weak_ref_extend_liveness_worker (gpointer data)
{
  TestDexWeakRefExtendLiveness *state = data;

  /* Prelock the weak-ref mutex so that we can make the
   * first thread block on acquiring it after it signals
   * our thread to continue.
   */
  g_mutex_lock (&state->wr.mutex);

  g_assert (state->to != NULL);

  /* Now wait for thread-1 to release the mutex (while waiting
   * for our signal) and we can signal it to continue.
   */
  g_mutex_lock (&state->mutex);
  g_cond_signal (&state->cond);
  g_mutex_unlock (&state->mutex);

  /* At this point, it is likely that we will get the first
   * thread to block on state->wr.mutex, but since we don't have
   * a condition to be signaled by, we just have to ... um, wait
   * and hope our CPU isn't running on hamster wheels.
   */
  while (DEX_OBJECT (state->to)->ref_count > 0)
    g_usleep (G_USEC_PER_SEC / 1000);

  g_assert_cmpint (finalize_count, ==, 0);

  /* We still have state->wr.mutex, so pretend we've extended
   * liveness while it's attempting to finalize.
   */
  g_atomic_int_inc (&DEX_OBJECT (state->to)->weak_refs_watermark);
  atomic_fetch_add_explicit (&DEX_OBJECT (state->to)->ref_count, 1, memory_order_relaxed);

  /* Now release our mutex so we can race against the
   * main thread to do the final unref.
   */
  g_mutex_unlock (&state->wr.mutex);

  dex_unref (state->to);

  return NULL;
}

static void
test_weak_ref_extend_liveness (void)
{
  TestDexWeakRefExtendLiveness state;
  GThread *thread;

  finalize_count = 0;

  state.to = test_object_new ();
  dex_weak_ref_init (&state.wr, state.to);
  g_mutex_init (&state.mutex);
  g_cond_init (&state.cond);

  g_mutex_lock (&state.mutex);

  thread = g_thread_new ("test_weak_ref_extend_liveness_worker",
                         test_weak_ref_extend_liveness_worker,
                         &state);

  g_cond_wait (&state.cond, &state.mutex);
  dex_unref (state.to);

  g_mutex_unlock (&state.mutex);

  g_thread_join (thread);

  state.to = NULL;
  dex_weak_ref_clear (&state.wr);
  g_mutex_clear (&state.mutex);
  g_cond_clear (&state.cond);

  g_assert_cmpint (finalize_count, ==, 1);
}

typedef struct
{
  TestObject *to;
  DexWeakRef wr;
  GMutex mutex;
  GCond cond;
} TestDexWeakRefImmortal;

static gpointer
test_weak_ref_immortal_worker (gpointer data)
{
  TestDexWeakRefImmortal *state = data;
  DexObject *borrowed;
  TestObject *to;

  borrowed = DEX_OBJECT (state->to);

  g_mutex_lock (&state->mutex);

  /* Acquire weak ref lock to force main thread to block */
  dex_object_lock (borrowed);

  /* Now signal other thread to continue */
  g_cond_signal (&state->cond);
  g_mutex_unlock (&state->mutex);

  /* Now wait until the ref count reaches zero, then bump the watermark
   * by creating a new reference.
   */
  while (g_atomic_int_get (&borrowed->ref_count) > 0)
    g_usleep (G_USEC_PER_SEC / 1000);

  to = dex_weak_ref_get (&state->wr);
  g_assert_nonnull (to);

  /* UB here really, since it's overflow */
  g_assert_cmpint (DEX_OBJECT (to)->weak_refs_watermark, ==, 0);

  /* Now release the lock allowing the main thread to continue */
  dex_object_unlock (borrowed);

  g_mutex_lock (&state->mutex);
  dex_unref (to);
  g_mutex_unlock (&state->mutex);

  dex_weak_ref_clear (&state->wr);

  return NULL;
}

static void
test_weak_ref_immortal (void)
{
  TestDexWeakRefImmortal state;
  DexObject *borrowed;
  GThread *thread;

  finalize_count = 0;

  g_mutex_init (&state.mutex);
  g_cond_init (&state.cond);

  state.to = test_object_new ();
  borrowed = DEX_OBJECT (state.to);
  dex_weak_ref_init (&state.wr, state.to);

  /* Make object immortal */
  borrowed->weak_refs_watermark = G_MAXUINT32;

  /* Now force a race on liveness from thread */
  g_mutex_lock (&state.mutex);
  thread = g_thread_new ("test_weak_ref_immortal", test_weak_ref_immortal_worker, &state);
  g_cond_wait (&state.cond, &state.mutex);

  /* Try to unref forcing the race (blocking while acquiring the weak ref
   * mutex until the thread allows us to continue).
   */
  dex_unref (state.to);
  g_assert_cmpint (finalize_count, ==, 0);

  /* Now release our lock allowing the thread to cleanup */
  g_mutex_unlock (&state.mutex);

  g_thread_join (thread);

  g_mutex_clear (&state.mutex);
  g_cond_clear (&state.cond);

  /* Clear watermark and unref so we are valgrind clean. Consumers
   * can't actually do this, but that is the point.
   */
  g_assert_cmpint (finalize_count, ==, 0);
  g_assert_cmpint (borrowed->ref_count, ==, 1);
  g_assert_cmpint (borrowed->weak_refs_watermark, ==, 1);
  borrowed->weak_refs_watermark = 1;
  borrowed->ref_count = 1;
  dex_unref (state.to);
  g_assert_cmpint (finalize_count, ==, 1);
}

static gboolean test_weak_ref_thread_guantlet_waiting;

static gpointer
test_weak_ref_thread_guantlet_worker (gpointer data)
{
  DexWeakRef *wr = data;
  TestObject *to;
  guint i = 0;

  while ((to = dex_weak_ref_get (wr)))
    {
      dex_unref (to);
      g_usleep (G_USEC_PER_SEC / 10000 * g_random_int_range (0, 100));

      /* It is possible, given a devilish enough of a thread scheduler, that
       * this could infinitely cause us to run due to always having a ref
       * greater than zero in one thread.
       *
       * So we read an atomic every now and again to break out.
       */
      i++;
      if ((i % 10) == 0 && g_atomic_int_get (&test_weak_ref_thread_guantlet_waiting))
        break;
    }

  dex_weak_ref_clear (wr);
  g_free (wr);

  return NULL;
}

static void
test_weak_ref_thread_guantlet (void)
{
  TestObject *to = test_object_new ();
  GThread *threads[8];

  finalize_count = 0;

  for (guint i = 0; i < G_N_ELEMENTS (threads); i++)
    {
      DexWeakRef *wr = g_new0 (DexWeakRef, 1);
      g_autofree char *thread_name = g_strdup_printf ("test_weak_ref_thread_guantlet:%d", i);

      dex_weak_ref_init (wr, to);
      threads[i] = g_thread_new (thread_name, test_weak_ref_thread_guantlet_worker, wr);
    }

  g_assert_cmpint (g_atomic_int_get (&finalize_count), ==, 0);

  g_usleep (G_USEC_PER_SEC / 3);
  g_assert_cmpint (g_atomic_int_get (&finalize_count), ==, 0);

  dex_unref (to);
  g_atomic_int_set (&test_weak_ref_thread_guantlet_waiting, TRUE);

  for (guint i = 0; i < G_N_ELEMENTS (threads); i++)
    g_thread_join (threads[i]);

  g_assert_cmpint (finalize_count, ==, 1);
}

static void
test_object_basic (void)
{
  g_assert_true (dex_object_get_type() != G_TYPE_INVALID);
  g_assert_true (G_TYPE_IS_INSTANTIATABLE (dex_object_get_type()));
  g_assert_true (G_TYPE_IS_ABSTRACT (dex_object_get_type()));
}

int
main (int   argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Dex/TestSuite/Object/basic", test_object_basic);
  g_test_add_func ("/Dex/TestSuite/WeakRef/single-threaded", test_weak_ref_st);
  g_test_add_func ("/Dex/TestSuite/WeakRef/multi-threaded", test_weak_ref_mt);
  g_test_add_func ("/Dex/TestSuite/WeakRef/extend-liveness", test_weak_ref_extend_liveness);
  g_test_add_func ("/Dex/TestSuite/WeakRef/immortal", test_weak_ref_immortal);
  g_test_add_func ("/Dex/TestSuite/WeakRef/thread-guantlet", test_weak_ref_thread_guantlet);
  return g_test_run ();
}
