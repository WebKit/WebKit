/* sysprof-collector.c
 *
 * Copyright 2020 Christian Hergert <chergert@redhat.com>
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
#include <poll.h>
#include <pthread.h>
#ifdef __linux__
# include <sched.h>
#endif
#include <stdarg.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

#include "mapped-ring-buffer.h"

#include "sysprof-capture-util-private.h"
#include "sysprof-collector.h"
#include "sysprof-macros-internal.h"

#define MAX_UNWIND_DEPTH 128
#define CREATRING      "CreatRing\0"
#define CREATRING_LEN  10

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

#ifndef MSG_CMSG_CLOEXEC
#define MSG_CMSG_CLOEXEC 0
#endif

typedef struct
{
  MappedRingBuffer *buffer;
  bool is_shared;
  int tid;
  int pid;
  int next_counter_id;
} SysprofCollector;

#define COLLECTOR_INVALID ((void *)&invalid)

static MappedRingBuffer       *request_writer         (void);
static void                    sysprof_collector_free (void *data);
static const SysprofCollector *sysprof_collector_get  (void);

static pthread_mutex_t control_fd_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_key_t collector_key;  /* initialised in sysprof_collector_init() */
static pthread_key_t single_trace_key;  /* initialised in sysprof_collector_init() */
static pthread_once_t collector_init = PTHREAD_ONCE_INIT;
static SysprofCollector *shared_collector;
static SysprofCollector invalid;

static inline bool
use_single_trace (void)
{
  return (bool) pthread_getspecific (single_trace_key);
}

static inline int
_do_getcpu (void)
{
#ifdef __linux__
  return sched_getcpu ();
#else
  return -1;
#endif
}

static inline size_t
realign (size_t size)
{
  return (size + SYSPROF_CAPTURE_ALIGN - 1) & ~(SYSPROF_CAPTURE_ALIGN - 1);
}

static bool
set_fd_blocking (int fd)
{
#ifdef F_GETFL
  long fcntl_flags;
  fcntl_flags = fcntl (peer_fd, F_GETFL);

  if (fcntl_flags == -1)
    return false;

#ifdef O_NONBLOCK
  fcntl_flags &= ~O_NONBLOCK;
#else
  fcntl_flags &= ~O_NDELAY;
#endif

  if (fcntl (peer_fd, F_SETFL, fcntl_flags) == -1)
    return false;

  return true;
#else
  return false;
#endif
}

static bool
block_on_poll (int fd,
               int condition)
{
  struct pollfd poll_fd;

  poll_fd.fd = fd;
  poll_fd.events = condition;

  return (TEMP_FAILURE_RETRY (poll (&poll_fd, 1, -1)) == 1);
}

static ssize_t
send_blocking (int            fd,
               const uint8_t *buffer,
               size_t         buffer_len)
{
  ssize_t res;

  while ((res = TEMP_FAILURE_RETRY (send (fd, buffer, buffer_len, MSG_NOSIGNAL))) < 0)
    {
      int errsv = errno;

      if (errsv == EWOULDBLOCK ||
          errsv == EAGAIN)
        {
          if (!block_on_poll (fd, POLLOUT))
            return -1;
        }
      else
        {
          return -1;
        }
    }

  return res;
}

static bool
send_all_blocking (int            fd,
                   const uint8_t *buffer,
                   size_t         buffer_len,
                   size_t        *bytes_written)
{
  size_t _bytes_written;

  _bytes_written = 0;
  while (_bytes_written < buffer_len)
    {
      ssize_t res = send_blocking (fd, buffer + _bytes_written, buffer_len - _bytes_written);
      if (res == -1)
        {
          if (bytes_written != NULL)
            *bytes_written = _bytes_written;
          return false;
        }
      assert (res > 0);

      _bytes_written += res;
    }

  if (bytes_written != NULL)
    *bytes_written = _bytes_written;
  return true;
}

static int
receive_fd_blocking (int peer_fd)
{
  ssize_t res;
  struct msghdr msg;
  struct iovec one_vector;
  char one_byte;
  uint8_t cmsg_buffer[CMSG_SPACE (sizeof (int))];
  struct cmsghdr *cmsg;
  const int *fds = NULL;
  size_t n_fds = 0;

  one_vector.iov_base = &one_byte;
  one_vector.iov_len = 1;

  msg.msg_name = NULL;
  msg.msg_namelen = 0;
  msg.msg_iov = &one_vector;
  msg.msg_iovlen = 1;
  msg.msg_flags = MSG_CMSG_CLOEXEC;
  msg.msg_control = &cmsg_buffer;
  msg.msg_controllen = sizeof (cmsg_buffer);

  while ((res = TEMP_FAILURE_RETRY (recvmsg (peer_fd, &msg, msg.msg_flags))) < 0)
    {
      int errsv = errno;

      if (errsv == EWOULDBLOCK ||
          errsv == EAGAIN)
        {
          if (!block_on_poll (peer_fd, POLLIN))
            return -1;
        }
      else
        {
          return -1;
        }
    }

  /* Decode the returned control message */
  cmsg = CMSG_FIRSTHDR (&msg);
  if (cmsg == NULL)
    return -1;

  if (cmsg->cmsg_level != SOL_SOCKET ||
      cmsg->cmsg_type != SCM_RIGHTS)
    return -1;

  /* non-integer number of FDs */
  if ((cmsg->cmsg_len - ((char *)CMSG_DATA (cmsg) - (char *)cmsg)) % 4 != 0)
    return -1;

  fds = (const int *)(void *)CMSG_DATA (cmsg);
  n_fds = (cmsg->cmsg_len - ((char *)CMSG_DATA (cmsg) - (char *)cmsg)) / sizeof (*fds);

  /* only expecting one FD */
  if (n_fds != 1)
    goto close_fds_err;

  for (size_t i = 0; i < n_fds; i++)
    {
      if (fds[i] < 0)
        goto close_fds_err;
    }

  /* only expecting one control message */
  cmsg = CMSG_NXTHDR (&msg, cmsg);
  if (cmsg != NULL)
    goto close_fds_err;

  return fds[0];

close_fds_err:
  for (size_t i = 0; i < n_fds; i++)
    close (fds[i]);

  return -1;
}

/* Called with @control_fd_lock held. */
static MappedRingBuffer *
request_writer (void)
{
  static int peer_fd = -1;
  MappedRingBuffer *buffer = NULL;

  if (peer_fd == -1)
    {
      const char *fdstr = getenv ("SYSPROF_CONTROL_FD");

      if (fdstr != NULL)
        peer_fd = atoi (fdstr);

      if (peer_fd > 0)
        {
          (void) set_fd_blocking (peer_fd);

#ifdef SO_NOSIGPIPE
          {
            int opt_value = 1;
            (void) setsockopt (peer_fd, SOL_SOCKET, SO_NOSIGPIPE, &opt_value, sizeof (opt_value));
          }
#endif
        }
    }

  if (peer_fd >= 0)
    {
      if (send_all_blocking (peer_fd, (const uint8_t *) CREATRING, CREATRING_LEN, NULL))
        {
          int ring_fd = receive_fd_blocking (peer_fd);

          if (ring_fd >= 0)
            {
              buffer = mapped_ring_buffer_new_writer (ring_fd);
              close (ring_fd);
            }
        }
    }

  return sysprof_steal_pointer (&buffer);
}

static void
write_final_frame (MappedRingBuffer *ring)
{
  SysprofCaptureFrame *fr;

  assert (ring != NULL);

  if ((fr = mapped_ring_buffer_allocate (ring, sizeof *fr)))
    {
      fr->len = sizeof *fr; /* aligned */
      fr->type = 0xFF;      /* Invalid */
      fr->cpu = -1;
      fr->pid = -1;
      fr->time = SYSPROF_CAPTURE_CURRENT_TIME;
      mapped_ring_buffer_advance (ring, fr->len);
    }
}

static void
sysprof_collector_free (void *data)
{
  SysprofCollector *collector = data;

  if (collector != NULL && collector != COLLECTOR_INVALID)
    {
      MappedRingBuffer *buffer = sysprof_steal_pointer (&collector->buffer);

      if (buffer != NULL)
        {
          write_final_frame (buffer);
          mapped_ring_buffer_unref (buffer);
        }

      free (collector);
    }
}

static const SysprofCollector *
sysprof_collector_get (void)
{
  const SysprofCollector *collector;

  sysprof_collector_init ();
  collector = pthread_getspecific (collector_key);

  /* We might have gotten here recursively */
  if SYSPROF_UNLIKELY (collector == COLLECTOR_INVALID)
    return COLLECTOR_INVALID;

  if SYSPROF_LIKELY (collector != NULL)
    return collector;

  if (use_single_trace () && shared_collector != COLLECTOR_INVALID)
    return shared_collector;

  {
    SysprofCollector *self, *old_collector;

    /* First set the collector to invalid so anything recursive
     * here fails instead of becoming re-entrant.
     */
    pthread_setspecific (collector_key, COLLECTOR_INVALID);

    /* Now we can malloc without ending up here again */
    self = sysprof_malloc0 (sizeof (SysprofCollector));
    if (self == NULL)
      return COLLECTOR_INVALID;

    self->pid = getpid ();
#ifdef __linux__
    self->tid = syscall (__NR_gettid, 0);
#else
    self->tid = self->pid;
#endif
    self->next_counter_id = 1;

    pthread_mutex_lock (&control_fd_lock);

    if (getenv ("SYSPROF_CONTROL_FD") != NULL)
      self->buffer = request_writer ();

    /* Update the stored collector */
    old_collector = pthread_getspecific (collector_key);

    if (self->is_shared)
      {
        if (pthread_setspecific (collector_key, COLLECTOR_INVALID) != 0)
          goto fail;
        sysprof_collector_free (old_collector);
        shared_collector = self;
      }
    else
      {
        if (pthread_setspecific (collector_key, self) != 0)
          goto fail;
        sysprof_collector_free (old_collector);
      }

    pthread_mutex_unlock (&control_fd_lock);

    return self;

fail:
    pthread_mutex_unlock (&control_fd_lock);
    sysprof_collector_free (self);

    return COLLECTOR_INVALID;
  }
}

static void
collector_init_cb (void)
{
  if (SYSPROF_UNLIKELY (pthread_key_create (&collector_key, sysprof_collector_free) != 0))
    abort ();
  if (SYSPROF_UNLIKELY (pthread_key_create (&single_trace_key, NULL) != 0))
    abort ();

  sysprof_clock_init ();
}

void
sysprof_collector_init (void)
{
  if SYSPROF_UNLIKELY (pthread_once (&collector_init, collector_init_cb) != 0)
    abort ();
}

#define COLLECTOR_BEGIN                                           \
  do {                                                  \
    const SysprofCollector *collector = sysprof_collector_get (); \
    if SYSPROF_LIKELY (collector->buffer)                         \
      {                                                           \
        if SYSPROF_UNLIKELY (collector->is_shared)                \
          pthread_mutex_lock (&control_fd_lock);                  \
                                                                  \
        {

#define COLLECTOR_END                                             \
        }                                                         \
                                                                  \
        if SYSPROF_UNLIKELY (collector->is_shared)                \
          pthread_mutex_unlock (&control_fd_lock);                \
      }                                                           \
  } while (0)

void
sysprof_collector_allocate (SysprofCaptureAddress   alloc_addr,
                            int64_t                 alloc_size,
                            SysprofBacktraceFunc    backtrace_func,
                            void                   *backtrace_data)
{
  COLLECTOR_BEGIN {
    SysprofCaptureAllocation *ev;
    size_t len;

    len = sizeof *ev + (sizeof (SysprofCaptureAllocation) * MAX_UNWIND_DEPTH);

    if ((ev = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        int n_addrs;

        /* First take a backtrace, so that backtrace_func() can overwrite
         * a little bit of data *BEFORE* ev->addrs as stratch space. This
         * is useful to allow using unw_backtrace() or backtrace() to skip
         * a small number of frames.
         *
         * We fill in all the other data afterwards which overwrites that
         * scratch space anyway.
         */
        if (backtrace_func)
          n_addrs = backtrace_func (ev->addrs, MAX_UNWIND_DEPTH, backtrace_data);
        else
          n_addrs = 0;

        ev->n_addrs = ((n_addrs < 0) ? 0 : (n_addrs > MAX_UNWIND_DEPTH) ? MAX_UNWIND_DEPTH : n_addrs);
        ev->frame.len = sizeof *ev + sizeof (SysprofCaptureAddress) * ev->n_addrs;
        ev->frame.type = SYSPROF_CAPTURE_FRAME_ALLOCATION;
        ev->frame.cpu = _do_getcpu ();
        ev->frame.pid = collector->pid;
        ev->frame.time = SYSPROF_CAPTURE_CURRENT_TIME;
        ev->tid = collector->tid;
        ev->alloc_addr = alloc_addr;
        ev->alloc_size = alloc_size;
        ev->padding1 = 0;

        mapped_ring_buffer_advance (collector->buffer, ev->frame.len);
      }

  } COLLECTOR_END;
}

void
sysprof_collector_sample (SysprofBacktraceFunc  backtrace_func,
                          void                 *backtrace_data)
{
  COLLECTOR_BEGIN {
    SysprofCaptureSample *ev;
    size_t len;

    len = sizeof *ev + (sizeof (SysprofCaptureSample) * MAX_UNWIND_DEPTH);

    if ((ev = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        int n_addrs;

        /* See comment from sysprof_collector_allocate(). */
        if (backtrace_func)
          n_addrs = backtrace_func (ev->addrs, MAX_UNWIND_DEPTH, backtrace_data);
        else
          n_addrs = 0;

        ev->n_addrs = ((n_addrs < 0) ? 0 : (n_addrs > MAX_UNWIND_DEPTH) ? MAX_UNWIND_DEPTH : n_addrs);
        ev->frame.len = sizeof *ev + sizeof (SysprofCaptureAddress) * ev->n_addrs;
        ev->frame.type = SYSPROF_CAPTURE_FRAME_SAMPLE;
        ev->frame.cpu = _do_getcpu ();
        ev->frame.pid = collector->pid;
        ev->frame.time = SYSPROF_CAPTURE_CURRENT_TIME;
        ev->tid = collector->tid;
        ev->padding1 = 0;

        mapped_ring_buffer_advance (collector->buffer, ev->frame.len);
      }

  } COLLECTOR_END;
}

void
sysprof_collector_trace (SysprofBacktraceFunc  backtrace_func,
                         void                 *backtrace_data,
                         bool                  entering)
{
  COLLECTOR_BEGIN {
    SysprofCaptureTrace *ev;
    size_t len;

    len = sizeof *ev + (sizeof (SysprofCaptureTrace) * MAX_UNWIND_DEPTH);

    if ((ev = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        int n_addrs;

        /* See comment from sysprof_collector_allocate(). */
        if (backtrace_func)
          n_addrs = backtrace_func (ev->addrs, MAX_UNWIND_DEPTH, backtrace_data);
        else
          n_addrs = 0;

        ev->n_addrs = ((n_addrs < 0) ? 0 : (n_addrs > MAX_UNWIND_DEPTH) ? MAX_UNWIND_DEPTH : n_addrs);
        ev->frame.len = sizeof *ev + sizeof (SysprofCaptureAddress) * ev->n_addrs;
        ev->frame.type = SYSPROF_CAPTURE_FRAME_TRACE;
        ev->frame.cpu = _do_getcpu ();
        ev->frame.pid = collector->pid;
        ev->frame.time = SYSPROF_CAPTURE_CURRENT_TIME;
        ev->tid = collector->tid;
        ev->entering = !!entering;
        ev->padding1 = 0;

        mapped_ring_buffer_advance (collector->buffer, ev->frame.len);
      }

  } COLLECTOR_END;
}

void
sysprof_collector_mark (int64_t     time,
                        int64_t     duration,
                        const char *group,
                        const char *mark,
                        const char *message)
{
  COLLECTOR_BEGIN {
    SysprofCaptureMark *ev;
    size_t len;
    size_t sl;

    if (group == NULL)
      group = "";

    if (mark == NULL)
      mark = "";

    if (message == NULL)
      message = "";

    sl = strlen (message);
    len = realign (sizeof *ev + sl + 1);

    if ((ev = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        ev->frame.len = len;
        ev->frame.type = SYSPROF_CAPTURE_FRAME_MARK;
        ev->frame.cpu = _do_getcpu ();
        ev->frame.pid = collector->pid;
        ev->frame.time = time;
        ev->duration = duration;
        _sysprof_strlcpy (ev->group, group, sizeof ev->group);
        _sysprof_strlcpy (ev->name, mark, sizeof ev->name);
        memcpy (ev->message, message, sl);
        ev->message[sl] = 0;

        mapped_ring_buffer_advance (collector->buffer, ev->frame.len);
      }

  } COLLECTOR_END;
}

void
sysprof_collector_mark_printf (int64_t     time,
                               int64_t     duration,
                               const char *group,
                               const char *mark,
                               const char *message_format,
                               ...)
{
  va_list args;

  va_start (args, message_format);
  sysprof_collector_mark_vprintf (time, duration, group, mark, message_format, args);
  va_end (args);
}

void
sysprof_collector_mark_vprintf (int64_t     time,
                                int64_t     duration,
                                const char *group,
                                const char *mark,
                                const char *message_format,
                                va_list     args)
{
  COLLECTOR_BEGIN {
    SysprofCaptureMark *ev;
    size_t len;
    size_t sl;
    va_list args2;

    /* Need to take a copy of @args since we iterate through it twice, once to
     * work out the formatted string length, and once to format it. */
    va_copy (args2, args);

    if (group == NULL)
      group = "";

    if (mark == NULL)
      mark = "";

    if (message_format == NULL)
      message_format = "";

    /* Work out the formatted message length */
    sl = vsnprintf (NULL, 0, message_format, args);

    len = realign (sizeof *ev + sl + 1);

    if ((ev = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        ev->frame.len = len;
        ev->frame.type = SYSPROF_CAPTURE_FRAME_MARK;
        ev->frame.cpu = _do_getcpu ();
        ev->frame.pid = collector->pid;
        ev->frame.time = time;
        ev->duration = duration;
        _sysprof_strlcpy (ev->group, group, sizeof ev->group);
        _sysprof_strlcpy (ev->name, mark, sizeof ev->name);
        vsnprintf (ev->message, sl + 1, message_format, args2);
        ev->message[sl] = 0;

        mapped_ring_buffer_advance (collector->buffer, ev->frame.len);
      }

    va_end (args2);
  } COLLECTOR_END;
}

void
sysprof_collector_log (int             severity,
                       const char     *domain,
                       const char     *message)
{
  COLLECTOR_BEGIN {
    SysprofCaptureLog *ev;
    size_t len;
    size_t sl;

    if (domain == NULL)
      domain = "";

    if (message == NULL)
      message = "";

    sl = strlen (message);
    len = realign (sizeof *ev + sl + 1);

    if ((ev = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        ev->frame.len = len;
        ev->frame.type = SYSPROF_CAPTURE_FRAME_LOG;
        ev->frame.cpu = _do_getcpu ();
        ev->frame.pid = collector->pid;
        ev->frame.time = SYSPROF_CAPTURE_CURRENT_TIME;
        ev->severity = severity & 0xFFFF;
        ev->padding1 = 0;
        ev->padding2 = 0;
        _sysprof_strlcpy (ev->domain, domain, sizeof ev->domain);
        memcpy (ev->message, message, sl);
        ev->message[sl] = 0;

        mapped_ring_buffer_advance (collector->buffer, ev->frame.len);
      }

  } COLLECTOR_END;
}

void
sysprof_collector_log_printf (int             severity,
                              const char     *domain,
                              const char     *format,
                              ...)
{
  COLLECTOR_BEGIN {
    char formatted[2048];
    SysprofCaptureLog *ev;
    va_list args;
    size_t len;
    size_t sl;

    va_start (args, format);
    vsnprintf (formatted, sizeof (formatted), format, args);
    va_end (args);

    if (domain == NULL)
      domain = "";

    sl = strlen (formatted);
    len = realign (sizeof *ev + sl + 1);

    if ((ev = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        ev->frame.len = len;
        ev->frame.type = SYSPROF_CAPTURE_FRAME_LOG;
        ev->frame.cpu = _do_getcpu ();
        ev->frame.pid = collector->pid;
        ev->frame.time = SYSPROF_CAPTURE_CURRENT_TIME;
        ev->severity = severity & 0xFFFF;
        ev->padding1 = 0;
        ev->padding2 = 0;
        _sysprof_strlcpy (ev->domain, domain, sizeof ev->domain);
        memcpy (ev->message, formatted, sl);
        ev->message[sl] = 0;

        mapped_ring_buffer_advance (collector->buffer, ev->frame.len);
      }
  } COLLECTOR_END;
}

void
sysprof_collector_define_counters (const SysprofCaptureCounter *counters,
                                   unsigned int                 n_counters)
{
  if (counters == NULL || n_counters == 0)
    return;

  COLLECTOR_BEGIN {
    SysprofCaptureCounterDefine *def;
    size_t len;

    len = realign (sizeof *def + (sizeof *counters * n_counters));

    if ((def = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        def->frame.len = len;
        def->frame.type = SYSPROF_CAPTURE_FRAME_CTRDEF;
        def->frame.cpu = _do_getcpu ();
        def->frame.pid = collector->pid;
        def->frame.time = SYSPROF_CAPTURE_CURRENT_TIME;
        def->padding1 = 0;
        def->padding2 = 0;
        def->n_counters = n_counters;
        memcpy (def->counters, counters, sizeof *counters * n_counters);

        mapped_ring_buffer_advance (collector->buffer, def->frame.len);
      }
  } COLLECTOR_END;
}

void
sysprof_collector_set_counters (const unsigned int               *counters_ids,
                                const SysprofCaptureCounterValue *values,
                                unsigned int                      n_counters)
{
  if (n_counters == 0)
    return;

  COLLECTOR_BEGIN {
    SysprofCaptureCounterSet *set;
    size_t len;
    unsigned int n_groups;
    unsigned int group;
    unsigned int field;
    unsigned int i;

    /* Determine how many value groups we need */
    n_groups = n_counters / SYSPROF_N_ELEMENTS (set->values[0].values);
    if ((n_groups * SYSPROF_N_ELEMENTS (set->values[0].values)) != n_counters)
      n_groups++;

    len = realign (sizeof *set + (n_groups * sizeof (SysprofCaptureCounterValues)));

    if ((set = mapped_ring_buffer_allocate (collector->buffer, len)))
      {
        set->frame.len = len;
        set->frame.type = SYSPROF_CAPTURE_FRAME_CTRSET;
        set->frame.cpu = _do_getcpu ();
        set->frame.pid = collector->pid;
        set->frame.time = SYSPROF_CAPTURE_CURRENT_TIME;
        set->padding1 = 0;
        set->padding2 = 0;
        set->n_values = n_groups;

        for (i = 0, group = 0, field = 0; i < n_counters; i++)
          {
            set->values[group].ids[field] = counters_ids[i];
            set->values[group].values[field] = values[i];

            field++;

            if (field == SYSPROF_N_ELEMENTS (set->values[0].values))
              {
                field = 0;
                group++;
              }
          }

        mapped_ring_buffer_advance (collector->buffer, set->frame.len);
      }
  } COLLECTOR_END;
}

unsigned int
sysprof_collector_request_counters (unsigned int n_counters)
{
  unsigned int ret = 0;

  if (n_counters == 0)
    return 0;

  COLLECTOR_BEGIN {
    ret = collector->next_counter_id;
    ((SysprofCollector *)collector)->next_counter_id += n_counters;
  } COLLECTOR_END;

  return ret;
}

bool
sysprof_collector_is_active (void)
{
  bool ret = false;

  COLLECTOR_BEGIN {
    ret = true;
  } COLLECTOR_END;

  return ret;
}
