/* dex-platform.c
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

#include "dex-platform.h"

#ifdef G_OS_WIN32
# include <windows.h>
#else
# include <unistd.h>
#endif

gsize
dex_get_page_size (void)
{
  static gsize page_size;

  if G_UNLIKELY (page_size == 0)
    {
#ifdef G_OS_WIN32
      SYSTEM_INFO si;
      GetSystemInfo (&si);
      page_size = si.dwPageSize;
#else
      page_size = sysconf (_SC_PAGESIZE);
#endif
    }

  return page_size;
}

gsize
dex_get_min_stack_size (void)
{
  static gsize min_stack_size;
  if G_UNLIKELY (min_stack_size == 0)
    {
#ifdef G_OS_WIN32
      /* Probably need to base this on granularity or something,
       * because the default stack size of 1MB is likely too much.
       */
      min_stack_size = 4096*16;
#else
      long sc_thread_stack_min;

      /* On FreeBSD we can get 2048 for min-stack-size which
       * doesn't really work well for pretty much anything.
       * Even when we round up to 4096 we trivially hit the
       * guard page. Perhaps something still needs to be
       * fixed in DexStack to ensure we don't write somewhere
       * we shouldn't, but the easiest thing to do right now
       * is to just enforce 2+pages + guard page.
       */
      min_stack_size = dex_get_page_size () * 2;

      sc_thread_stack_min = sysconf (_SC_THREAD_STACK_MIN);
      if (sc_thread_stack_min != -1 && sc_thread_stack_min > min_stack_size)
        min_stack_size = sc_thread_stack_min;
#endif
    }

  return min_stack_size;
}
