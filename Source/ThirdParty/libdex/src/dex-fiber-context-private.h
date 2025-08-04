/* dex-fiber-context-private.h
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

#pragma once

#include <errno.h>
#include <string.h>

#include <glib.h>

#include "dex-compat-private.h"
#include "dex-platform.h"
#include "dex-stack-private.h"

#ifdef G_OS_UNIX
# include "dex-ucontext-private.h"
#endif

#ifdef G_OS_WIN32
# include <windows.h>
#endif

G_BEGIN_DECLS

typedef struct _DexFiberContextStart
{
  GHookFunc func;
  gpointer  data;
} DexFiberContextStart;

#ifdef G_OS_UNIX
/* If the system we're on has an alignment requirement of > sizeof(void*)
 * then we will allocate aligned memory for the ucontext_t instead of
 * including it inline. Otherwise, g_type_create_instance() (which only will
 * guarantee, at least via public API descriptions) a structure that is
 * guaranteed to be aligned to sizeof(void*).
 *
 * This is an issue, at minimum, on FreeBSD where x86_64 has a 16-bytes
 * alignment requirement for ucontext_t.
 */
#if ALIGN_OF_UCONTEXT > GLIB_SIZEOF_VOID_P
typedef ucontext_t *DexFiberContext;
#else
typedef ucontext_t DexFiberContext;
#endif

static inline void
dex_fiber_context_switch (DexFiberContext *old_context,
                          DexFiberContext *new_context);

static inline void
dex_fiber_context_start (guint start_lo,
                         guint start_hi)
{
  union {
    guintptr uval;
    DexFiberContextStart *ptr;
  } u;

  u.uval = 0;
#if GLIB_SIZEOF_VOID_P == 8
  u.uval |= start_hi;
  u.uval <<= 32;
#endif
  u.uval |= start_lo;

  u.ptr->func (u.ptr->data);
}

static inline void
_dex_fiber_context_makecontext (ucontext_t           *ucontext,
                                DexStack             *stack,
                                DexFiberContextStart *start)
{
  guint start_lo;
  guint start_hi = 0;

  ucontext->uc_stack.ss_size = stack->size;
  ucontext->uc_stack.ss_sp = stack->ptr;
  ucontext->uc_link = 0;

#if GLIB_SIZEOF_VOID_P == 8
  start_lo = GPOINTER_TO_SIZE (start) & 0xFFFFFFFFF;
  start_hi = (GPOINTER_TO_SIZE (start) >> 32) & 0xFFFFFFFFF;
#else
  start_lo = GPOINTER_TO_SIZE (start);
  start_hi = 0;
#endif

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  makecontext (ucontext,
               G_CALLBACK (dex_fiber_context_start),
               2, start_lo, start_hi);
  G_GNUC_END_IGNORE_DEPRECATIONS
}

static inline void
dex_fiber_context_init (DexFiberContext      *context,
                        DexStack             *stack,
                        DexFiberContextStart *start)
{
  ucontext_t *ucontext;

#if ALIGN_OF_UCONTEXT > GLIB_SIZEOF_VOID_P
  *context = g_aligned_alloc (1, sizeof (ucontext_t), ALIGN_OF_UCONTEXT);
  ucontext = *context;
#else
  ucontext = context;
#endif

  memset (ucontext, 0, sizeof *ucontext);

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  getcontext (ucontext);
  G_GNUC_END_IGNORE_DEPRECATIONS

  /* If stack is NULL, then this is a context used to save state
   * such as from the original stack in the fiber scheduler.
   */
  if (stack != NULL)
    _dex_fiber_context_makecontext (ucontext, stack, start);
}

static inline void
dex_fiber_context_clear (DexFiberContext *context)
{
#if ALIGN_OF_UCONTEXT > GLIB_SIZEOF_VOID_P
  g_aligned_free (*context);
#endif
}

static inline void
dex_fiber_context_init_main (DexFiberContext *context)
{
  dex_fiber_context_init (context, NULL, NULL);
}

static inline void
dex_fiber_context_clear_main (DexFiberContext *context)
{
  dex_fiber_context_clear (context);
}

static inline void
dex_fiber_context_switch (DexFiberContext *old_context,
                          DexFiberContext *new_context)
{
  int r;

  G_GNUC_BEGIN_IGNORE_DEPRECATIONS
#if ALIGN_OF_UCONTEXT > GLIB_SIZEOF_VOID_P
  r = swapcontext (*old_context, *new_context);
#else
  r = swapcontext (old_context, new_context);
#endif
  G_GNUC_END_IGNORE_DEPRECATIONS

#ifndef G_DISABLE_ASSERT
  if G_UNLIKELY (r != 0)
    g_error ("swapcontext(): %s", g_strerror (errno));
#else
  (void)r;
#endif
}
#endif

#ifdef G_OS_WIN32
typedef LPVOID DexFiberContext;

static inline void
dex_fiber_context_init_main (DexFiberContext *context)
{
  *context = ConvertThreadToFiber (0);
}

static inline void
dex_fiber_context_clear_main (DexFiberContext *context)
{
  *context = NULL;
  ConvertFiberToThread ();
}

static inline void
dex_fiber_context_init (DexFiberContext      *context,
                        DexStack             *stack,
                        DexFiberContextStart *start)
{
  *context = CreateFiberEx (dex_get_min_stack_size (),
                            stack->size,
                            FIBER_FLAG_FLOAT_SWITCH,
                            (LPFIBER_START_ROUTINE)start->func,
                            start->data);

  if (*context == NULL)
    g_printerr ("Failed to create fiber (stack %u) (start_func %p): %s",
                (guint)stack->size, start->func,
                g_strerror (GetLastError ()));
}

static inline void
dex_fiber_context_clear (DexFiberContext *context)
{
  DeleteFiber (*context);
}

static inline void
dex_fiber_context_switch (DexFiberContext *old_context,
                          DexFiberContext *new_context)
{
  SwitchToFiber (*new_context);
}
#endif

G_END_DECLS
