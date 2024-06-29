/* sysprof-capture-util.c
 *
 * Copyright 2019 Christian Hergert <chergert@redhat.com>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * Subject to the terms and conditions of this license, each copyright holder
 * and contributor hereby grants to those receiving rights under this license
 * a perpetual, worldwide, non-exclusive, no-charge, royalty-free,
 * irrevocable (except for failure to satisfy the conditions of this license)
 * patent license to make, have made, use, offer to sell, sell, import, and
 * otherwise transfer this software, where such license applies only to those
 * patent claims, already acquired or hereafter acquired, licensable by such
 * copyright holder or contributor that are necessarily infringed by:
 *
 * (a) their Contribution(s) (the licensed copyrights of copyright holders
 *     and non-copyrightable additions of contributors, in source or binary
 *     form) alone; or
 *
 * (b) combination of their Contribution(s) with the work of authorship to
 *     which such Contribution(s) was added by such copyright holder or
 *     contributor, if, at the time the Contribution is added, such addition
 *     causes such combination to be necessarily infringed. The patent license
 *     shall not apply to any other combinations which include the
 *     Contribution.
 *
 * Except as expressly stated above, no rights or licenses from any copyright
 * holder or contributor is granted under this license, whether expressly, by
 * implication, estoppel or otherwise.
 *
 * DISCLAIMER
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS
 * IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * SPDX-License-Identifier: BSD-2-Clause-Patent
 */

#include "config.h"

#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <unistd.h>

#ifdef _WIN32
# include <process.h>
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif

#include "sysprof-capture-util-private.h"
#include "sysprof-macros.h"

#ifdef _WIN32
static void *_sysprof_io_sync_lock = SRWLOCK_INIT;
#endif

size_t
(_sysprof_getpagesize) (void)
{
  static size_t pgsz = 0;

  if SYSPROF_UNLIKELY (pgsz == 0)
    {
#ifdef _WIN32
      SYSTEM_INFO system_info;
      GetSystemInfo (&system_info);
      pgsz = system_info.dwPageSize;
#else
      pgsz = sysconf (_SC_PAGESIZE);
#endif
    }

  return pgsz;
}

ssize_t
(_sysprof_pread) (int     fd,
                  void   *buf,
                  size_t  count,
                  off_t   offset)
{
#ifdef _WIN32
  ssize_t ret = -1;

  AcquireSRWLockExclusive (_sysprof_io_sync_lock);
  errno = 0;
  if (lseek (fd, offset, SEEK_SET) != -1)
    ret = read (fd, buf, count);
  ReleaseSRWLockExclusive (_sysprof_io_sync_lock);

  return ret;
#else
  errno = 0;
  return pread (fd, buf, count, offset);
#endif
}

ssize_t
(_sysprof_pwrite) (int         fd,
                   const void *buf,
                   size_t      count,
                   off_t       offset)
{
#ifdef _WIN32
  ssize_t ret = -1;

  AcquireSRWLockExclusive (_sysprof_io_sync_lock);
  errno = 0;
  if (lseek (fd, offset, SEEK_SET) != -1)
    ret = write (fd, buf, count);
  ReleaseSRWLockExclusive (_sysprof_io_sync_lock);

  return ret;
#else
  errno = 0;
  return pwrite (fd, buf, count, offset);
#endif
}

ssize_t
(_sysprof_write) (int         fd,
                  const void *buf,
                  size_t      count)
{
#ifdef _WIN32
  ssize_t ret = -1;

  AcquireSRWLockExclusive (_sysprof_io_sync_lock);
  errno = 0;
  ret = write (fd, buf, count);
  ReleaseSRWLockExclusive (_sysprof_io_sync_lock);

  return ret;
#else
  errno = 0;
  return write (fd, buf, count);
#endif
}

int32_t
(_sysprof_getpid) (void)
{
#ifdef _WIN32
  return _getpid ();
#else
  return getpid ();
#endif
}

ssize_t
(_sysprof_sendfile) (int     out_fd,
                     int     in_fd,
                     off_t  *offset,
                     size_t  count)
{
  ssize_t total = 0;
  off_t wpos = 0;
  off_t rpos = 0;

  errno = 0;

  if (offset != NULL && *offset > 0)
    wpos = rpos = *offset;

  while (count > 0)
    {
      unsigned char buf[4096*4];
      ssize_t n_written = 0;
      ssize_t n_read;
      off_t off = 0;
      size_t to_read;

      /* Try to page align */
      if ((rpos % 4096) != 0)
        to_read = 4096 - rpos;
      else
        to_read = sizeof buf;

      if (to_read > count)
        to_read = count;

      errno = 0;
      n_read = _sysprof_pread (in_fd, buf, to_read, rpos);

      if (n_read <= 0)
        return -1;

      assert (count >= n_read);

      count -= n_read;
      rpos += n_read;

      while (wpos < rpos)
        {
          assert (off < sizeof buf);

          errno = 0;
          n_written = write (out_fd, &buf[off], rpos - wpos);

          if (n_written <= 0)
            return -1;

          wpos += n_written;
          off += n_written;
          total += n_written;
        }
    }

  assert (count == 0);

  if (offset != NULL)
    *offset = rpos;

  errno = 0;
  return total;
}

size_t
(_sysprof_strlcpy) (char       *dest,
                    const char *src,
                    size_t      dest_size)
{
  size_t i = 0;

  if (dest_size > 0)
    {
      for (; i < dest_size - 1 && src[i] != '\0'; i++)
        dest[i] = src[i];
      dest[i] = '\0';
    }

  for (; src[i] != '\0'; i++)
    ;

  return i;
}

void *
(_sysprof_reallocarray) (void   *ptr,
                         size_t  m,
                         size_t  n)
{
       if (n && m > (size_t)(-1) / n) {
               errno = ENOMEM;
               return NULL;
       }
       return realloc(ptr, m * n);
}
