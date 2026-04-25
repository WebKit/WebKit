/* sysprof-capture-reader.c
 *
 * Copyright 2016-2019 Christian Hergert <chergert@redhat.com>
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
#include <fcntl.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "sysprof-capture-reader.h"
#include "sysprof-capture-util-private.h"
#include "sysprof-capture-writer.h"
#include "sysprof-macros-internal.h"

struct _SysprofCaptureReader
{
  volatile int              ref_count;
  char                     *filename;
  uint8_t                  *buf;
  size_t                    bufsz;
  size_t                    len;
  size_t                    pos;
  size_t                    fd_off;
  int                       fd;
  int                       endian;
  SysprofCaptureFileHeader  header;
  int64_t                   end_time;
  SysprofCaptureStat        st_buf;
  unsigned int              st_buf_set : 1;
  char                    **list_files;
  size_t                    n_list_files;
};

/* Sets @errno on failure. Sets @errno to EBADMSG if the file magic doesn’t
 * match, and otherwise can return any error as for read(). */
static bool
sysprof_capture_reader_read_file_header (SysprofCaptureReader     *self,
                                         SysprofCaptureFileHeader *header)
{
  assert (self != NULL);
  assert (header != NULL);

  if (sizeof *header != _sysprof_pread (self->fd, header, sizeof *header, 0L))
    {
      /* errno is propagated */
      return false;
    }

  if (header->magic != SYSPROF_CAPTURE_MAGIC)
    {
      errno = EBADMSG;
      return false;
    }

  header->capture_time[sizeof header->capture_time - 1] = '\0';

  return true;
}

static void
sysprof_capture_reader_finalize (SysprofCaptureReader *self)
{
  if (self != NULL)
    {
      for (size_t i = 0; i < self->n_list_files; i++)
        free (self->list_files[i]);
      free (self->list_files);

      close (self->fd);
      free (self->buf);
      free (self->filename);
      free (self);
    }
}

const char *
sysprof_capture_reader_get_time (SysprofCaptureReader *self)
{
  assert (self != NULL);

  return self->header.capture_time;
}

const char *
sysprof_capture_reader_get_filename (SysprofCaptureReader *self)
{
  assert (self != NULL);

  return self->filename;
}

static void
sysprof_capture_reader_discover_end_time (SysprofCaptureReader *self)
{
  SysprofCaptureFrame frame;

  assert (self != NULL);

  while (sysprof_capture_reader_peek_frame (self, &frame))
    {
      int64_t end_time = frame.time;

      switch (frame.type)
        {
        case SYSPROF_CAPTURE_FRAME_MARK: {
            const SysprofCaptureMark *mark = NULL;

            if ((mark = sysprof_capture_reader_read_mark (self)))
              end_time = frame.time + ((mark->duration > 0) ? mark->duration : 0);
          }
          break;

        case SYSPROF_CAPTURE_FRAME_ALLOCATION:
        case SYSPROF_CAPTURE_FRAME_CTRSET:
        case SYSPROF_CAPTURE_FRAME_EXIT:
        case SYSPROF_CAPTURE_FRAME_FORK:
        case SYSPROF_CAPTURE_FRAME_LOG:
        case SYSPROF_CAPTURE_FRAME_PROCESS:
        case SYSPROF_CAPTURE_FRAME_SAMPLE:
        case SYSPROF_CAPTURE_FRAME_TIMESTAMP:
          if (end_time > self->end_time)
            self->end_time = end_time;
          break;

        case SYSPROF_CAPTURE_FRAME_MAP:
        case SYSPROF_CAPTURE_FRAME_JITMAP:
        case SYSPROF_CAPTURE_FRAME_CTRDEF:
        case SYSPROF_CAPTURE_FRAME_METADATA:
        case SYSPROF_CAPTURE_FRAME_FILE_CHUNK:
        default:
          break;
        }

      if (!sysprof_capture_reader_skip (self))
        break;
    }

  sysprof_capture_reader_reset (self);
}

/**
 * sysprof_capture_reader_new_from_fd:
 * @fd: an fd to take ownership from
 *
 * Creates a new reader using the file-descriptor.
 *
 * This is useful if you don't necessarily have access to the filename itself.
 *
 * If this function fails, `errno` is set.
 *
 * Returns: (transfer full): an #SysprofCaptureReader or %NULL upon failure.
 */
SysprofCaptureReader *
sysprof_capture_reader_new_from_fd (int fd)
{
  SysprofCaptureReader *self;

  assert (fd > -1);

  self = sysprof_malloc0 (sizeof (SysprofCaptureReader));
  if (self == NULL)
    {
      errno = ENOMEM;
      return NULL;
    }

  self->ref_count = 1;
  self->bufsz = USHRT_MAX * 2;
  self->buf = sysprof_malloc0 (self->bufsz);
  if (self->buf == NULL)
    {
      free (self);
      errno = ENOMEM;
      return NULL;
    }

  self->len = 0;
  self->pos = 0;
  self->fd = fd;
  self->fd_off = sizeof (SysprofCaptureFileHeader);

  if (!sysprof_capture_reader_read_file_header (self, &self->header))
    {
      int errsv = errno;
      sysprof_capture_reader_finalize (self);
      errno = errsv;
      return NULL;
    }

  if (self->header.little_endian)
    self->endian = __LITTLE_ENDIAN;
  else
    self->endian = __BIG_ENDIAN;

  /* If we detect a capture file that did not get an end time, or an erroneous
   * end time, then we need to take a performance hit here and scan the file
   * and discover the end time with frame timings.
   */
  if (self->header.end_time < self->header.time)
    sysprof_capture_reader_discover_end_time (self);

  return self;
}

SysprofCaptureReader *
sysprof_capture_reader_new (const char *filename)
{
  SysprofCaptureReader *self;
  int fd;

  assert (filename != NULL);

  if (-1 == (fd = open (filename, O_RDONLY, 0)))
    {
      /* errno is propagated */
      return NULL;
    }

  if (NULL == (self = sysprof_capture_reader_new_from_fd (fd)))
    {
      int errsv = errno;
      close (fd);
      errno = errsv;
      return NULL;
    }

  self->filename = sysprof_strdup (filename);

  return self;
}

static inline void
sysprof_capture_reader_bswap_frame (SysprofCaptureReader *self,
                                    SysprofCaptureFrame  *frame)
{
  assert (self != NULL);
  assert (frame!= NULL);

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      frame->len = bswap_16 (frame->len);
      frame->cpu = bswap_16 (frame->cpu);
      frame->pid = bswap_32 (frame->pid);
      frame->time = bswap_64 (frame->time);
    }
}

static inline void
sysprof_capture_reader_bswap_file_chunk (SysprofCaptureReader    *self,
                                         SysprofCaptureFileChunk *file_chunk)
{
  assert (self != NULL);
  assert (file_chunk != NULL);

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    file_chunk->len = bswap_16 (file_chunk->len);
}

static inline void
sysprof_capture_reader_bswap_log (SysprofCaptureReader *self,
                                  SysprofCaptureLog    *log)
{
  assert (self != NULL);
  assert (log != NULL);

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    log->severity = bswap_16 (log->severity);
}

static inline void
sysprof_capture_reader_bswap_map (SysprofCaptureReader *self,
                                  SysprofCaptureMap    *map)
{
  assert (self != NULL);
  assert (map != NULL);

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      map->start = bswap_64 (map->start);
      map->end = bswap_64 (map->end);
      map->offset = bswap_64 (map->offset);
      map->inode = bswap_64 (map->inode);
    }
}

static inline void
sysprof_capture_reader_bswap_mark (SysprofCaptureReader *self,
                                   SysprofCaptureMark   *mark)
{
  assert (self != NULL);
  assert (mark != NULL);

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    mark->duration = bswap_64 (mark->duration);
}

static inline void
sysprof_capture_reader_bswap_overlay (SysprofCaptureReader  *self,
                                       SysprofCaptureOverlay *pr)
{
  assert (self != NULL);
  assert (pr != NULL);

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      pr->layer = bswap_32 (pr->layer);
      pr->src_len = bswap_32 (pr->src_len);
      pr->dst_len = bswap_32 (pr->dst_len);
    }
}

static inline void
sysprof_capture_reader_bswap_jitmap (SysprofCaptureReader *self,
                                     SysprofCaptureJitmap *jitmap)
{
  assert (self != NULL);
  assert (jitmap != NULL);

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    jitmap->n_jitmaps = bswap_64 (jitmap->n_jitmaps);
}

static bool
sysprof_capture_reader_ensure_space_for (SysprofCaptureReader *self,
                                         size_t                len)
{
  assert (self != NULL);
  assert (self->pos <= self->len);
  assert (len > 0);

  /* Ensure alignment of length to read */
  len = (len + SYSPROF_CAPTURE_ALIGN - 1) & ~(SYSPROF_CAPTURE_ALIGN - 1);

  if ((self->len - self->pos) < len)
    {
      ssize_t r;

      if (self->len > self->pos)
        memmove (self->buf, &self->buf[self->pos], self->len - self->pos);
      self->len -= self->pos;
      self->pos = 0;

      while (self->len < len)
        {
          assert ((self->pos + self->len) < self->bufsz);
          assert (self->len < self->bufsz);

          /* Read into our buffer after our current read position */
          r = _sysprof_pread (self->fd,
                              &self->buf[self->len],
                              self->bufsz - self->len,
                              self->fd_off);

          if (r <= 0)
            break;

          self->fd_off += r;
          self->len += r;
        }
    }

  return (self->len - self->pos) >= len;
}

bool
sysprof_capture_reader_skip (SysprofCaptureReader *self)
{
  SysprofCaptureFrame *frame;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof (SysprofCaptureFrame)))
    return false;

  frame = (SysprofCaptureFrame *)(void *)&self->buf[self->pos];
  sysprof_capture_reader_bswap_frame (self, frame);

  if (frame->len < sizeof (SysprofCaptureFrame))
    return false;

  if (!sysprof_capture_reader_ensure_space_for (self, frame->len))
    return false;

  frame = (SysprofCaptureFrame *)(void *)&self->buf[self->pos];

  self->pos += frame->len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return false;

  return true;
}

bool
sysprof_capture_reader_peek_frame (SysprofCaptureReader *self,
                                   SysprofCaptureFrame  *frame)
{
  SysprofCaptureFrame *real_frame;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->len);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *real_frame))
    return false;

  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);

  real_frame = (SysprofCaptureFrame *)(void *)&self->buf[self->pos];

  *frame = *real_frame;

  sysprof_capture_reader_bswap_frame (self, frame);

  /* In case the capture did not update the end_time during normal usage,
   * we can update our cached known end_time based on the greatest frame
   * we come across.
   */
  if (frame->time > self->end_time)
    self->end_time = frame->time;

  return frame->type > 0 && frame->type < SYSPROF_CAPTURE_FRAME_LAST;
}

bool
sysprof_capture_reader_peek_type (SysprofCaptureReader    *self,
                                  SysprofCaptureFrameType *type)
{
  SysprofCaptureFrame frame;

  assert (self != NULL);
  assert (type != NULL);

  if (!sysprof_capture_reader_peek_frame (self, &frame))
    return false;

  *type = frame.type;

  return frame.type > 0 && frame.type < SYSPROF_CAPTURE_FRAME_LAST;
}

static const SysprofCaptureFrame *
sysprof_capture_reader_read_basic (SysprofCaptureReader    *self,
                                   SysprofCaptureFrameType  type,
                                   size_t                   extra)
{
  SysprofCaptureFrame *frame;
  size_t len = sizeof *frame + extra;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, len))
    return NULL;

  frame = (SysprofCaptureFrame *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, frame);

  if (frame->len < len)
    return NULL;

  if (frame->type != type)
    return NULL;

  if (frame->len > (self->len - self->pos))
    return NULL;

  self->pos += frame->len;

  return frame;
}

const SysprofCaptureTimestamp *
sysprof_capture_reader_read_timestamp (SysprofCaptureReader *self)
{
  return (SysprofCaptureTimestamp *)
    sysprof_capture_reader_read_basic (self, SYSPROF_CAPTURE_FRAME_TIMESTAMP, 0);
}

const SysprofCaptureExit *
sysprof_capture_reader_read_exit (SysprofCaptureReader *self)
{
  return (SysprofCaptureExit *)
    sysprof_capture_reader_read_basic (self, SYSPROF_CAPTURE_FRAME_EXIT, 0);
}

const SysprofCaptureFork *
sysprof_capture_reader_read_fork (SysprofCaptureReader *self)
{
  SysprofCaptureFork *fk;

  assert (self != NULL);

  fk = (SysprofCaptureFork *)
    sysprof_capture_reader_read_basic (self, SYSPROF_CAPTURE_FRAME_FORK, sizeof (uint32_t));

  if (fk != NULL)
    {
      if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
        fk->child_pid = bswap_32 (fk->child_pid);
    }

  return fk;
}

const SysprofCaptureMap *
sysprof_capture_reader_read_map (SysprofCaptureReader *self)
{
  SysprofCaptureMap *map;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *map))
    return NULL;

  map = (SysprofCaptureMap *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &map->frame);

  if (map->frame.type != SYSPROF_CAPTURE_FRAME_MAP)
    return NULL;

  if (map->frame.len < (sizeof *map + 1))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, map->frame.len))
    return NULL;

  map = (SysprofCaptureMap *)(void *)&self->buf[self->pos];

  if (self->buf[self->pos + map->frame.len - 1] != '\0')
    return NULL;

  sysprof_capture_reader_bswap_map (self, map);

  self->pos += map->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  return map;
}

const SysprofCaptureLog *
sysprof_capture_reader_read_log (SysprofCaptureReader *self)
{
  SysprofCaptureLog *log;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *log))
    return NULL;

  log = (SysprofCaptureLog *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &log->frame);

  if (log->frame.type != SYSPROF_CAPTURE_FRAME_LOG)
    return NULL;

  if (log->frame.len < (sizeof *log + 1))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, log->frame.len))
    return NULL;

  log = (SysprofCaptureLog *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_log (self, log);

  self->pos += log->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  /* Ensure trailing \0 in domain and message */
  log->domain[sizeof log->domain - 1] = 0;
  if (log->frame.len > sizeof *log)
    ((char *)log)[log->frame.len - 1] = 0;

  return log;
}

const SysprofCaptureMark *
sysprof_capture_reader_read_mark (SysprofCaptureReader *self)
{
  SysprofCaptureMark *mark;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *mark))
    return NULL;

  mark = (SysprofCaptureMark *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &mark->frame);

  if (mark->frame.type != SYSPROF_CAPTURE_FRAME_MARK)
    return NULL;

  if (mark->frame.len < (sizeof *mark + 1))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, mark->frame.len))
    return NULL;

  mark = (SysprofCaptureMark *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_mark (self, mark);

  self->pos += mark->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  /* Ensure trailing \0 in name and message */
  mark->name[sizeof mark->name - 1] = 0;
  if (mark->frame.len > sizeof *mark)
    ((char *)mark)[mark->frame.len - 1] = 0;

  /* Maybe update end-time */
  if SYSPROF_UNLIKELY ((mark->frame.time + mark->duration) > self->end_time)
    self->end_time = mark->frame.time + mark->duration;

  return mark;
}

const SysprofCaptureOverlay *
sysprof_capture_reader_read_overlay (SysprofCaptureReader *self)
{
  SysprofCaptureOverlay *pr;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *pr + 1))
    return NULL;

  pr = (SysprofCaptureOverlay *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &pr->frame);

  if (pr->frame.type != SYSPROF_CAPTURE_FRAME_OVERLAY)
    return NULL;

  if (pr->frame.len < (sizeof *pr + 2))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, pr->frame.len))
    return NULL;

  pr = (SysprofCaptureOverlay *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_overlay (self, pr);

  /* Make sure there is enough space for paths and trailing \0 */
  if (((size_t)pr->src_len + (size_t)pr->dst_len) > (pr->frame.len - sizeof *pr - 2))
    return NULL;

  /* Enforce trailing \0 */
  pr->data[pr->src_len] = 0;
  pr->data[pr->src_len+1+pr->dst_len] = 0;

  self->pos += pr->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  /* Ensure trailing \0 in name and message */
  ((char *)pr)[pr->frame.len-1] = 0;

  return pr;
}

const SysprofCaptureMetadata *
sysprof_capture_reader_read_metadata (SysprofCaptureReader *self)
{
  SysprofCaptureMetadata *metadata;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *metadata))
    return NULL;

  metadata = (SysprofCaptureMetadata *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &metadata->frame);

  if (metadata->frame.type != SYSPROF_CAPTURE_FRAME_METADATA)
    return NULL;

  if (metadata->frame.len < (sizeof *metadata + 1))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, metadata->frame.len))
    return NULL;

  metadata = (SysprofCaptureMetadata *)(void *)&self->buf[self->pos];

  self->pos += metadata->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  /* Ensure trailing \0 in .id and .metadata */
  metadata->id[sizeof metadata->id - 1] = 0;
  if (metadata->frame.len > sizeof *metadata)
    ((char *)metadata)[metadata->frame.len - 1] = 0;

  return metadata;
}

static inline void
sysprof_capture_reader_bswap_dbus_message (SysprofCaptureReader      *self,
                                           SysprofCaptureDBusMessage *dbus_message)
{
  assert (self != NULL);
  assert (dbus_message != NULL);

  /* This only swaps the frame, not the endianness of the message data
   * which is automatically handled by dbus libraries such as GDBus.
   */

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      dbus_message->flags = bswap_16 (dbus_message->flags);
      dbus_message->message_len = bswap_16 (dbus_message->message_len);
    }
}

const SysprofCaptureDBusMessage *
sysprof_capture_reader_read_dbus_message (SysprofCaptureReader *self)
{
  SysprofCaptureDBusMessage *dbus_message;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *dbus_message))
    return NULL;

  dbus_message = (SysprofCaptureDBusMessage *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &dbus_message->frame);

  if (dbus_message->frame.type != SYSPROF_CAPTURE_FRAME_DBUS_MESSAGE)
    return NULL;

  sysprof_capture_reader_bswap_dbus_message (self, dbus_message);

  if (dbus_message->frame.len < (sizeof *dbus_message + dbus_message->message_len))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, dbus_message->frame.len))
    return NULL;

  dbus_message = (SysprofCaptureDBusMessage *)(void *)&self->buf[self->pos];

  self->pos += dbus_message->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  return dbus_message;
}

const SysprofCaptureProcess *
sysprof_capture_reader_read_process (SysprofCaptureReader *self)
{
  SysprofCaptureProcess *process;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *process))
    return NULL;

  process = (SysprofCaptureProcess *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &process->frame);

  if (process->frame.type != SYSPROF_CAPTURE_FRAME_PROCESS)
    return NULL;

  if (process->frame.len < (sizeof *process + 1))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, process->frame.len))
    return NULL;

  process = (SysprofCaptureProcess *)(void *)&self->buf[self->pos];

  if (self->buf[self->pos + process->frame.len - 1] != '\0')
    return NULL;

  self->pos += process->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  return process;
}

const SysprofCaptureJitmap *
sysprof_capture_reader_read_jitmap (SysprofCaptureReader *self)
{
  SysprofCaptureJitmap *jitmap;
  uint8_t *buf;
  uint8_t *endptr;
  unsigned int i;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *jitmap))
    return NULL;

  jitmap = (SysprofCaptureJitmap *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &jitmap->frame);

  if (jitmap->frame.type != SYSPROF_CAPTURE_FRAME_JITMAP)
    return NULL;

  if (jitmap->frame.len < sizeof *jitmap)
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, jitmap->frame.len))
    return NULL;

  jitmap = (SysprofCaptureJitmap *)(void *)&self->buf[self->pos];

  buf = jitmap->data;
  endptr = &self->buf[self->pos + jitmap->frame.len];

  /* Check the strings are all nul-terminated. */
  for (i = 0; i < jitmap->n_jitmaps; i++)
    {
      SysprofCaptureAddress addr;

      if (buf + sizeof addr >= endptr)
        return NULL;

      memcpy (&addr, buf, sizeof addr);
      buf += sizeof addr;

      buf = memchr (buf, '\0', (endptr - buf));

      if (buf == NULL)
        return NULL;

      buf++;
    }

  sysprof_capture_reader_bswap_jitmap (self, jitmap);

  self->pos += jitmap->frame.len;

  return jitmap;
}

const SysprofCaptureSample *
sysprof_capture_reader_read_sample (SysprofCaptureReader *self)
{
  SysprofCaptureSample *sample;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *sample))
    return NULL;

  sample = (SysprofCaptureSample *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &sample->frame);

  if (sample->frame.type != SYSPROF_CAPTURE_FRAME_SAMPLE)
    return NULL;

  if (sample->frame.len < sizeof *sample)
    return NULL;

  if (self->endian != __BYTE_ORDER)
    sample->n_addrs = bswap_16 (sample->n_addrs);

  if (sample->frame.len < (sizeof *sample + (sizeof(SysprofCaptureAddress) * sample->n_addrs)))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, sample->frame.len))
    return NULL;

  sample = (SysprofCaptureSample *)(void *)&self->buf[self->pos];

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      unsigned int i;

      for (i = 0; i < sample->n_addrs; i++)
        sample->addrs[i] = bswap_64 (sample->addrs[i]);
    }

  self->pos += sample->frame.len;

  return sample;
}

const SysprofCaptureTrace *
sysprof_capture_reader_read_trace (SysprofCaptureReader *self)
{
  SysprofCaptureTrace *trace;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *trace))
    return NULL;

  trace = (SysprofCaptureTrace *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &trace->frame);

  if (trace->frame.type != SYSPROF_CAPTURE_FRAME_TRACE)
    return NULL;

  if (trace->frame.len < sizeof *trace)
    return NULL;

  if (self->endian != __BYTE_ORDER)
    trace->n_addrs = bswap_16 (trace->n_addrs);

  if (trace->frame.len < (sizeof *trace + (sizeof(SysprofCaptureAddress) * trace->n_addrs)))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, trace->frame.len))
    return NULL;

  trace = (SysprofCaptureTrace *)(void *)&self->buf[self->pos];

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      unsigned int i;

      for (i = 0; i < trace->n_addrs; i++)
        trace->addrs[i] = bswap_64 (trace->addrs[i]);
    }

  self->pos += trace->frame.len;

  return trace;
}

const SysprofCaptureCounterDefine *
sysprof_capture_reader_read_counter_define (SysprofCaptureReader *self)
{
  SysprofCaptureCounterDefine *def;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *def))
    return NULL;

  def = (SysprofCaptureCounterDefine *)(void *)&self->buf[self->pos];

  if (def->frame.type != SYSPROF_CAPTURE_FRAME_CTRDEF)
    return NULL;

  if (def->frame.len < sizeof *def)
    return NULL;

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    def->n_counters = bswap_16 (def->n_counters);

  if (def->frame.len < (sizeof *def + (sizeof (SysprofCaptureCounterDefine) * def->n_counters)))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, def->frame.len))
    return NULL;

  def = (SysprofCaptureCounterDefine *)(void *)&self->buf[self->pos];

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      unsigned int i;

      for (i = 0; i < def->n_counters; i++)
        {
          def->counters[i].id = bswap_32 (def->counters[i].id);
          def->counters[i].value.v64 = bswap_64 (def->counters[i].value.v64);
        }
    }

  self->pos += def->frame.len;

  return def;
}

const SysprofCaptureCounterSet *
sysprof_capture_reader_read_counter_set (SysprofCaptureReader *self)
{
  SysprofCaptureCounterSet *set;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *set))
    return NULL;

  set = (SysprofCaptureCounterSet *)(void *)&self->buf[self->pos];

  if (set->frame.type != SYSPROF_CAPTURE_FRAME_CTRSET)
    return NULL;

  if (set->frame.len < sizeof *set)
    return NULL;

  if (self->endian != __BYTE_ORDER)
    set->n_values = bswap_16 (set->n_values);

  if (set->frame.len < (sizeof *set + (sizeof (SysprofCaptureCounterValues) * set->n_values)))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, set->frame.len))
    return NULL;

  set = (SysprofCaptureCounterSet *)(void *)&self->buf[self->pos];

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      unsigned int i;

      for (i = 0; i < set->n_values; i++)
        {
          unsigned int j;

          for (j = 0; j < SYSPROF_N_ELEMENTS (set->values[0].values); j++)
            {
              set->values[i].ids[j] = bswap_32 (set->values[i].ids[j]);
              set->values[i].values[j].v64 = bswap_64 (set->values[i].values[j].v64);
            }
        }
    }

  self->pos += set->frame.len;

  return set;
}

bool
sysprof_capture_reader_reset (SysprofCaptureReader *self)
{
  assert (self != NULL);

  self->fd_off = sizeof (SysprofCaptureFileHeader);
  self->pos = 0;
  self->len = 0;

  return true;
}

SysprofCaptureReader *
sysprof_capture_reader_ref (SysprofCaptureReader *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  __atomic_fetch_add (&self->ref_count, 1, __ATOMIC_SEQ_CST);

  return self;
}

void
sysprof_capture_reader_unref (SysprofCaptureReader *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  if (__atomic_fetch_sub (&self->ref_count, 1, __ATOMIC_SEQ_CST) == 1)
    sysprof_capture_reader_finalize (self);
}

bool
sysprof_capture_reader_splice (SysprofCaptureReader *self,
                               SysprofCaptureWriter *dest)
{
  assert (self != NULL);
  assert (self->fd != -1);
  assert (dest != NULL);

  /* Flush before writing anything to ensure consistency */
  if (!sysprof_capture_writer_flush (dest))
    {
      /* errno is propagated */
      return false;
    }

  /*
   * We don't need to track position because writer will
   * track the current position to avoid reseting it.
   */

  /* Perform the splice; errno is propagated on failure */
  return _sysprof_capture_writer_splice_from_fd (dest, self->fd);
}

/**
 * sysprof_capture_reader_save_as:
 * @self: An #SysprofCaptureReader
 * @filename: the file to save the capture as
 *
 * This is a convenience function for copying a capture file for which
 * you may have already discarded the writer for.
 *
 * `errno` is set on failure. It may be any of the errors returned by
 * `open()`, `fstat()`, `ftruncate()`, `lseek()` or `sendfile()`.
 *
 * Returns: %TRUE on success; otherwise %FALSE.
 */
bool
sysprof_capture_reader_save_as (SysprofCaptureReader *self,
                                const char           *filename)
{
  struct stat stbuf;
  off_t in_off;
  size_t to_write;
  int fd = -1;
  int errsv;

  assert (self != NULL);
  assert (filename != NULL);

  if (-1 == (fd = open (filename, O_CREAT | O_WRONLY, 0640)))
    goto handle_errno;

  if (-1 == fstat (self->fd, &stbuf))
    goto handle_errno;

  if (-1 == ftruncate (fd, stbuf.st_size))
    goto handle_errno;

  if ((off_t)-1 == lseek (fd, 0L, SEEK_SET))
    goto handle_errno;

  in_off = 0;
  to_write = stbuf.st_size;

  while (to_write > 0)
    {
      ssize_t written;

      written = _sysprof_sendfile (fd, self->fd, &in_off, to_write);

      if (written < 0)
        goto handle_errno;

      if (written == 0 && errno != EAGAIN)
        goto handle_errno;

      assert (written <= (ssize_t)to_write);

      to_write -= written;
    }

  if (self->filename == NULL)
    self->filename = sysprof_strdup (filename);

  close (fd);

  return true;

handle_errno:
  errsv = errno;

  if (fd != -1)
    close (fd);

  errno = errsv;

  return false;
}

int64_t
sysprof_capture_reader_get_start_time (SysprofCaptureReader *self)
{
  assert (self != NULL);

  if (self->endian != __BYTE_ORDER)
    return bswap_64 (self->header.time);

  return self->header.time;
}

/**
 * sysprof_capture_reader_get_end_time:
 *
 * This function will return the end time for the capture, which is the
 * same as the last event added to the capture.
 *
 * If the end time has been stored in the capture header, that will be used.
 * Otherwise, the time is discovered from the last capture frame and therefore
 * the caller must have read all frames to ensure this value is accurate.
 *
 * Captures created by sysprof versions containing this API will have the
 * end time set if the output capture file-descriptor supports seeking.
 *
 * Since: 3.22.1
 */
int64_t
sysprof_capture_reader_get_end_time (SysprofCaptureReader *self)
{
  int64_t end_time = 0;

  assert (self != NULL);

  if (self->header.end_time != 0)
    {
      if (self->endian != __BYTE_ORDER)
        end_time = bswap_64 (self->header.end_time);
      else
        end_time = self->header.end_time;
    }

  return (self->end_time > end_time) ? self->end_time : end_time;
}

/**
 * sysprof_capture_reader_copy:
 *
 * This function makes a copy of the reader. Since readers use
 * positioned reads with pread(), this allows you to have multiple
 * readers with the shared file descriptor. This uses dup() to create
 * another file descriptor.
 *
 * Returns: (transfer full): A copy of @self with a new file-descriptor.
 */
SysprofCaptureReader *
sysprof_capture_reader_copy (SysprofCaptureReader *self)
{
  SysprofCaptureReader *copy;
  int fd;

  assert (self != NULL);

  if (-1 == (fd = dup (self->fd)))
    return NULL;

  copy = sysprof_malloc0 (sizeof (SysprofCaptureReader));
  if (copy == NULL)
    {
      close (fd);
      return NULL;
    }

  *copy = *self;

  copy->ref_count = 1;
  copy->filename = sysprof_strdup (self->filename);
  copy->fd = fd;
  copy->end_time = self->end_time;
  copy->st_buf = self->st_buf;
  copy->st_buf_set = self->st_buf_set;

  copy->buf = malloc (self->bufsz);
  if (copy->buf == NULL)
    {
      close (fd);
      free (copy->filename);
      free (copy);
      return NULL;
    }

  memcpy (copy->buf, self->buf, self->bufsz);

  return copy;
}

void
sysprof_capture_reader_set_stat (SysprofCaptureReader     *self,
                                 const SysprofCaptureStat *st_buf)
{
  assert (self != NULL);

  if (st_buf != NULL)
    {
      self->st_buf = *st_buf;
      self->st_buf_set = true;
    }
  else
    {
      memset (&self->st_buf, 0, sizeof (self->st_buf));
      self->st_buf_set = false;
    }
}

bool
sysprof_capture_reader_get_stat (SysprofCaptureReader *self,
                                 SysprofCaptureStat   *st_buf)
{
  assert (self != NULL);

  if (st_buf != NULL)
    *st_buf = self->st_buf;

  return self->st_buf_set;
}

const SysprofCaptureFileChunk *
sysprof_capture_reader_read_file (SysprofCaptureReader *self)
{
  SysprofCaptureFileChunk *file_chunk;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *file_chunk))
    return NULL;

  file_chunk = (SysprofCaptureFileChunk *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &file_chunk->frame);

  if (file_chunk->frame.type != SYSPROF_CAPTURE_FRAME_FILE_CHUNK)
    return NULL;

  if (file_chunk->frame.len < sizeof *file_chunk)
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, file_chunk->frame.len))
    return NULL;

  file_chunk = (SysprofCaptureFileChunk *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_file_chunk (self, file_chunk);

  self->pos += file_chunk->frame.len;

  if ((self->pos % SYSPROF_CAPTURE_ALIGN) != 0)
    return NULL;

  /* Make sure len is < the extra frame data */
  if (file_chunk->len > (file_chunk->frame.len - sizeof *file_chunk))
    return NULL;

  /* Ensure trailing \0 in .path */
  file_chunk->path[sizeof file_chunk->path - 1] = 0;

  return file_chunk;
}

static bool
array_append (const char ***files,
              size_t       *n_files,
              size_t       *n_files_allocated,
              const char   *new_element)
{
  if (*n_files == *n_files_allocated)
    {
      const char **new_files;

      *n_files_allocated = (*n_files_allocated > 0) ? 2 * *n_files_allocated : 4;
      new_files = _sysprof_reallocarray (*files, *n_files_allocated, sizeof (**files));
      if (new_files == NULL)
        return false;
      *files = new_files;
    }

  (*files)[*n_files] = new_element ? sysprof_strdup (new_element) : NULL;
  *n_files = *n_files + 1;
  assert (*n_files <= *n_files_allocated);

  return true;
}

static void
array_deduplicate (const char **files,
                   size_t      *n_files)
{
  size_t last_written, next_to_read;

  if (*n_files == 0)
    return;

  for (last_written = 0, next_to_read = 1; last_written <= next_to_read && next_to_read < *n_files;)
    {
      if (strcmp (files[next_to_read], files[last_written]) == 0)
        {
          free ((char *)files[next_to_read]);
          next_to_read++;
        }
      else
        files[++last_written] = files[next_to_read++];
    }

  assert (last_written + 1 <= *n_files);
  *n_files = last_written + 1;
}

static int
compare_strings (const void *a,
                 const void *b)
{
  const char * const *astr = a;
  const char * const *bstr = b;

  return strcmp (*astr, *bstr);
}

const char **
sysprof_capture_reader_list_files (SysprofCaptureReader *self)
{
  const char **files = NULL;
  size_t n_files = 0, n_files_allocated = 0;
  SysprofCaptureFrameType type;

  assert (self != NULL);

  /* Only generate the list of files once */
  if (self->list_files == NULL)
    {
      while (sysprof_capture_reader_peek_type (self, &type))
        {
          const SysprofCaptureFileChunk *file;

          if (type != SYSPROF_CAPTURE_FRAME_FILE_CHUNK)
            {
              sysprof_capture_reader_skip (self);
              continue;
            }

          if (!(file = sysprof_capture_reader_read_file (self)))
            break;

          if (!array_append (&files, &n_files, &n_files_allocated, file->path))
            {
              free (files);
              errno = ENOMEM;
              return NULL;
            }
        }

      /* Sort and deduplicate the files array. */
      qsort (files, n_files, sizeof (*files), compare_strings);
      array_deduplicate (files, &n_files);

      /* Add a null terminator */
      if (!array_append (&files, &n_files, &n_files_allocated, NULL))
        {
          free (files);
          errno = ENOMEM;
          return NULL;
        }

      self->list_files = (char **)sysprof_steal_pointer (&files);
      self->n_list_files = n_files; /* including NULL */
    }

  /* Now copy the list but not the strings */
  files = malloc (sizeof (char *) * self->n_list_files);
  memcpy (files, self->list_files, sizeof (char *) * self->n_list_files);

  return sysprof_steal_pointer (&files);
}

bool
sysprof_capture_reader_read_file_fd (SysprofCaptureReader *self,
                                     const char           *path,
                                     int                   fd)
{
  assert (self != NULL);
  assert (path != NULL);
  assert (fd > -1);

  for (;;)
    {
      SysprofCaptureFrameType type;
      const SysprofCaptureFileChunk *file;
      const uint8_t *buf;
      size_t to_write;

      if (!sysprof_capture_reader_peek_type (self, &type))
        return false;

      if (type != SYSPROF_CAPTURE_FRAME_FILE_CHUNK)
        goto skip;

      if (!(file = sysprof_capture_reader_read_file (self)))
        return false;

      if (strcmp (path, file->path) != 0)
        goto skip;

      buf = file->data;
      to_write = file->len;

      while (to_write > 0)
        {
          ssize_t written;

          written = _sysprof_write (fd, buf, to_write);
          if (written < 0)
            return false;

          if (written == 0 && errno != EAGAIN)
            return false;

          assert (written <= (ssize_t)to_write);

          buf += written;
          to_write -= written;
        }

      if (!file->is_last)
        continue;

      return true;

    skip:
      if (!sysprof_capture_reader_skip (self))
        return false;
    }

  sysprof_assert_not_reached ();
}

int
sysprof_capture_reader_get_byte_order (SysprofCaptureReader *self)
{
  assert (self != NULL);

  return self->endian;
}

const SysprofCaptureFileChunk *
sysprof_capture_reader_find_file (SysprofCaptureReader *self,
                                  const char           *path)
{
  SysprofCaptureFrameType type;

  assert (self != NULL);
  assert (path != NULL);

  while (sysprof_capture_reader_peek_type (self, &type))
    {
      if (type == SYSPROF_CAPTURE_FRAME_FILE_CHUNK)
        {
          const SysprofCaptureFileChunk *fc;

          if (!(fc = sysprof_capture_reader_read_file (self)))
            break;

          if (strcmp (path, fc->path) == 0)
            return fc;

          continue;
        }

      if (!sysprof_capture_reader_skip (self))
        break;
    }

  return NULL;
}

const SysprofCaptureAllocation *
sysprof_capture_reader_read_allocation (SysprofCaptureReader *self)
{
  SysprofCaptureAllocation *ma;

  assert (self != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);
  assert (self->pos <= self->bufsz);

  if (!sysprof_capture_reader_ensure_space_for (self, sizeof *ma))
    return NULL;

  ma = (SysprofCaptureAllocation *)(void *)&self->buf[self->pos];

  sysprof_capture_reader_bswap_frame (self, &ma->frame);

  if (ma->frame.type != SYSPROF_CAPTURE_FRAME_ALLOCATION)
    return NULL;

  if (ma->frame.len < sizeof *ma)
    return NULL;

  if (self->endian != __BYTE_ORDER)
    {
      ma->n_addrs = bswap_16 (ma->n_addrs);
      ma->alloc_size = bswap_64 (ma->alloc_size);
      ma->alloc_addr = bswap_64 (ma->alloc_addr);
      ma->tid = bswap_32 (ma->tid);
    }

  if (ma->frame.len < (sizeof *ma + (sizeof(SysprofCaptureAddress) * ma->n_addrs)))
    return NULL;

  if (!sysprof_capture_reader_ensure_space_for (self, ma->frame.len))
    return NULL;

  ma = (SysprofCaptureAllocation *)(void *)&self->buf[self->pos];

  if (SYSPROF_UNLIKELY (self->endian != __BYTE_ORDER))
    {
      for (unsigned int i = 0; i < ma->n_addrs; i++)
        ma->addrs[i] = bswap_64 (ma->addrs[i]);
    }

  self->pos += ma->frame.len;

  return ma;
}

typedef struct {
  const SysprofCaptureJitmap *jitmap;
  const uint8_t *buf;
  unsigned int i;

  void *padding1;
  void *padding2;
} RealSysprofCaptureJitmapIter;

void
sysprof_capture_jitmap_iter_init (SysprofCaptureJitmapIter   *iter,
                                  const SysprofCaptureJitmap *jitmap)
{
  RealSysprofCaptureJitmapIter *real_iter = (RealSysprofCaptureJitmapIter *) iter;

  assert (iter != NULL);
  assert (jitmap != NULL);

  real_iter->jitmap = jitmap;
  real_iter->buf = jitmap->data;
  real_iter->i = 0;
}

bool
sysprof_capture_jitmap_iter_next (SysprofCaptureJitmapIter  *iter,
                                  SysprofCaptureAddress     *addr,
                                  const char               **name)
{
  RealSysprofCaptureJitmapIter *real_iter = (RealSysprofCaptureJitmapIter *) iter;
  const char *_name;

  assert (iter != NULL);

  if (real_iter->i >= real_iter->jitmap->n_jitmaps)
    return false;

  if (addr != NULL)
    memcpy (addr, real_iter->buf, sizeof (*addr));
  real_iter->buf += sizeof (*addr);

  _name = (const char *) real_iter->buf;
  if (name != NULL)
    *name = _name;
  real_iter->buf += strlen (_name) + 1;

  real_iter->i++;

  return true;
}
