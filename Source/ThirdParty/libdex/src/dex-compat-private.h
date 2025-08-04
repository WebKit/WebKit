/* dex-compat-private.h
 *
 * Copyright 2022 Christian Hergert <christian@hergert.me>
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

#include <glib-object.h>

#if !GLIB_CHECK_VERSION(2, 72, 0)
# ifndef _ISOC11_SOURCE
#  define _ISOC11_SOURCE
# endif
# include <stdlib.h>
# include <string.h>
#endif

#ifdef G_OS_WIN32
# include <windows.h>
# include <io.h>
#endif

G_BEGIN_DECLS

#if !GLIB_CHECK_VERSION(2, 68, 0)
static inline gpointer
g_memdup2 (gconstpointer mem,
           gsize         byte_size)
{
  g_assert (byte_size <= G_MAXUINT);
  return g_memdup (mem, byte_size);
}
#endif

static inline void
_g_source_set_static_name (GSource    *source,
                           const char *name)
{
#if GLIB_CHECK_VERSION(2, 70, 0)
  g_source_set_static_name (source, name);
#else
  g_source_set_name (source, name);
#endif
}

#if !GLIB_CHECK_VERSION(2, 70, 0)
# define G_DEFINE_FINAL_TYPE_WITH_CODE(TN, t_n, T_P, _C_) _G_DEFINE_TYPE_EXTENDED_BEGIN (TN, t_n, T_P, 0) {_C_;} _G_DEFINE_TYPE_EXTENDED_END()
# define G_TYPE_FLAG_FINAL 0
# define G_TYPE_IS_FINAL(type) 1
#endif

#if !GLIB_CHECK_VERSION(2, 73, 0)
# define G_DEFINE_ENUM_VALUE(EnumValue, EnumNick) \
  { EnumValue, #EnumValue, EnumNick }
# define G_DEFINE_ENUM_TYPE(TypeName, type_name, ...) \
GType \
G_PASTE(type_name, _get_type) (void) \
{ \
  static gsize g_define_type__static = 0; \
  if (g_once_init_enter (&g_define_type__static)) { \
    static const GEnumValue enum_values[] = { \
      __VA_ARGS__ , \
      { 0, NULL, NULL }, \
    }; \
    GType g_define_type = g_enum_register_static (g_intern_static_string (G_STRINGIFY (TypeName)), enum_values); \
    g_once_init_leave (&g_define_type__static, g_define_type); \
  } \
  return g_define_type__static; \
}
#endif

#if !GLIB_CHECK_VERSION(2, 72, 0)
static inline gpointer
g_aligned_alloc (gsize n_blocks,
                 gsize n_block_bytes,
                 gsize alignment)
{
  gpointer mem = aligned_alloc (alignment, n_blocks * n_block_bytes);

  if (mem == NULL)
    g_error ("Failed to allocate %"G_GSIZE_FORMAT" bytes",
             n_blocks * n_block_bytes);

  return mem;
}
static inline gpointer
g_aligned_alloc0 (gsize n_blocks,
                  gsize n_block_bytes,
                  gsize alignment)
{
  gpointer mem = g_aligned_alloc (n_blocks, n_block_bytes, alignment);
  memset (mem, 0, n_blocks * n_block_bytes);
  return mem;
}
static inline void
g_aligned_free (gpointer mem)
{
  free (mem);
}
#endif

#ifdef G_OS_WIN32
static inline gsize
pread (int      fd,
       gpointer buf,
       gsize    count,
       goffset  offset)
{
  OVERLAPPED ov = {0};
  DWORD n_read = 0;
  HANDLE hFile;

  hFile = (HANDLE)_get_osfhandle (fd);

  ov.OffsetHigh = (offset & 0xFFFFFFFF00000000LL) >> 32;
  ov.Offset = (offset & 0xFFFFFFFFLL);

  SetLastError (0);

  if (!ReadFile (hFile, buf, count, &n_read, &ov))
    {
      int errsv = GetLastError ();

      if (errsv != ERROR_HANDLE_EOF)
        {
          errno = errsv;
          return -1;
        }
    }

  return n_read;
}

static inline gsize
pwrite (int           fd,
        gconstpointer buf,
        gsize         count,
        goffset       offset)
{
  OVERLAPPED ov = {0};
  DWORD n_written = 0;
  HANDLE hFile;

  hFile = (HANDLE)_get_osfhandle (fd);

  ov.OffsetHigh = (offset & 0xFFFFFFFF00000000LL) >> 32;
  ov.Offset = (offset & 0xFFFFFFFFLL);

  SetLastError (0);

  if (!WriteFile (hFile, buf, count, &n_written, &ov))
    {
      errno = GetLastError ();
      return -1;
    }

  return n_written;
}
#endif

G_END_DECLS
