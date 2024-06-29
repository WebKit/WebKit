/* sysprof-capture-util-private.h
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

#pragma once

#ifdef __linux__
# include <sys/sendfile.h>
#endif

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression)            \
    ({ long int __result;                         \
       do { __result = (long int) (expression); } \
       while (__result == -1L && errno == EINTR); \
       __result; })
#endif

static inline void *
sysprof_malloc0 (size_t size)
{
  void *ptr = malloc (size);
  if (ptr == NULL)
    return NULL;
  memset (ptr, 0, size);
  return ptr;
}

#ifdef __linux__
# define _sysprof_getpagesize()     getpagesize()
# define _sysprof_pread(a,b,c,d)    pread(a,b,c,d)
# define _sysprof_pwrite(a,b,c,d)   pwrite(a,b,c,d)
# define _sysprof_write(a,b,c)      write(a,b,c)
# define _sysprof_getpid()          getpid()
# define _sysprof_sendfile(a,b,c,d) sendfile(a,b,c,d)
#else
size_t  _sysprof_getpagesize (void);
ssize_t _sysprof_pread       (int         fd,
                              void       *buf,
                              size_t      count,
                              off_t       offset);
ssize_t _sysprof_pwrite      (int         fd,
                              const void *buf,
                              size_t      count,
                              off_t       offset);
ssize_t _sysprof_write       (int         fd,
                              const void *buf,
                              size_t      count);
int32_t _sysprof_getpid      (void);
ssize_t _sysprof_sendfile    (int         out_fd,
                              int         in_fd,
                              off_t      *offset,
                              size_t      count);
#endif

#ifdef HAVE_STRLCPY
# define _sysprof_strlcpy(d,s,ds) strlcpy(d,s,ds)
#else
size_t  _sysprof_strlcpy     (char       *dest,
                              const char *src,
                              size_t      dest_size);
#endif

#ifdef HAVE_REALLOCARRAY
# define _sysprof_reallocarray(p,m,n) reallocarray(p,m,n)
#else
void *_sysprof_reallocarray  (void       *ptr,
                              size_t      m,
                              size_t      n);
#endif
