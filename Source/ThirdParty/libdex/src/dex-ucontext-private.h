/* dex-ucontext-private.h
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

#include <signal.h>
#include <stdarg.h>
#include <string.h>
#include <unistd.h>

#include "config.h"

/* The following code is from libtask by Russ Cox to emulate the
 * ucontext implementation in a fashion that doesn't necessarily
 * require context switches for implementation.
 */

/* Copyright (c) 2005-2006 Russ Cox, MIT; see COPYRIGHT */

#if defined(__sun__)
# define __EXTENSIONS__ 1 /* SunOS */
# if defined(__SunOS5_6__) || defined(__SunOS5_7__) || defined(__SunOS5_8__)
  /* NOT USING #define __MAKECONTEXT_V2_SOURCE 1 / * SunOS */
# else
#  define __MAKECONTEXT_V2_SOURCE 1
# endif
#endif

#define USE_UCONTEXT 1

#if defined(__OpenBSD__) || defined(__mips__)
# undef USE_UCONTEXT
# define USE_UCONTEXT 0
#endif

#if defined(__APPLE__)
# ifndef _XOPEN_SOURCE
#  define _XOPEN_SOURCE
# endif
#endif

#if USE_UCONTEXT
# include <ucontext.h>
#endif

#if defined(__FreeBSD__) && __FreeBSD__ < 5
extern int   getmcontext(mcontext_t*);
extern void  setmcontext(const mcontext_t*);
# define setcontext(u) setmcontext(&(u)->uc_mcontext)
# define getcontext(u) getmcontext(&(u)->uc_mcontext)
extern int   swapcontext(ucontext_t*, const ucontext_t*);
extern void  makecontext(ucontext_t*, void(*)(void), int, ...);
#endif

#if defined(__OpenBSD__)
# define mcontext libthread_mcontext
# define mcontext_t libthread_mcontext_t
# define ucontext libthread_ucontext
# define ucontext_t libthread_ucontext_t
# if defined __i386__
#  include "386-ucontext.h"
# else
#  include "power-ucontext.h"
# endif
#endif

#if defined(__linux__) && defined(__mips__)
# if !defined(HAVE_UCONTEXT_H)
#  include "mips-ucontext.h"
# endif
extern int getcontext (ucontext_t *ucp);
extern int setcontext(const ucontext_t *ucp);
extern int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);
/* glibc makecontext.S for mips specifies int return type, not void */
extern void makecontext(ucontext_t *ucp, void(*)(void), int, ...);
#endif
