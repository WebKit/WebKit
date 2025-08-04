/*
 * dex-scheduler.h
 *
 * Copyright 2022 Christian Hergert <chergert@gnome.org>
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

#include "dex-future.h"

G_BEGIN_DECLS

#define DEX_TYPE_SCHEDULER    (dex_scheduler_get_type())
#define DEX_SCHEDULER(obj)    (G_TYPE_CHECK_INSTANCE_CAST(obj, DEX_TYPE_SCHEDULER, DexScheduler))
#define DEX_IS_SCHEDULER(obj) (G_TYPE_CHECK_INSTANCE_TYPE(obj, DEX_TYPE_SCHEDULER))

typedef struct _DexScheduler DexScheduler;

typedef void (*DexSchedulerFunc) (gpointer user_data);

/**
 * DexFiberFunc:
 *
 * This function prototype is used for spawning fibers. A fiber
 * is a lightweight, cooperative-multitasking feature where the
 * fiber is given its own stack. The fiber runs until it reaches
 * a point of suspension (using `dex_await` or similar) or exits
 * the fiber.
 *
 * When suspended, the fiber is placed onto a queue until it is
 * runnable again. Once runnable, the fiber is scheduled to run
 * from within whatever scheduler it was created with.
 *
 * See `dex_scheduler_spawn()`
 *
 * Returns: (transfer full) (nullable): a #DexFuture or %NULL
 */
typedef DexFuture *(*DexFiberFunc) (gpointer user_data);

DEX_AVAILABLE_IN_ALL
GType         dex_scheduler_get_type           (void) G_GNUC_CONST;
DEX_AVAILABLE_IN_ALL
DexScheduler *dex_scheduler_get_thread_default (void);
DEX_AVAILABLE_IN_ALL
DexScheduler *dex_scheduler_ref_thread_default (void)
  G_GNUC_WARN_UNUSED_RESULT;
DEX_AVAILABLE_IN_ALL
DexScheduler *dex_scheduler_get_default        (void);
DEX_AVAILABLE_IN_ALL
GMainContext *dex_scheduler_get_main_context   (DexScheduler     *scheduler);
DEX_AVAILABLE_IN_ALL
void          dex_scheduler_push               (DexScheduler     *scheduler,
                                                DexSchedulerFunc  func,
                                                gpointer          func_data);
DEX_AVAILABLE_IN_ALL
DexFuture    *dex_scheduler_spawn              (DexScheduler     *scheduler,
                                                gsize             stack_size,
                                                DexFiberFunc      func,
                                                gpointer          func_data,
                                                GDestroyNotify    func_data_destroy)
  G_GNUC_WARN_UNUSED_RESULT;

#if G_GNUC_CHECK_VERSION(3,0) && defined(DEX_ENABLE_DEBUG)
# define _DEX_FIBER_NEW_(counter, ...) \
  ({ DexFuture *G_PASTE(__f, counter) = dex_scheduler_spawn (__VA_ARGS__); \
     dex_future_set_static_name (DEX_FUTURE (G_PASTE (__f, counter)), G_STRLOC); \
     G_PASTE (__f, counter); })
# define _DEX_FIBER_NEW(...) _DEX_FIBER_NEW_(__COUNTER__, __VA_ARGS__)
# define dex_scheduler_spawn(...) _DEX_FIBER_NEW(__VA_ARGS__)
#endif

G_DEFINE_AUTOPTR_CLEANUP_FUNC (DexScheduler, dex_unref)

G_END_DECLS
