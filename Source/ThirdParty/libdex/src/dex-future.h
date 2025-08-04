/*
 * dex-future.h
 *
 * Copyright 2022-2023 Christian Hergert <chergert@gnome.org>
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

#pragma once

#if !defined (DEX_INSIDE) && !defined (DEX_COMPILATION)
# error "Only <libdex.h> can be included directly."
#endif

#include "dex-enums.h"
#include "dex-object.h"

G_BEGIN_DECLS

#define DEX_TYPE_FUTURE       (dex_future_get_type())
#define DEX_FUTURE(object)    (G_TYPE_CHECK_INSTANCE_CAST(object, DEX_TYPE_FUTURE, DexFuture))
#define DEX_IS_FUTURE(object) (G_TYPE_CHECK_INSTANCE_TYPE(object, DEX_TYPE_FUTURE))

typedef struct _DexFuture DexFuture;

/**
 * DexFutureCallback:
 * @future: a resolved or rejected #DexFuture
 * @user_data: closure data associated with the callback
 *
 * A #DexFutureCallback can be executed from a #DexBlock as response to
 * another #DexFuture resolving or rejecting.
 *
 * The callback will be executed within the scheduler environment the
 * block is created within when using dex_future_then(), dex_future_catch(),
 * dex_future_finally(), dex_future_all(), and similar functions.
 *
 * This is the expected way to handle completion of a future when not using
 * #DexFiber via dex_scheduler_spawn().
 *
 * Returns: (transfer full) (nullable): a #DexFuture or %NULL
 */
typedef DexFuture *(*DexFutureCallback) (DexFuture *future,
                                         gpointer   user_data);

DEX_AVAILABLE_IN_ALL
GType            dex_future_get_type        (void) G_GNUC_CONST;
DEX_AVAILABLE_IN_ALL
DexFutureStatus  dex_future_get_status      (DexFuture          *future);
DEX_AVAILABLE_IN_ALL
gboolean         dex_future_is_resolved     (DexFuture          *future);
DEX_AVAILABLE_IN_ALL
gboolean         dex_future_is_rejected     (DexFuture          *future);
DEX_AVAILABLE_IN_ALL
gboolean         dex_future_is_pending      (DexFuture          *future);
DEX_AVAILABLE_IN_ALL
const GValue    *dex_future_get_value       (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_value   (const GValue       *value)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_error   (GError             *error)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_infinite    (void)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_reject      (GQuark              domain,
                                             int                 error_code,
                                             const char         *format,
                                             ...) G_GNUC_PRINTF (3, 4)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_errno   (int                 errno_)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_string  (const char         *string)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_take_string (char               *string)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_int     (int                 v_int)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_uint    (guint               v_uint)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_int64   (gint64              v_int64)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_uint64  (guint64             v_uint64)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_boolean (gboolean            v_bool)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_double  (double              v_double)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_float   (float               v_float)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_take_variant(GVariant           *v_variant)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_take_boxed  (GType               boxed_type,
                                             gpointer            value)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_pointer (gpointer            pointer)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_for_object  (gpointer            value)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_new_take_object (gpointer            value)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_then            (DexFuture          *future,
                                             DexFutureCallback   callback,
                                             gpointer            callback_data,
                                             GDestroyNotify      callback_data_destroy)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_then_loop       (DexFuture          *future,
                                             DexFutureCallback   callback,
                                             gpointer            callback_data,
                                             GDestroyNotify      callback_data_destroy)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_catch           (DexFuture          *future,
                                             DexFutureCallback   callback,
                                             gpointer            callback_data,
                                             GDestroyNotify      callback_data_destroy)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_catch_loop      (DexFuture          *future,
                                             DexFutureCallback   callback,
                                             gpointer            callback_data,
                                             GDestroyNotify      callback_data_destroy)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_finally         (DexFuture          *future,
                                             DexFutureCallback   callback,
                                             gpointer            callback_data,
                                             GDestroyNotify      callback_data_destroy)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_finally_loop    (DexFuture          *future,
                                             DexFutureCallback   callback,
                                             gpointer            callback_data,
                                             GDestroyNotify      callback_data_destroy)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_all             (DexFuture          *first_future,
                                             ...) G_GNUC_NULL_TERMINATED
G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_allv            (DexFuture * const  *futures,
                                             guint               n_futures)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_all_race        (DexFuture          *first_future,
                                             ...) G_GNUC_NULL_TERMINATED
G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_all_racev       (DexFuture * const  *futures,
                                             guint               n_futures)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_any             (DexFuture          *first_future,
                                             ...)
  G_GNUC_NULL_TERMINATED
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_anyv            (DexFuture * const  *futures,
                                             guint               n_futures)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_first           (DexFuture          *first_future,
                                             ...)
  G_GNUC_NULL_TERMINATED
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexFuture       *dex_future_firstv          (DexFuture  * const *futures,
                                             guint               n_futures)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
const char      *dex_future_get_name        (DexFuture          *future);
DEX_AVAILABLE_IN_ALL
void             dex_future_set_static_name (DexFuture          *future,
                                             const char         *name);
DEX_AVAILABLE_IN_ALL
void             dex_future_disown          (DexFuture          *future);
DEX_AVAILABLE_IN_ALL
gboolean         dex_await                  (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
gboolean         dex_await_boolean          (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
guint            dex_await_enum             (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
guint            dex_await_flags            (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
char            *dex_await_string           (DexFuture          *future,
                                             GError            **error)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
float            dex_await_float            (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
double           dex_await_double           (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
gpointer         dex_await_pointer          (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
int              dex_await_int              (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
guint            dex_await_uint             (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
gint64           dex_await_int64            (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
guint64          dex_await_uint64           (DexFuture          *future,
                                             GError            **error);
DEX_AVAILABLE_IN_ALL
GVariant        *dex_await_variant          (DexFuture          *future,
                                             GError            **error)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
gpointer         dex_await_boxed            (DexFuture          *future,
                                             GError            **error)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
gpointer         dex_await_object           (DexFuture          *future,
                                             GError            **error)
  G_GNUC_WARN_UNUSED_RESULT;

#if G_GNUC_CHECK_VERSION(3,0) && defined(DEX_ENABLE_DEBUG)
# define _DEX_FUTURE_NEW_(func, counter, ...) \
  ({ DexFuture *G_PASTE(__f, counter) = G_PASTE (dex_future_, func) (__VA_ARGS__); \
     dex_future_set_static_name (DEX_FUTURE (G_PASTE (__f, counter)), G_STRLOC); \
     G_PASTE (__f, counter); })
# define _DEX_FUTURE_NEW(func, ...) _DEX_FUTURE_NEW_(func, __COUNTER__, __VA_ARGS__)
# define dex_future_then(...) _DEX_FUTURE_NEW(then, __VA_ARGS__)
# define dex_future_then_loop(...) _DEX_FUTURE_NEW(then_loop, __VA_ARGS__)
# define dex_future_catch(...) _DEX_FUTURE_NEW(catch, __VA_ARGS__)
# define dex_future_catch_loop(...) _DEX_FUTURE_NEW(catch_loop, __VA_ARGS__)
# define dex_future_finally(...) _DEX_FUTURE_NEW(finally, __VA_ARGS__)
# define dex_future_finally_loop(...) _DEX_FUTURE_NEW(finally_loop, __VA_ARGS__)
# define dex_future_all(...) _DEX_FUTURE_NEW(all, __VA_ARGS__)
# define dex_future_allv(...) _DEX_FUTURE_NEW(allv, __VA_ARGS__)
# define dex_future_all_race(...) _DEX_FUTURE_NEW(all_race, __VA_ARGS__)
# define dex_future_all_racev(...) _DEX_FUTURE_NEW(all_racev, __VA_ARGS__)
# define dex_future_any(...) _DEX_FUTURE_NEW(any, __VA_ARGS__)
# define dex_future_anyv(...) _DEX_FUTURE_NEW(anyv, __VA_ARGS__)
# define dex_future_first(...) _DEX_FUTURE_NEW(first, __VA_ARGS__)
# define dex_future_firstv(...) _DEX_FUTURE_NEW(firstv, __VA_ARGS__)
# define dex_future_new_for_value(...) _DEX_FUTURE_NEW(new_for_value, __VA_ARGS__)
# define dex_future_new_for_error(...) _DEX_FUTURE_NEW(new_for_error, __VA_ARGS__)
# define dex_future_new_for_errno(...) _DEX_FUTURE_NEW(new_for_errno, __VA_ARGS__)
# define dex_future_new_for_string(...) _DEX_FUTURE_NEW(new_for_string, __VA_ARGS__)
# define dex_future_new_take_string(...) _DEX_FUTURE_NEW(new_take_string, __VA_ARGS__)
# define dex_future_new_for_int(...) _DEX_FUTURE_NEW(new_for_int, __VA_ARGS__)
# define dex_future_new_for_uint(...) _DEX_FUTURE_NEW(new_for_uint, __VA_ARGS__)
# define dex_future_new_for_int64(...) _DEX_FUTURE_NEW(new_for_int64, __VA_ARGS__)
# define dex_future_new_for_uint64(...) _DEX_FUTURE_NEW(new_for_uint64, __VA_ARGS__)
# define dex_future_new_for_boolean(...) _DEX_FUTURE_NEW(new_for_boolean, __VA_ARGS__)
# define dex_future_new_for_double(...) _DEX_FUTURE_NEW(new_for_double, __VA_ARGS__)
# define dex_future_new_for_float(...) _DEX_FUTURE_NEW(new_for_float, __VA_ARGS__)
# define dex_future_new_take_variant(...) _DEX_FUTURE_NEW(new_take_variant, __VA_ARGS__)
# define dex_future_new_take_boxed(...) _DEX_FUTURE_NEW(new_take_boxed, __VA_ARGS__)
# define dex_future_new_for_object(...) _DEX_FUTURE_NEW(new_for_object, __VA_ARGS__)
# define dex_future_new_take_object(...) _DEX_FUTURE_NEW(new_take_object, __VA_ARGS__)
# define dex_future_new_for_pointer(...) _DEX_FUTURE_NEW(new_for_pointer, __VA_ARGS__)
#endif

#define dex_future_new_true() dex_future_new_for_boolean(TRUE)
#define dex_future_new_false() dex_future_new_for_boolean(FALSE)

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexFuture, dex_unref)

G_END_DECLS
