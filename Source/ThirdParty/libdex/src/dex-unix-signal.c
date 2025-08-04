/*
 * dex-unix-signal.c
 *
 * Copyright 2022-2023 Christian Hergert <chergert@redhat.com>
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

#include <glib-unix.h>

#include "dex-compat-private.h"
#include "dex-future-private.h"
#include "dex-unix-signal.h"

/**
 * DexUnixSignal:
 *
 * #DexUnixSignal is a #DexFuture that will resolve when a specific unix
 * signal has been received.
 *
 * Use this when you want to handle a signal from your main loop rather than
 * from a resticted operating signal handler.
 *
 * On Linux, this uses a signalfd.
 */

struct _DexUnixSignal
{
  DexFuture parent_instance;
  GSource *source;
  int signum;
};

typedef struct _DexUnixSignalClass
{
  DexFutureClass parent_class;
} DexUnixSignalClass;

DEX_DEFINE_FINAL_TYPE (DexUnixSignal, dex_unix_signal, DEX_TYPE_FUTURE)

static void
dex_unix_signal_finalize (DexObject *object)
{
  DexUnixSignal *unix_signal = DEX_UNIX_SIGNAL (object);

  if (unix_signal->source != NULL)
    {
      g_source_destroy (unix_signal->source);
      g_clear_pointer (&unix_signal->source, g_source_unref);
    }

  DEX_OBJECT_CLASS (dex_unix_signal_parent_class)->finalize (object);
}

static void
dex_unix_signal_class_init (DexUnixSignalClass *unix_signal_class)
{
  DexObjectClass *object_class = DEX_OBJECT_CLASS (unix_signal_class);

  object_class->finalize = dex_unix_signal_finalize;
}

static void
dex_unix_signal_init (DexUnixSignal *unix_signal)
{
}

static gboolean
dex_unix_signal_source_func (gpointer data)
{
  DexWeakRef *wr = data;
  DexUnixSignal *unix_signal = dex_weak_ref_get (wr);

  g_assert (!unix_signal || DEX_IS_UNIX_SIGNAL (unix_signal));

  if (unix_signal != NULL)
    {
      GValue value = G_VALUE_INIT;

      g_value_init (&value, G_TYPE_INT);
      g_value_set_int (&value, unix_signal->signum);
      dex_future_complete (DEX_FUTURE (unix_signal), &value, NULL);
    }

  return G_SOURCE_REMOVE;
}

static void
clear_weak_ref (gpointer data)
{
  dex_weak_ref_clear (data);
  g_free (data);
}

/**
 * dex_unix_signal_new:
 * @signum: a unix signal number
 *
 * Creates a new #DexUnixSignal that completes when @signum is delivered
 * to the process.
 *
 * @signum must be one of SIGHUP, SIGINT, SIGTERM, SIGUSR1, SIGUSR2, or
 * SIGWINCH.
 *
 * This API is only supported on UNIX-like systems.
 *
 * Returns: (transfer full): a new #DexFuture
 */
DexFuture *
dex_unix_signal_new (int signum)
{
  DexUnixSignal *unix_signal;
  const char *name = NULL;
  DexWeakRef *wr;

  g_return_val_if_fail (signum == SIGHUP || signum == SIGINT || signum == SIGTERM ||
                        signum == SIGUSR1 || signum == SIGUSR2 || signum == SIGWINCH,
                        NULL);

  switch (signum)
    {
    case SIGHUP: name = "[dex-unix-signal-SIGHUP]"; break;
    case SIGINT: name = "[dex-unix-signal-SIGINT]"; break;
    case SIGTERM: name = "[dex-unix-signal-SIGTERM]"; break;
    case SIGUSR1: name = "[dex-unix-signal-SIGUSR1]"; break;
    case SIGUSR2: name = "[dex-unix-signal-SIGUSR2]"; break;
    case SIGWINCH: name = "[dex-unix-signal-SIGWINCH]"; break;
    default:
      g_assert_not_reached ();
    }

  unix_signal = (DexUnixSignal *)dex_object_create_instance (DEX_TYPE_UNIX_SIGNAL);
  unix_signal->signum = signum;
  unix_signal->source = g_unix_signal_source_new (signum);

  wr = g_new0 (DexWeakRef, 1);
  dex_weak_ref_init (wr, unix_signal);

  g_source_set_callback (unix_signal->source,
                         dex_unix_signal_source_func,
                         wr,
                         clear_weak_ref);
  _g_source_set_static_name (unix_signal->source, name);
  g_source_attach (unix_signal->source, NULL);

  return DEX_FUTURE (unix_signal);
}
