/* test-future.c
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

#include <gio/gio.h>

#include "dex-future-private.h"
#include "dex-async-pair-private.h"

#define ASSERT_STATUS(f,status) g_assert_cmpint(status, ==, dex_future_get_status(DEX_FUTURE(f)))
#define ASSERT_INSTANCE_TYPE(obj,type) \
  G_STMT_START { \
    if (!G_TYPE_CHECK_INSTANCE_TYPE((obj), (type))) \
      g_error ("%s: %s is not a %s", \
               G_STRLOC, \
               (obj) ? G_OBJECT_TYPE_NAME((obj)) \
                     : "NULL", g_type_name(type)); \
  } G_STMT_END

typedef struct
{
  guint catch;
  guint destroy;
  guint finally;
  guint then;
} TestInfo;

static DexFuture *
catch_cb (DexFuture *future,
          gpointer   user_data)
{
  TestInfo *info = user_data;
  g_atomic_int_inc (&info->catch);
  return DEX_FUTURE (dex_future_new_for_string ("123"));
}

static DexFuture *
then_cb (DexFuture *future,
         gpointer   user_data)
{
  TestInfo *info = user_data;
  const GValue *value;
  GError *error = NULL;

  g_atomic_int_inc (&info->then);

  value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_STRING (value));
  g_assert_cmpstr (g_value_get_string (value), ==, "123");

  return DEX_FUTURE (dex_future_new_for_int (123));
}

static DexFuture *
finally_cb (DexFuture *future,
            gpointer   user_data)
{
  TestInfo *info = user_data;
  const GValue *value;
  GError *error = NULL;

  g_atomic_int_inc (&info->finally);

  value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 123);

  return NULL;
}

static void
destroy_cb (gpointer data)
{
  TestInfo *info = data;
  g_atomic_int_inc (&info->destroy);
}

static void
test_future_then (void)
{
  DexCancellable *cancellable;
  const GValue *resolved;
  DexFuture *future;
  GError *error = NULL;
  TestInfo info = {0};

  cancellable = dex_cancellable_new ();
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (cancellable)), ==, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancellable);
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (cancellable)), ==, DEX_FUTURE_STATUS_REJECTED);

  future = dex_future_catch (dex_ref (cancellable), catch_cb, &info, destroy_cb);
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_RESOLVED);

  future = dex_future_then (future, then_cb, &info, destroy_cb);
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_RESOLVED);

  future = dex_future_finally (future, finally_cb, &info, destroy_cb);
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_RESOLVED);

  resolved = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (resolved);
  g_assert_true (G_VALUE_HOLDS_INT (resolved));
  g_assert_cmpint (g_value_get_int (resolved), ==, 123);

  dex_unref (future);
  dex_unref (cancellable);

  g_assert_cmpint (info.catch, ==, 1);
  g_assert_cmpint (info.finally, ==, 1);
  g_assert_cmpint (info.then, ==, 1);
  g_assert_cmpint (info.destroy, ==, 3);
}

static void
test_cancellable_cancel (void)
{
  DexCancellable *future = dex_cancellable_new ();
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (future)), ==, DEX_FUTURE_STATUS_PENDING);
  dex_cancellable_cancel (future);
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (future)), ==, DEX_FUTURE_STATUS_REJECTED);
  dex_unref (future);
}

static DexFuture *
on_timed_out (DexFuture *future,
              gpointer   user_data)
{
  g_main_loop_quit (user_data);
  return NULL;
}

static void
test_timeout (void)
{
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE);
  DexFuture *timeout = dex_timeout_new_deadline (g_get_monotonic_time ());
  DexFuture *future = dex_future_catch (dex_ref (timeout), on_timed_out, main_loop, NULL);

  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (timeout)), ==, DEX_FUTURE_STATUS_PENDING);
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_PENDING);

  g_main_loop_run (main_loop);

  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (timeout)), ==, DEX_FUTURE_STATUS_REJECTED);
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_REJECTED);

  dex_clear (&future);
  dex_clear (&timeout);

  g_main_loop_unref (main_loop);
}

static void
test_promise_type (void)
{
  g_assert_true (dex_promise_get_type() != G_TYPE_INVALID);
  g_assert_true (G_TYPE_IS_INSTANTIATABLE (dex_promise_get_type()));
  g_assert_false (G_TYPE_IS_ABSTRACT (dex_promise_get_type()));
  g_assert_true (G_TYPE_IS_FINAL (dex_promise_get_type()));
}

static void
test_promise_autoptr (void)
{
  G_GNUC_UNUSED g_autoptr(DexPromise) promise = NULL;
}

static void
test_promise_new (void)
{
  DexPromise *promise;

  promise = dex_promise_new ();
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (promise)), ==, DEX_FUTURE_STATUS_PENDING);
  g_assert_nonnull (promise);
  g_assert_true (DEX_IS_FUTURE (promise));
  dex_clear (&promise);
}

static void
test_static_future_new (void)
{
  DexFuture *future;

  future = dex_future_new_for_string ("123");
  g_assert_true (DEX_IS_STATIC_FUTURE (future));
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (future)), ==, DEX_FUTURE_STATUS_RESOLVED);
  dex_clear (&future);

  future = dex_future_new_for_int (123);
  g_assert_true (DEX_IS_STATIC_FUTURE (future));
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (future)), ==, DEX_FUTURE_STATUS_RESOLVED);
  dex_clear (&future);

  future = dex_future_new_for_boolean (TRUE);
  g_assert_true (DEX_IS_STATIC_FUTURE (future));
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (future)), ==, DEX_FUTURE_STATUS_RESOLVED);
  dex_clear (&future);

  future = dex_future_new_for_error (g_error_new_literal (G_IO_ERROR, G_IO_ERROR_PENDING, "pending"));
  g_assert_true (DEX_IS_STATIC_FUTURE (future));
  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (future)), ==, DEX_FUTURE_STATUS_REJECTED);
  dex_clear (&future);
}

static void
test_promise_resolve (void)
{
  DexPromise *promise = dex_promise_new ();
  GValue value = G_VALUE_INIT;
  const GValue *resolved;
  GError *error = NULL;

  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (promise)), ==, DEX_FUTURE_STATUS_PENDING);

  g_value_init (&value, G_TYPE_BOOLEAN);
  g_value_set_boolean (&value, TRUE);
  dex_promise_resolve (promise, &value);

  g_assert_cmpint (dex_future_get_status (DEX_FUTURE (promise)), ==, DEX_FUTURE_STATUS_RESOLVED);

  resolved = dex_future_get_value (DEX_FUTURE (promise), &error);
  g_assert_no_error (error);
  g_assert_cmpint (G_VALUE_TYPE (resolved), ==, G_VALUE_TYPE (&value));
  g_assert_cmpint (g_value_get_boolean (resolved), ==, g_value_get_boolean (&value));

  g_value_unset (&value);
  dex_unref (promise);
}

#define ASYNC_TEST(T, TYPE, NAME, propagate, gvalue, res, cmp) \
typedef struct G_PASTE (_Test, T) G_PASTE (Test, T); \
static void \
G_PASTE (test_async_, T) (G_PASTE (Test, T)   *instance, \
                          GCancellable        *cancellable, \
                          GAsyncReadyCallback  callback, \
                          gpointer             user_data) \
{ \
  GTask *task = g_task_new (instance, cancellable, callback, user_data); \
  g_assert_true (G_IS_OBJECT (instance)); \
  g_assert_true (!cancellable || G_IS_CANCELLABLE (cancellable)); \
  g_assert_true (callback != NULL); \
  G_PASTE (g_task_return_, propagate)(task, res); \
  g_object_unref (task); \
} \
static T \
G_PASTE (test_finish_, T)(G_PASTE (Test, T) *instance, GAsyncResult *result, GError **error) \
{ \
  g_assert_true (G_IS_TASK (result)); \
  g_assert_true (G_IS_OBJECT (instance)); \
  return G_PASTE (g_task_propagate_, propagate) (G_TASK (result), error); \
} \
static DexFuture * \
G_PASTE (test_complete_, T) (DexFuture *future, gpointer user_data) \
{ \
  GMainLoop *main_loop = user_data; \
  GError *error = NULL; \
  const GValue *value = dex_future_get_value (future, &error); \
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_RESOLVED); \
  g_assert_no_error (error); \
  g_assert_true (G_PASTE (G_VALUE_HOLDS_, NAME)(value)); \
  G_PASTE (g_assert_, cmp) (G_PASTE (g_value_get_, gvalue) (value), ==, res); \
  g_main_loop_quit (main_loop); \
  return NULL; \
} \
static void \
G_PASTE (test_async_pair_, T) (void) \
{ \
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE); \
  GObject *object = G_OBJECT (g_menu_new ()); \
  DexFuture *future = dex_async_pair_new (object, \
                                          &DEX_ASYNC_PAIR_INFO (G_PASTE (test_async_, T), G_PASTE (test_finish_, T), TYPE)); \
  future = dex_future_finally (future, G_PASTE (test_complete_, T), main_loop, NULL); \
  g_main_loop_run (main_loop); \
  dex_unref (future); \
  g_clear_object (&object); \
  g_main_loop_unref (main_loop); \
}

ASYNC_TEST (gboolean, G_TYPE_BOOLEAN, BOOLEAN, boolean, boolean, TRUE, cmpint)
ASYNC_TEST (int, G_TYPE_INT, INT, int, int, 123, cmpint)
ASYNC_TEST (guint, G_TYPE_UINT, UINT, int, uint, 321, cmpint)
ASYNC_TEST (gint64, G_TYPE_INT64, INT64, int, int64, -123123123L, cmpint)
ASYNC_TEST (guint64, G_TYPE_UINT64, UINT64, int, uint64, 123123123L, cmpint)
ASYNC_TEST (glong, G_TYPE_LONG, LONG, int, long, -123123, cmpint)
ASYNC_TEST (gulong, G_TYPE_ULONG, ULONG, int, ulong, 123123, cmpint)
ASYNC_TEST (DexFutureStatus, DEX_TYPE_FUTURE_STATUS, ENUM, int, enum, DEX_FUTURE_STATUS_REJECTED, cmpint)
ASYNC_TEST (GSubprocessFlags, G_TYPE_SUBPROCESS_FLAGS, FLAGS, int, flags, (G_SUBPROCESS_FLAGS_STDIN_PIPE|G_SUBPROCESS_FLAGS_STDOUT_PIPE), cmpint)

#define ASYNC_TEST_PTR(T, NAME, propagate, gvalue, res, cmp, copyfunc, freefunc) \
typedef struct G_PASTE (_Test, T) G_PASTE (Test, T); \
static void \
G_PASTE (test_async_, T) (G_PASTE (Test, T)   *instance, \
                          GCancellable        *cancellable, \
                          GAsyncReadyCallback  callback, \
                          gpointer             user_data) \
{ \
  GTask *task = g_task_new (instance, cancellable, callback, user_data); \
  g_assert_true (G_IS_OBJECT (instance)); \
  g_assert_true (!cancellable || G_IS_CANCELLABLE (cancellable)); \
  g_assert_true (callback != NULL); \
  G_PASTE (g_task_return_, propagate)(task, copyfunc, (GDestroyNotify)freefunc); \
  g_object_unref (task); \
} \
static T * \
G_PASTE (test_finish_, T)(G_PASTE (Test, T) *instance, GAsyncResult *result, GError **error) \
{ \
  g_assert_true (G_IS_TASK (result)); \
  g_assert_true (G_IS_OBJECT (instance)); \
  return G_PASTE (g_task_propagate_, propagate) (G_TASK (result), error); \
} \
static DexFuture * \
G_PASTE (test_complete_, T) (DexFuture *future, gpointer user_data) \
{ \
  gpointer *state = user_data; \
  GMainLoop *main_loop = state[0]; \
  G_GNUC_UNUSED gpointer instance = state[1]; \
  GError *error = NULL; \
  const GValue *value = dex_future_get_value (future, &error); \
  const T *ret = G_PASTE (g_value_get_, gvalue) (value); \
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_RESOLVED); \
  g_assert_no_error (error); \
  g_assert_true (G_PASTE (G_VALUE_HOLDS_, NAME)(value)); \
  g_assert_true (cmp(ret,res)); \
  g_main_loop_quit (main_loop); \
  return NULL; \
} \
static void \
G_PASTE (test_async_pair_, T) (void) \
{ \
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE); \
  GObject *instance = G_OBJECT (g_menu_new ()); \
  gpointer state[2] = { main_loop, instance }; \
  DexFuture *future = dex_async_pair_new (instance, \
                                          &G_PASTE (DEX_ASYNC_PAIR_INFO_, NAME) (G_PASTE (test_async_, T), \
                                                                                 G_PASTE (test_finish_, T))); \
  future = dex_future_finally (future, G_PASTE (test_complete_, T), state, NULL); \
  g_main_loop_run (main_loop); \
  dex_unref (future); \
  g_clear_object (&instance); \
  g_main_loop_unref (main_loop); \
}

ASYNC_TEST_PTR (char, STRING, pointer, string, "string-test", g_str_equal, g_strdup("string-test"), g_free)

static void ptr_free (gpointer p) { }
static gboolean cmpptr (gconstpointer a, gconstpointer b) { return a == b; }
ASYNC_TEST_PTR (gpointer, POINTER, pointer, pointer, "a-pointer", cmpptr, (gpointer)"a-pointer", ptr_free)

ASYNC_TEST_PTR (GObject, OBJECT, pointer, object, instance, cmpptr, g_object_ref(instance), g_object_unref)

#define ASYNC_TEST_ERROR(T, NAME, propagate) \
typedef struct G_PASTE (_Test, T) G_PASTE (Test, T); \
static void \
G_PASTE (test_async_, T) (G_PASTE (Test, T)   *instance, \
                          GCancellable        *cancellable, \
                          GAsyncReadyCallback  callback, \
                          gpointer             user_data) \
{ \
  GTask *task = g_task_new (instance, cancellable, callback, user_data); \
  g_assert_true (G_IS_OBJECT (instance)); \
  g_assert_true (!cancellable || G_IS_CANCELLABLE (cancellable)); \
  g_assert_true (callback != NULL); \
  g_task_return_new_error (task, DEX_ERROR, DEX_ERROR_DEPENDENCY_FAILED, "Failed"); \
  g_object_unref (task); \
} \
static T * \
G_PASTE (test_finish_, T)(G_PASTE (Test, T) *instance, GAsyncResult *result, GError **error) \
{ \
  g_assert_true (G_IS_TASK (result)); \
  g_assert_true (G_IS_OBJECT (instance)); \
  return G_PASTE (g_task_propagate_, propagate) (G_TASK (result), error); \
} \
static DexFuture * \
G_PASTE (test_complete_, T) (DexFuture *future, gpointer user_data) \
{ \
  GMainLoop *main_loop = user_data; \
  GError *error = NULL; \
  const GValue *value = dex_future_get_value (future, &error); \
  g_assert_cmpint (dex_future_get_status (future), ==, DEX_FUTURE_STATUS_REJECTED); \
  g_assert_error (error, DEX_ERROR, DEX_ERROR_DEPENDENCY_FAILED); \
  g_assert_null (value); \
  g_main_loop_quit (main_loop); \
  return NULL; \
} \
static void \
G_PASTE (test_async_pair_, T) (void) \
{ \
  GMainLoop *main_loop = g_main_loop_new (NULL, FALSE); \
  GObject *instance = G_OBJECT (g_menu_new ()); \
  DexFuture *future = dex_async_pair_new (instance, \
                                          &G_PASTE (DEX_ASYNC_PAIR_INFO_, NAME) (G_PASTE (test_async_, T), \
                                                                                 G_PASTE (test_finish_, T))); \
  future = dex_future_finally (future, G_PASTE (test_complete_, T), main_loop, NULL); \
  g_main_loop_run (main_loop); \
  dex_unref (future); \
  g_clear_object (&instance); \
  g_main_loop_unref (main_loop); \
}

typedef GObject ErrorTest;
ASYNC_TEST_ERROR (ErrorTest, OBJECT, pointer)

static void
test_future_set_first_preresolved (void)
{
  DexFuture *promise1 = dex_future_new_for_int (123);
  DexFuture *promise2 = dex_future_new_for_int (321);
  DexFuture *future = dex_future_first (DEX_FUTURE (promise1), promise2, NULL);
  GError *error = NULL;
  const GValue *value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 123);
  dex_unref (future);
}

static void
test_future_set_all_race_preresolved (void)
{
  DexFuture *promise1 = dex_future_new_for_int (123);
  DexFuture *promise2 = dex_future_new_for_int (321);
  DexFuture *future = dex_future_all_race (DEX_FUTURE (promise1), promise2, NULL);
  GError *error = NULL;
  const GValue *value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (value));
  g_assert_cmpint (g_value_get_boolean (value), ==, TRUE);
  dex_unref (future);
}

static void
test_future_set_any_preresolved (void)
{
  DexFuture *promise1 = dex_future_new_for_int (123);
  DexFuture *promise2 = dex_future_new_for_int (321);
  DexFuture *future = dex_future_any (DEX_FUTURE (promise1), promise2, NULL);
  GError *error = NULL;
  const GValue *value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 123);
  dex_unref (future);
}

static void
test_future_set_all_preresolved (void)
{
  DexFuture *promise1 = dex_future_new_for_int (123);
  DexFuture *promise2 = dex_future_new_for_int (321);
  DexFuture *future = dex_future_all (DEX_FUTURE (promise1), promise2, NULL);
  GError *error = NULL;
  const GValue *value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_BOOLEAN (value));
  g_assert_cmpint (g_value_get_boolean (value), ==, TRUE);
  dex_unref (future);
}

static void
test_future_set_all_preresolved_error (void)
{
  DexFuture *promise1 = dex_future_new_for_int (123);
  DexCancellable *cancel1 = dex_cancellable_new ();
  DexFuture *future;
  GError *error = NULL;
  const GValue *value;

  dex_cancellable_cancel (cancel1);

  future = dex_future_all (DEX_FUTURE (promise1), cancel1, NULL);
  value = dex_future_get_value (future, &error);

  g_assert_null (value);
  g_assert_error (error, DEX_ERROR, DEX_ERROR_DEPENDENCY_FAILED);

  g_clear_error (&error);
  dex_unref (future);
}

static void
test_future_set_any_preresolved_error (void)
{
  DexPromise *promise1 = dex_promise_new ();
  DexCancellable *cancel1 = dex_cancellable_new ();
  DexFuture *future;
  GError *error = NULL;
  const GValue *value;

  dex_cancellable_cancel (cancel1);

  future = dex_future_any (DEX_FUTURE (promise1), cancel1, NULL);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (promise1, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_promise_resolve_int (promise1, 123);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (promise1, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_RESOLVED);

  value = dex_future_get_value (future, &error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (g_value_get_int (value), ==, 123);

  g_clear_error (&error);
  dex_unref (future);
}

static void
test_future_all (void)
{
  DexCancellable *cancel1 = dex_cancellable_new ();
  DexCancellable *cancel2 = dex_cancellable_new ();
  DexCancellable *cancel3 = dex_cancellable_new ();
  const GValue *value;
  DexFuture *future;
  GError *error = NULL;

  future = dex_future_all (dex_ref (cancel1), dex_ref (cancel2), dex_ref (cancel3), NULL);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel1);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel2);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel3);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);
  value = dex_future_get_value (future, &error);
  g_assert_null (value);
  g_assert_error (error, DEX_ERROR, DEX_ERROR_DEPENDENCY_FAILED);
  g_clear_error (&error);

  g_assert_true (DEX_IS_FUTURE_SET (future));
  g_assert_cmpint (dex_future_set_get_size (DEX_FUTURE_SET (future)), ==, 3);

  for (guint i = 0; i < dex_future_set_get_size (DEX_FUTURE_SET (future)); i++)
    {
      DexFuture *dep = dex_future_set_get_future_at (DEX_FUTURE_SET (future), i);
      value = dex_future_get_value (dep, &error);
      g_assert_null (value);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
      g_clear_error (&error);
    }

  dex_clear (&cancel1);
  dex_clear (&cancel2);
  dex_clear (&cancel3);
  dex_clear (&future);
}

static void
test_future_all_race (void)
{
  DexCancellable *cancel1 = dex_cancellable_new ();
  DexCancellable *cancel2 = dex_cancellable_new ();
  DexCancellable *cancel3 = dex_cancellable_new ();
  const GValue *value;
  DexFuture *future;
  GError *error = NULL;

  future = dex_future_all_race (dex_ref (cancel1), dex_ref (cancel2), dex_ref (cancel3), NULL);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel1);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);

  dex_cancellable_cancel (cancel2);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);

  dex_cancellable_cancel (cancel3);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);
  value = dex_future_get_value (future, &error);
  g_assert_null (value);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);

  g_assert_true (DEX_IS_FUTURE_SET (future));

  g_assert_cmpint (dex_future_set_get_size (DEX_FUTURE_SET (future)), ==, 3);
  for (guint i = 0; i < dex_future_set_get_size (DEX_FUTURE_SET (future)); i++)
    {
      DexFuture *dep = dex_future_set_get_future_at (DEX_FUTURE_SET (future), i);
      value = dex_future_get_value (dep, &error);
      g_assert_null (value);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
      g_clear_error (&error);
    }

  dex_clear (&cancel1);
  dex_clear (&cancel2);
  dex_clear (&cancel3);
  dex_clear (&future);
}

static void
test_future_any (void)
{
  DexCancellable *cancel1 = dex_cancellable_new ();
  DexCancellable *cancel2 = dex_cancellable_new ();
  DexCancellable *cancel3 = dex_cancellable_new ();
  const GValue *value;
  DexFuture *future;
  GError *error = NULL;

  future = dex_future_any (dex_ref (cancel1), dex_ref (cancel2), dex_ref (cancel3), NULL);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel1);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel2);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel3);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);
  value = dex_future_get_value (future, &error);
  g_assert_null (value);
  g_assert_error (error, DEX_ERROR, DEX_ERROR_DEPENDENCY_FAILED);
  g_clear_error (&error);

  g_assert_true (DEX_IS_FUTURE_SET (future));

  g_assert_cmpint (dex_future_set_get_size (DEX_FUTURE_SET (future)), ==, 3);
  for (guint i = 0; i < dex_future_set_get_size (DEX_FUTURE_SET (future)); i++)
    {
      DexFuture *dep = dex_future_set_get_future_at (DEX_FUTURE_SET (future), i);
      value = dex_future_get_value (dep, &error);
      g_assert_null (value);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
      g_clear_error (&error);
    }

  dex_clear (&cancel1);
  dex_clear (&cancel2);
  dex_clear (&cancel3);
  dex_clear (&future);
}

static void
test_future_first (void)
{
  DexCancellable *cancel1 = dex_cancellable_new ();
  DexCancellable *cancel2 = dex_cancellable_new ();
  DexCancellable *cancel3 = dex_cancellable_new ();
  const GValue *value;
  DexFuture *future;
  GError *error = NULL;

  future = dex_future_first (dex_ref (cancel1), dex_ref (cancel2), dex_ref (cancel3), NULL);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);

  dex_cancellable_cancel (cancel1);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);

  dex_cancellable_cancel (cancel2);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);

  dex_cancellable_cancel (cancel3);
  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel2, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (cancel3, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_REJECTED);
  value = dex_future_get_value (future, &error);
  g_assert_null (value);
  g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
  g_clear_error (&error);

  g_assert_true (DEX_IS_FUTURE_SET (future));

  g_assert_cmpint (dex_future_set_get_size (DEX_FUTURE_SET (future)), ==, 3);
  for (guint i = 0; i < dex_future_set_get_size (DEX_FUTURE_SET (future)); i++)
    {
      DexFuture *dep = dex_future_set_get_future_at (DEX_FUTURE_SET (future), i);
      value = dex_future_get_value (dep, &error);
      g_assert_null (value);
      g_assert_error (error, G_IO_ERROR, G_IO_ERROR_CANCELLED);
      g_clear_error (&error);
    }

  dex_clear (&cancel1);
  dex_clear (&cancel2);
  dex_clear (&cancel3);
  dex_clear (&future);
}

static void
discard_async (GMenu               *menu,
               GCancellable        *cancellable,
               GAsyncReadyCallback  callback,
               gpointer             user_data)
{
  GTask *task;

  g_print ("Discard async\n");

  ASSERT_INSTANCE_TYPE (menu, G_TYPE_MENU);
  ASSERT_INSTANCE_TYPE (cancellable, G_TYPE_CANCELLABLE);

  task = g_task_new (menu, cancellable, callback, user_data);
  g_task_set_check_cancellable (task, TRUE);
  g_task_set_return_on_cancel (task, TRUE);
}

static gboolean
discard_finish (GMenu         *menu,
                GAsyncResult  *result,
                GError       **error)
{
  GTask *task = G_TASK (result);
  gboolean ret;
  ASSERT_INSTANCE_TYPE (menu, G_TYPE_MENU);
  g_task_return_error_if_cancelled (task);
  ret = g_task_propagate_boolean (task, error);
  g_object_unref (task);
  return ret;
}

static void
test_future_discard_cancelled (GCancellable *cancellable,
                               gpointer      user_data)
{
  gboolean *was_cancelled = user_data;
  *was_cancelled = TRUE;
}

static void
test_future_discard (void)
{
  GMenu *menu = g_menu_new ();
  DexFuture *call = dex_async_pair_new (menu, &DEX_ASYNC_PAIR_INFO (discard_async, discard_finish, G_TYPE_BOOLEAN));
  DexCancellable *cancel1;
  DexFuture *any;
  gboolean was_cancelled = FALSE;

  ASSERT_INSTANCE_TYPE (call, DEX_TYPE_ASYNC_PAIR);
  ASSERT_INSTANCE_TYPE (DEX_ASYNC_PAIR (call)->instance, G_TYPE_MENU);
  ASSERT_INSTANCE_TYPE (DEX_ASYNC_PAIR (call)->cancellable, G_TYPE_CANCELLABLE);

  cancel1 = dex_cancellable_new ();
  ASSERT_INSTANCE_TYPE (cancel1, DEX_TYPE_CANCELLABLE);

  any = dex_future_first (call, cancel1, NULL);
  ASSERT_INSTANCE_TYPE (any, DEX_TYPE_FUTURE_SET);

  g_signal_connect (DEX_ASYNC_PAIR (call)->cancellable,
                    "cancelled",
                    G_CALLBACK (test_future_discard_cancelled),
                    &was_cancelled);

  dex_cancellable_cancel (cancel1);

  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (call, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (any, DEX_FUTURE_STATUS_REJECTED);

  g_assert_true (was_cancelled);

  ASSERT_STATUS (cancel1, DEX_FUTURE_STATUS_REJECTED);
  ASSERT_STATUS (call, DEX_FUTURE_STATUS_PENDING);
  ASSERT_STATUS (any, DEX_FUTURE_STATUS_REJECTED);

  g_clear_object (&menu);
  dex_clear (&any);
}

#ifdef G_OS_UNIX
static void
test_unix_signal_sigusr2 (void)
{
  DexFuture *future = dex_unix_signal_new (SIGUSR2);
  const GValue *value;
  GError *error = NULL;

  kill (getpid (), SIGUSR2);

  while (dex_future_get_status (future) == DEX_FUTURE_STATUS_PENDING)
    g_main_context_iteration (g_main_context_default (), TRUE);

  ASSERT_STATUS (future, DEX_FUTURE_STATUS_RESOLVED);
  value = dex_future_get_value (future, &error);
  g_assert_no_error (error);
  g_assert_nonnull (value);
  g_assert_true (G_VALUE_HOLDS_INT (value));
  g_assert_cmpint (SIGUSR2, ==, g_value_get_int (value));

  dex_unref (future);
}
#endif

static void
test_delayed_simple (void)
{
  DexFuture *result = dex_future_new_for_int (123);
  DexFuture *delayed = dex_delayed_new (dex_ref (result));

  ASSERT_STATUS (result, DEX_FUTURE_STATUS_RESOLVED);
  ASSERT_STATUS (delayed, DEX_FUTURE_STATUS_PENDING);

  dex_delayed_release (DEX_DELAYED (delayed));
  ASSERT_STATUS (delayed, DEX_FUTURE_STATUS_RESOLVED);

  dex_clear (&delayed);
  dex_clear (&result);
}

static void
test_future_name (void)
{
  DexFuture *future = DEX_FUTURE (dex_promise_new ());
  dex_future_set_static_name (future, "futuristic programming");
  g_assert_cmpstr ("futuristic programming", ==, dex_future_get_name (future));
  dex_unref (future);
}

static void
test_infinite_simple (void)
{
  DexFuture *future = dex_future_new_infinite ();
  ASSERT_STATUS (future, DEX_FUTURE_STATUS_PENDING);
  dex_unref (future);
}

int
main (int   argc,
      char *argv[])
{
  dex_init ();
  g_test_init (&argc, &argv, NULL);
  g_test_add_func ("/Dex/TestSuite/Future/name", test_future_name);
  g_test_add_func ("/Dex/TestSuite/Block/then", test_future_then);
  g_test_add_func ("/Dex/TestSuite/Cancellable/cancel", test_cancellable_cancel);
  g_test_add_func ("/Dex/TestSuite/StaticFuture/new", test_static_future_new);
  g_test_add_func ("/Dex/TestSuite/Promise/type", test_promise_type);
  g_test_add_func ("/Dex/TestSuite/Promise/autoptr", test_promise_autoptr);
  g_test_add_func ("/Dex/TestSuite/Promise/new", test_promise_new);
  g_test_add_func ("/Dex/TestSuite/Promise/resolve", test_promise_resolve);
  g_test_add_func ("/Dex/TestSuite/Timeout/timed-out", test_timeout);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/boolean", test_async_pair_gboolean);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/int", test_async_pair_int);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/uint", test_async_pair_guint);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/long", test_async_pair_glong);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/ulong", test_async_pair_gulong);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/int64", test_async_pair_gint64);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/uint64", test_async_pair_guint64);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/string", test_async_pair_char);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/object", test_async_pair_GObject);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/pointer", test_async_pair_gpointer);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/flags", test_async_pair_GSubprocessFlags);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/enums", test_async_pair_DexFutureStatus);
  g_test_add_func ("/Dex/TestSuite/AsyncPair/GError", test_async_pair_ErrorTest);
  g_test_add_func ("/Dex/TestSuite/Future/first_preresolved", test_future_set_first_preresolved);
  g_test_add_func ("/Dex/TestSuite/Future/all_race_preresolved", test_future_set_all_race_preresolved);
  g_test_add_func ("/Dex/TestSuite/Future/any_preresolved", test_future_set_any_preresolved);
  g_test_add_func ("/Dex/TestSuite/Future/all_preresolved", test_future_set_all_preresolved);
  g_test_add_func ("/Dex/TestSuite/Future/all_preresolved_error", test_future_set_all_preresolved_error);
  g_test_add_func ("/Dex/TestSuite/Future/any_preresolved_error", test_future_set_any_preresolved_error);
  g_test_add_func ("/Dex/TestSuite/Future/all", test_future_all);
  g_test_add_func ("/Dex/TestSuite/Future/all_race", test_future_all_race);
  g_test_add_func ("/Dex/TestSuite/Future/any", test_future_any);
  g_test_add_func ("/Dex/TestSuite/Future/first", test_future_first);
  g_test_add_func ("/Dex/TestSuite/Future/discard", test_future_discard);
  g_test_add_func ("/Dex/TestSuite/Delayed/simple", test_delayed_simple);
  g_test_add_func ("/Dex/TestSuite/Infinite/simple", test_infinite_simple);
#ifdef G_OS_UNIX
  g_test_add_func ("/Dex/TestSuite/UnixSignal/sigusr2", test_unix_signal_sigusr2);
#endif
  return g_test_run ();
}
