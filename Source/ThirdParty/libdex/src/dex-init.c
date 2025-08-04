/* libdex.c
 *
 * Copyright 2022 Christian Hergert
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "config.h"

#include "libdex.h"

#include "dex-main-scheduler-private.h"
#include "dex-infinite-private.h"
#include "dex-scheduler-private.h"
#include "dex-semaphore-private.h"
#include "dex-thread-pool-worker-private.h"

#ifdef HAVE_LIBURING
# include "dex-uring-future-private.h"
#endif

#include "gconstructor.h"

static void
dex_init_once (void)
{
  DexMainScheduler *main_scheduler;

  (void)dex_error_quark ();

  /* Base object, always register first */
  g_type_ensure (DEX_TYPE_OBJECT);

  /* Scheduler type */
  g_type_ensure (DEX_TYPE_SCHEDULER);
  g_type_ensure (DEX_TYPE_MAIN_SCHEDULER);
  g_type_ensure (DEX_TYPE_THREAD_POOL_SCHEDULER);
  g_type_ensure (DEX_TYPE_THREAD_POOL_WORKER);

  /* Future types */
  g_type_ensure (DEX_TYPE_FUTURE);
  g_type_ensure (DEX_TYPE_ASYNC_PAIR);
  g_type_ensure (DEX_TYPE_FIBER);
  g_type_ensure (DEX_TYPE_FUTURE_SET);
  g_type_ensure (DEX_TYPE_BLOCK);
  g_type_ensure (DEX_TYPE_CANCELLABLE);
  g_type_ensure (DEX_TYPE_PROMISE);
  g_type_ensure (DEX_TYPE_STATIC_FUTURE);
  g_type_ensure (DEX_TYPE_TIMEOUT);
  g_type_ensure (DEX_TYPE_INFINITE);
#ifdef G_OS_UNIX
  g_type_ensure (DEX_TYPE_UNIX_SIGNAL);
#endif
#ifdef HAVE_LIBURING
  g_type_ensure (DEX_TYPE_URING_FUTURE);
#endif

  /* Misc types */
  g_type_ensure (DEX_TYPE_ASYNC_RESULT);
  g_type_ensure (DEX_TYPE_CHANNEL);
  g_type_ensure (DEX_TYPE_SEMAPHORE);

  /* Setup default scheduler for application */
  main_scheduler = dex_main_scheduler_new (NULL);
  dex_scheduler_set_default (DEX_SCHEDULER (main_scheduler));
}

void
dex_init (void)
{
  static gsize initialized;

  if (g_once_init_enter (&initialized))
    {
      dex_init_once ();
      g_once_init_leave (&initialized, TRUE);
    }
}

G_DEFINE_CONSTRUCTOR (dex_init_ctor)

static void
dex_init_ctor (void)
{
  dex_init ();
}
