/* sysprof-capture-writer.c
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
#ifdef __APPLE__
# include <machine/endian.h>
#else
# include <endian.h>
#endif
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
#include "sysprof-macros.h"

#define DEFAULT_BUFFER_SIZE (_sysprof_getpagesize() * 64L)
#define INVALID_ADDRESS     (SYSPROF_UINT64_CONSTANT(0))
#define MAX_COUNTERS        ((1 << 24) - 1)
#define MAX_UNWIND_DEPTH    64

typedef struct
{
  /* A pinter into the string buffer */
  const char *str;

  /* The unique address for the string */
  uint64_t addr;
} SysprofCaptureJitmapBucket;

struct _SysprofCaptureWriter
{
  /*
   * This is our buffer location for incoming strings. This is used
   * similarly to GStringChunk except there is only one-page, and after
   * it fills, we flush to disk.
   *
   * This is paired with a closed hash table for deduplication.
   */
  char addr_buf[4096*4];

  /* Our hashtable for deduplication. */
  SysprofCaptureJitmapBucket addr_hash[512];

  /* We keep the large fields above so that our allocation will be page
   * alinged for the write buffer. This improves the performance of large
   * writes to the target file-descriptor.
   */
  volatile int ref_count;

  /*
   * Our address sequence counter. The value that comes from
   * monotonically increasing this is OR'd with JITMAP_MARK to denote
   * the address name should come from the JIT map.
   */
  size_t addr_seq;

  /* Our position in addr_buf. */
  size_t addr_buf_pos;

  /*
   * The number of hash table items in @addr_hash. This is an
   * optimization so that we can avoid calculating the number of strings
   * when flushing out the jitmap.
   */
  unsigned int addr_hash_size;

  /* Capture file handle */
  int fd;

  /* Our write buffer for fd */
  uint8_t *buf;
  size_t pos;
  size_t len;

  /* counter id sequence */
  int next_counter_id;

  /* Statistics while recording */
  SysprofCaptureStat stat;
};

static inline void
sysprof_capture_writer_frame_init (SysprofCaptureFrame     *frame_,
                                   int                      len,
                                   int                      cpu,
                                   int32_t                  pid,
                                   int64_t                  time_,
                                   SysprofCaptureFrameType  type)
{
  assert (frame_ != NULL);

  frame_->len = len;
  frame_->cpu = cpu;
  frame_->pid = pid;
  frame_->time = time_;
  frame_->type = type;
  frame_->padding1 = 0;
  frame_->padding2 = 0;
}

static void
sysprof_capture_writer_finalize (SysprofCaptureWriter *self)
{
  if (self != NULL)
    {
      sysprof_capture_writer_flush (self);

      if (self->fd != -1)
        {
          close (self->fd);
          self->fd = -1;
        }

      free (self->buf);
      free (self);
    }
}

SysprofCaptureWriter *
sysprof_capture_writer_ref (SysprofCaptureWriter *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  __atomic_fetch_add (&self->ref_count, 1, __ATOMIC_SEQ_CST);

  return self;
}

void
sysprof_capture_writer_unref (SysprofCaptureWriter *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  if (__atomic_fetch_sub (&self->ref_count, 1, __ATOMIC_SEQ_CST) == 1)
    sysprof_capture_writer_finalize (self);
}

static bool
sysprof_capture_writer_flush_data (SysprofCaptureWriter *self)
{
  const uint8_t *buf;
  ssize_t written;
  size_t to_write;

  assert (self != NULL);
  assert (self->pos <= self->len);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);

  if (self->pos == 0)
    return true;

  buf = self->buf;
  to_write = self->pos;

  while (to_write > 0)
    {
      written = _sysprof_write (self->fd, buf, to_write);
      if (written < 0)
        return false;

      if (written == 0 && errno != EAGAIN)
        return false;

      assert (written <= (ssize_t)to_write);

      buf += written;
      to_write -= written;
    }

  self->pos = 0;

  return true;
}

static inline void
sysprof_capture_writer_realign (size_t *pos)
{
  *pos = (*pos + SYSPROF_CAPTURE_ALIGN - 1) & ~(SYSPROF_CAPTURE_ALIGN - 1);
}

static inline bool
sysprof_capture_writer_ensure_space_for (SysprofCaptureWriter *self,
                                         size_t                len)
{
  /* Check for max frame size */
  if (len > USHRT_MAX)
    return false;

  if ((self->len - self->pos) < len)
    {
      if (!sysprof_capture_writer_flush_data (self))
        return false;
    }

  return true;
}

static inline void *
sysprof_capture_writer_allocate (SysprofCaptureWriter *self,
                                 size_t               *len)
{
  void *p;

  assert (self != NULL);
  assert (len != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);

  sysprof_capture_writer_realign (len);

  if (!sysprof_capture_writer_ensure_space_for (self, *len))
    return NULL;

  p = (void *)&self->buf[self->pos];

  self->pos += *len;

  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);

  return p;
}

static bool
sysprof_capture_writer_flush_jitmap (SysprofCaptureWriter *self)
{
  SysprofCaptureJitmap jitmap;
  ssize_t r;
  size_t len;

  assert (self != NULL);

  if (self->addr_hash_size == 0)
    return true;

  assert (self->addr_buf_pos > 0);

  len = sizeof jitmap + self->addr_buf_pos;

  sysprof_capture_writer_realign (&len);

  sysprof_capture_writer_frame_init (&jitmap.frame,
                                     len,
                                     -1,
                                     _sysprof_getpid (),
                                     SYSPROF_CAPTURE_CURRENT_TIME,
                                     SYSPROF_CAPTURE_FRAME_JITMAP);
  jitmap.n_jitmaps = self->addr_hash_size;

  if (sizeof jitmap != _sysprof_write (self->fd, &jitmap, sizeof jitmap))
    return false;

  r = _sysprof_write (self->fd, self->addr_buf, len - sizeof jitmap);
  if (r < 0 || (size_t)r != (len - sizeof jitmap))
    return false;

  self->addr_buf_pos = 0;
  self->addr_hash_size = 0;
  memset (self->addr_hash, 0, sizeof self->addr_hash);

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_JITMAP]++;

  return true;
}

/* djb hash */
static unsigned int
str_hash (const char *str)
{
  const uint8_t *p;
  uint32_t h = 5381;

  for (p = (const uint8_t *) str; *p != '\0'; p++)
    h = (h << 5) + h + *p;

  return h;
}

static bool
sysprof_capture_writer_lookup_jitmap (SysprofCaptureWriter  *self,
                                      const char            *name,
                                      SysprofCaptureAddress *addr)
{
  unsigned int hash;
  unsigned int i;

  assert (self != NULL);
  assert (name != NULL);
  assert (addr != NULL);

  hash = str_hash (name) % SYSPROF_N_ELEMENTS (self->addr_hash);

  for (i = hash; i < SYSPROF_N_ELEMENTS (self->addr_hash); i++)
    {
      SysprofCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (bucket->str == NULL)
        return false;

      if (strcmp (bucket->str, name) == 0)
        {
          *addr = bucket->addr;
          return true;
        }
    }

  for (i = 0; i < hash; i++)
    {
      SysprofCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (bucket->str == NULL)
        return false;

      if (strcmp (bucket->str, name) == 0)
        {
          *addr = bucket->addr;
          return true;
        }
    }

  return false;
}

static SysprofCaptureAddress
sysprof_capture_writer_insert_jitmap (SysprofCaptureWriter *self,
                                      const char           *str)
{
  SysprofCaptureAddress addr;
  char *dst;
  size_t len;
  unsigned int hash;
  unsigned int i;

  assert (self != NULL);
  assert (str != NULL);
  assert ((self->pos % SYSPROF_CAPTURE_ALIGN) == 0);

  len = sizeof addr + strlen (str) + 1;

  if ((self->addr_hash_size == SYSPROF_N_ELEMENTS (self->addr_hash)) ||
      ((sizeof self->addr_buf - self->addr_buf_pos) < len))
    {
      if (!sysprof_capture_writer_flush_jitmap (self))
        return INVALID_ADDRESS;

      assert (self->addr_hash_size == 0);
      assert (self->addr_buf_pos == 0);
    }

  assert (self->addr_hash_size < SYSPROF_N_ELEMENTS (self->addr_hash));
  assert (len > sizeof addr);

  /* Allocate the next unique address */
  addr = SYSPROF_CAPTURE_JITMAP_MARK | ++self->addr_seq;

  /* Copy the address into the buffer */
  dst = (char *)&self->addr_buf[self->addr_buf_pos];
  memcpy (dst, &addr, sizeof addr);

  /*
   * Copy the string into the buffer, keeping dst around for
   * when we insert into the hashtable.
   */
  dst += sizeof addr;
  memcpy (dst, str, len - sizeof addr);

  /* Advance our string cache position */
  self->addr_buf_pos += len;
  assert (self->addr_buf_pos <= sizeof self->addr_buf);

  /* Now place the address into the hashtable */
  hash = str_hash (str) % SYSPROF_N_ELEMENTS (self->addr_hash);

  /* Start from the current hash bucket and go forward */
  for (i = hash; i < SYSPROF_N_ELEMENTS (self->addr_hash); i++)
    {
      SysprofCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (SYSPROF_LIKELY (bucket->str == NULL))
        {
          bucket->str = dst;
          bucket->addr = addr;
          self->addr_hash_size++;
          return addr;
        }
    }

  /* Wrap around to the beginning */
  for (i = 0; i < hash; i++)
    {
      SysprofCaptureJitmapBucket *bucket = &self->addr_hash[i];

      if (SYSPROF_LIKELY (bucket->str == NULL))
        {
          bucket->str = dst;
          bucket->addr = addr;
          self->addr_hash_size++;
          return addr;
        }
    }

  sysprof_assert_not_reached ();

  return INVALID_ADDRESS;
}

SysprofCaptureWriter *
sysprof_capture_writer_new_from_fd (int    fd,
                                    size_t buffer_size)
{
  time_t now;
  char now_str[sizeof ("2020-06-30T14:34:00Z")];
  SysprofCaptureWriter *self;
  SysprofCaptureFileHeader *header;
  size_t header_len = sizeof(*header);

  if (fd < 0)
    return NULL;

  if (buffer_size == 0)
    buffer_size = DEFAULT_BUFFER_SIZE;

  assert (fd != -1);
  assert (buffer_size % _sysprof_getpagesize() == 0);

  /* This is only useful on files, memfd, etc */
  if (ftruncate (fd, 0) != 0) { /* Do Nothing */ }

  self = sysprof_malloc0 (sizeof (SysprofCaptureWriter));
  if (self == NULL)
    return NULL;

  self->ref_count = 1;
  self->fd = fd;
  self->buf = (uint8_t *) sysprof_malloc0 (buffer_size);
  if (self->buf == NULL)
    {
      free (self);
      return NULL;
    }
  self->len = buffer_size;
  self->next_counter_id = 1;

  /* Format the time as ISO 8601, in UTC */
  time (&now);
  if (strftime (now_str, sizeof (now_str), "%FT%TZ", gmtime (&now)) == 0)
    {
      free (self->buf);
      free (self);
      return NULL;
    }

  header = sysprof_capture_writer_allocate (self, &header_len);

  if (header == NULL)
    {
      sysprof_capture_writer_finalize (self);
      return NULL;
    }

  header->magic = SYSPROF_CAPTURE_MAGIC;
  header->version = 1;
#if __BYTE_ORDER == __LITTLE_ENDIAN
  header->little_endian = true;
#else
  header->little_endian = false;
#endif
  header->padding = 0;
  _sysprof_strlcpy (header->capture_time, now_str, sizeof header->capture_time);
  header->time = SYSPROF_CAPTURE_CURRENT_TIME;
  header->end_time = 0;
  memset (header->suffix, 0, sizeof header->suffix);

  if (!sysprof_capture_writer_flush_data (self))
    {
      sysprof_capture_writer_finalize (self);
      return NULL;
    }

  assert (self->pos == 0);
  assert (self->len > 0);
  assert (self->len % _sysprof_getpagesize() == 0);
  assert (self->buf != NULL);
  assert (self->addr_hash_size == 0);
  assert (self->fd != -1);

  return self;
}

SysprofCaptureWriter *
sysprof_capture_writer_new (const char *filename,
                            size_t      buffer_size)
{
  SysprofCaptureWriter *self;
  int fd;

  assert (filename != NULL);
  assert (buffer_size % _sysprof_getpagesize() == 0);

  if ((-1 == (fd = open (filename, O_CREAT | O_RDWR, 0640))) ||
      (-1 == ftruncate (fd, 0L)))
    return NULL;

  self = sysprof_capture_writer_new_from_fd (fd, buffer_size);

  if (self == NULL)
    close (fd);

  return self;
}

bool
sysprof_capture_writer_add_map (SysprofCaptureWriter *self,
                                int64_t               time,
                                int                   cpu,
                                int32_t               pid,
                                uint64_t              start,
                                uint64_t              end,
                                uint64_t              offset,
                                uint64_t              inode,
                                const char           *filename)
{
  SysprofCaptureMap *ev;
  size_t len;

  if (filename == NULL)
    filename = "";

  assert (self != NULL);
  assert (filename != NULL);

  len = sizeof *ev + strlen (filename) + 1;

  ev = (SysprofCaptureMap *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_MAP);
  ev->start = start;
  ev->end = end;
  ev->offset = offset;
  ev->inode = inode;

  _sysprof_strlcpy (ev->filename, filename, len - sizeof *ev);
  ev->filename[len - sizeof *ev - 1] = '\0';

  ((char*)ev)[len-1] = 0;

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_MAP]++;

  return true;
}

bool
sysprof_capture_writer_add_map_with_build_id (SysprofCaptureWriter *self,
                                              int64_t               time,
                                              int                   cpu,
                                              int32_t               pid,
                                              uint64_t              start,
                                              uint64_t              end,
                                              uint64_t              offset,
                                              uint64_t              inode,
                                              const char           *filename,
                                              const char           *build_id)
{
  SysprofCaptureMap *ev;
  size_t len;
  size_t filename_len;
  size_t build_id_len;

  if (filename == NULL)
    filename = "";

  if (build_id == NULL)
    build_id = "";

  assert (self != NULL);
  assert (filename != NULL);
  assert (build_id != NULL);

  filename_len = strlen (filename) + 1;
  build_id_len = strlen (build_id) + 1;

  len = sizeof *ev + filename_len + strlen("@") + build_id_len;

  ev = (SysprofCaptureMap *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_MAP);
  ev->start = start;
  ev->end = end;
  ev->offset = offset;
  ev->inode = inode;

  _sysprof_strlcpy (ev->filename, filename, filename_len);
  ev->filename[filename_len] = '@';
  _sysprof_strlcpy (&ev->filename[filename_len+1], build_id, build_id_len);

  ((char*)ev)[len-1] = 0;

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_MAP]++;

  return true;
}

bool
sysprof_capture_writer_add_mark (SysprofCaptureWriter *self,
                                 int64_t               time,
                                 int                   cpu,
                                 int32_t               pid,
                                 uint64_t              duration,
                                 const char           *group,
                                 const char           *name,
                                 const char           *message)
{
  SysprofCaptureMark *ev;
  size_t message_len;
  size_t len;

  assert (self != NULL);
  assert (name != NULL);
  assert (group != NULL);

  if (message == NULL)
    message = "";
  message_len = strlen (message) + 1;

  len = sizeof *ev + message_len;
  ev = (SysprofCaptureMark *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_MARK);

  ev->duration = duration;
  _sysprof_strlcpy (ev->group, group, sizeof ev->group);
  _sysprof_strlcpy (ev->name, name, sizeof ev->name);
  memcpy (ev->message, message, message_len);

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_MARK]++;

  return true;
}

bool
sysprof_capture_writer_add_metadata (SysprofCaptureWriter *self,
                                     int64_t               time,
                                     int                   cpu,
                                     int32_t               pid,
                                     const char           *id,
                                     const char           *metadata,
                                     ssize_t               metadata_len)
{
  SysprofCaptureMetadata *ev;
  size_t len;

  assert (self != NULL);
  assert (id != NULL);

  if (metadata == NULL)
    {
      metadata = "";
      len = 0;
    }

  if (metadata_len < 0)
    metadata_len = strlen (metadata);

  len = sizeof *ev + metadata_len + 1;
  ev = (SysprofCaptureMetadata *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_METADATA);

  _sysprof_strlcpy (ev->id, id, sizeof ev->id);
  memcpy (ev->metadata, metadata, metadata_len);
  ev->metadata[metadata_len] = 0;

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_METADATA]++;

  return true;
}

bool
sysprof_capture_writer_add_dbus_message (SysprofCaptureWriter *self,
                                         int64_t               time,
                                         int                   cpu,
                                         int32_t               pid,
                                         uint16_t              bus_type,
                                         uint16_t              flags,
                                         const uint8_t        *message_data,
                                         size_t                message_len)
{
  SysprofCaptureDBusMessage *ev;
  size_t len;

  assert (self != NULL);
  assert (message_data != NULL || message_len == 0);

  if (message_len > (1<<16) - sizeof *ev - 16)
    {
      flags |= SYSPROF_CAPTURE_DBUS_FLAGS_MESSAGE_TOO_LARGE;
      message_data = NULL;
      message_len = 0;
    }

  len = sizeof *ev + message_len;

  ev = (SysprofCaptureDBusMessage *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_DBUS_MESSAGE);

  ev->bus_type = bus_type;
  ev->flags = flags;
  ev->message_len = message_len;
  memcpy (ev->message, message_data, message_len);

  return true;
}

SysprofCaptureAddress
sysprof_capture_writer_add_jitmap (SysprofCaptureWriter *self,
                                   const char           *name)
{
  SysprofCaptureAddress addr = INVALID_ADDRESS;

  if (name == NULL)
    name = "";

  assert (self != NULL);
  assert (name != NULL);

  if (!sysprof_capture_writer_lookup_jitmap (self, name, &addr))
    addr = sysprof_capture_writer_insert_jitmap (self, name);

  return addr;
}

bool
sysprof_capture_writer_add_process (SysprofCaptureWriter *self,
                                    int64_t               time,
                                    int                   cpu,
                                    int32_t               pid,
                                    const char           *cmdline)
{
  SysprofCaptureProcess *ev;
  size_t len;

  if (cmdline == NULL)
    cmdline = "";

  assert (self != NULL);
  assert (cmdline != NULL);

  len = sizeof *ev + strlen (cmdline) + 1;

  ev = (SysprofCaptureProcess *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_PROCESS);

  _sysprof_strlcpy (ev->cmdline, cmdline, len - sizeof *ev);
  ev->cmdline[len - sizeof *ev - 1] = '\0';

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_PROCESS]++;

  return true;
}

bool
sysprof_capture_writer_add_sample (SysprofCaptureWriter        *self,
                                   int64_t                      time,
                                   int                          cpu,
                                   int32_t                      pid,
                                   int32_t                      tid,
                                   const SysprofCaptureAddress *addrs,
                                   unsigned int                 n_addrs)
{
  SysprofCaptureSample *ev;
  size_t len;

  assert (self != NULL);

  len = sizeof *ev + (n_addrs * sizeof (SysprofCaptureAddress));

  ev = (SysprofCaptureSample *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_SAMPLE);
  ev->n_addrs = n_addrs;
  ev->tid = tid;

  memcpy (ev->addrs, addrs, (n_addrs * sizeof (SysprofCaptureAddress)));

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_SAMPLE]++;

  return true;
}

bool
sysprof_capture_writer_add_trace (SysprofCaptureWriter        *self,
                                  int64_t                      time,
                                  int                          cpu,
                                  int32_t                      pid,
                                  int32_t                      tid,
                                  const SysprofCaptureAddress *addrs,
                                  unsigned int                 n_addrs,
                                  bool                         entering)
{
  SysprofCaptureTrace *ev;
  size_t len;

  assert (self != NULL);

  len = sizeof *ev + (n_addrs * sizeof (SysprofCaptureAddress));

  ev = (SysprofCaptureTrace *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_SAMPLE);
  ev->n_addrs = n_addrs;
  ev->tid = tid;
  ev->entering = !!entering;

  memcpy (ev->addrs, addrs, (n_addrs * sizeof (SysprofCaptureAddress)));

  return true;
}

bool
sysprof_capture_writer_add_fork (SysprofCaptureWriter *self,
                                 int64_t          time,
                                 int              cpu,
                                 int32_t          pid,
                                 int32_t          child_pid)
{
  SysprofCaptureFork *ev;
  size_t len = sizeof *ev;

  assert (self != NULL);

  ev = (SysprofCaptureFork *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_FORK);
  ev->child_pid = child_pid;

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_FORK]++;

  return true;
}

bool
sysprof_capture_writer_add_overlay (SysprofCaptureWriter *self,
                                    int64_t               time,
                                    int                   cpu,
                                    int32_t               pid,
                                    uint32_t              layer,
                                    const char           *src,
                                    const char           *dst)
{
  SysprofCaptureOverlay *ev;
  size_t srclen = strlen (src);
  size_t dstlen = strlen (dst);
  size_t len = sizeof *ev + srclen + 1 + dstlen + 1;

  assert (self != NULL);
  assert (src != NULL);
  assert (dst != NULL);

  if (srclen > INT16_MAX || dstlen > INT16_MAX)
    return false;

  ev = (SysprofCaptureOverlay *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_OVERLAY);

  ev->layer = layer;
  ev->src_len = srclen;
  ev->dst_len = dstlen;

  memcpy (&ev->data[0], src, srclen);
  memcpy (&ev->data[srclen+1], dst, dstlen);

  ev->data[srclen] = 0;
  ev->data[srclen+1+dstlen] = 0;

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_OVERLAY]++;

  return true;
}

bool
sysprof_capture_writer_add_exit (SysprofCaptureWriter *self,
                                 int64_t               time,
                                 int                   cpu,
                                 int32_t               pid)
{
  SysprofCaptureExit *ev;
  size_t len = sizeof *ev;

  assert (self != NULL);

  ev = (SysprofCaptureExit *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_EXIT);

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_EXIT]++;

  return true;
}

bool
sysprof_capture_writer_add_timestamp (SysprofCaptureWriter *self,
                                      int64_t               time,
                                      int                   cpu,
                                      int32_t               pid)
{
  SysprofCaptureTimestamp *ev;
  size_t len = sizeof *ev;

  assert (self != NULL);

  ev = (SysprofCaptureTimestamp *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_TIMESTAMP);

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_TIMESTAMP]++;

  return true;
}

static bool
sysprof_capture_writer_flush_end_time (SysprofCaptureWriter *self)
{
  int64_t end_time = SYSPROF_CAPTURE_CURRENT_TIME;
  ssize_t ret;

  assert (self != NULL);

  /* This field is opportunistic, so a failure is okay. */

again:
  ret = _sysprof_pwrite (self->fd,
                         &end_time,
                         sizeof (end_time),
                         offsetof (SysprofCaptureFileHeader, end_time));

  if (ret < 0 && errno == EAGAIN)
    goto again;

  return true;
}

bool
sysprof_capture_writer_flush (SysprofCaptureWriter *self)
{
  assert (self != NULL);

  return sysprof_capture_writer_flush_jitmap (self) &&
         sysprof_capture_writer_flush_data (self) &&
         sysprof_capture_writer_flush_end_time (self);
}

/**
 * sysprof_capture_writer_save_as:
 * @self: A #SysprofCaptureWriter
 * @filename: the file to save the capture as
 *
 * Saves the captured data as the file @filename.
 *
 * This is primarily useful if the writer was created with a memory-backed
 * file-descriptor such as a memfd or tmpfs file on Linux.
 *
 * `errno` is set on error, to any of the errors returned by `open()`,
 * sysprof_capture_writer_flush(), `lseek()` or `sendfile()`.
 *
 * Returns: %TRUE if successful, otherwise %FALSE and `errno` is set.
 */
bool
sysprof_capture_writer_save_as (SysprofCaptureWriter *self,
                                const char           *filename)
{
  size_t to_write;
  off_t in_off;
  off_t pos;
  int fd = -1;
  int errsv;

  assert (self != NULL);
  assert (self->fd != -1);
  assert (filename != NULL);

  if (-1 == (fd = open (filename, O_CREAT | O_RDWR, 0640)))
    goto handle_errno;

  if (!sysprof_capture_writer_flush (self))
    goto handle_errno;

  if (-1 == (pos = lseek (self->fd, 0L, SEEK_CUR)))
    goto handle_errno;

  to_write = pos;
  in_off = 0;

  while (to_write > 0)
    {
      ssize_t written;

      written = _sysprof_sendfile (fd, self->fd, &in_off, pos);

      if (written < 0)
        goto handle_errno;

      if (written == 0 && errno != EAGAIN)
        goto handle_errno;

      assert (written <= (ssize_t)to_write);

      to_write -= written;
    }

  close (fd);

  return true;

handle_errno:
  errsv = errno;

  if (fd != -1)
    {
      close (fd);
      unlink (filename);
    }

  errno = errsv;

  return false;
}

/**
 * _sysprof_capture_writer_splice_from_fd:
 * @self: An #SysprofCaptureWriter
 * @fd: the fd to read from.
 *
 * This is internal API for SysprofCaptureWriter and SysprofCaptureReader to
 * communicate when splicing a reader into a writer.
 *
 * This should not be used outside of #SysprofCaptureReader or
 * #SysprofCaptureWriter.
 *
 * This will not advance the position of @fd.
 *
 * `errno` is set on error, to any of the errors returned by `fstat()` or
 * `sendfile()`, or `EBADMSG` if the file is corrupt.
 *
 * Returns: %TRUE if successful; otherwise %FALSE and `errno` is set.
 */
bool
_sysprof_capture_writer_splice_from_fd (SysprofCaptureWriter *self,
                                        int                   fd)
{
  struct stat stbuf;
  off_t in_off;
  size_t to_write;

  assert (self != NULL);
  assert (self->fd != -1);

  if (-1 == fstat (fd, &stbuf))
    goto handle_errno;

  if (stbuf.st_size < 256)
    {
      errno = EBADMSG;
      return false;
    }

  in_off = 256;
  to_write = stbuf.st_size - in_off;

  while (to_write > 0)
    {
      ssize_t written;

      written = _sysprof_sendfile (self->fd, fd, &in_off, to_write);

      if (written < 0)
        goto handle_errno;

      if (written == 0 && errno != EAGAIN)
        goto handle_errno;

      assert (written <= (ssize_t)to_write);

      to_write -= written;
    }

  return true;

handle_errno:
  /* errno is propagated */
  return false;
}

/**
 * sysprof_capture_writer_splice:
 * @self: An #SysprofCaptureWriter
 * @dest: An #SysprofCaptureWriter
 *
 * This function will copy the capture @self into the capture @dest.  This
 * tries to be semi-efficient by using sendfile() to copy the contents between
 * the captures. @self and @dest will be flushed before the contents are copied
 * into the @dest file-descriptor.
 *
 * `errno` is set on error, to any of the errors returned by
 * sysprof_capture_writer_flush(), `lseek()` or
 * _sysprof_capture_writer_splice_from_fd().
 *
 * Returns: %TRUE if successful, otherwise %FALSE and and `errno` is set.
 */
bool
sysprof_capture_writer_splice (SysprofCaptureWriter *self,
                               SysprofCaptureWriter *dest)
{
  bool ret;
  off_t pos;
  int errsv;

  assert (self != NULL);
  assert (self->fd != -1);
  assert (dest != NULL);
  assert (dest->fd != -1);

  /* Flush before writing anything to ensure consistency */
  if (!sysprof_capture_writer_flush (self) || !sysprof_capture_writer_flush (dest))
    goto handle_errno;

  /* Track our current position so we can reset */
  if ((off_t)-1 == (pos = lseek (self->fd, 0L, SEEK_CUR)))
    goto handle_errno;

  /* Perform the splice */
  ret = _sysprof_capture_writer_splice_from_fd (dest, self->fd);
  errsv = errno;

  /* Now reset or file-descriptor position (it should be the same */
  if (pos != lseek (self->fd, pos, SEEK_SET))
    goto handle_errno;

  if (!ret)
    errno = errsv;
  return ret;

handle_errno:
  /* errno is propagated */
  return false;
}

/**
 * sysprof_capture_writer_create_reader:
 * @self: A #SysprofCaptureWriter
 *
 * Creates a new reader for the writer.
 *
 * Since readers use positioned reads, this uses the same file-descriptor for
 * the #SysprofCaptureReader. Therefore, if you are writing to the capture while
 * also consuming from the reader, you could get transient failures unless you
 * synchronize the operations.
 *
 * `errno` is set on error, to any of the errors returned by
 * sysprof_capture_writer_flush(), `dup()` or
 * sysprof_capture_reader_new_from_fd().
 *
 * Returns: (transfer full): A #SysprofCaptureReader.
 */
SysprofCaptureReader *
sysprof_capture_writer_create_reader (SysprofCaptureWriter *self)
{
  SysprofCaptureReader *ret;
  int copy;

  assert (self != NULL);
  assert (self->fd != -1);

  if (!sysprof_capture_writer_flush (self))
    {
      /* errno is propagated */
      return NULL;
    }

  /*
   * We don't care about the write position, since the reader
   * uses positioned reads.
   */
  if (-1 == (copy = dup (self->fd)))
    {
      /* errno is propagated */
      return NULL;
    }

  if (!(ret = sysprof_capture_reader_new_from_fd (copy)))
    {
      /* errno is propagated */
      return NULL;
    }

  sysprof_capture_reader_set_stat (ret, &self->stat);

  return sysprof_steal_pointer (&ret);
}

/**
 * sysprof_capture_writer_stat:
 * @self: A #SysprofCaptureWriter
 * @stat: (out): A location for an #SysprofCaptureStat
 *
 * This function will fill @stat with statistics generated while capturing
 * the profiler session.
 */
void
sysprof_capture_writer_stat (SysprofCaptureWriter *self,
                             SysprofCaptureStat   *stat)
{
  assert (self != NULL);
  assert (stat != NULL);

  *stat = self->stat;
}

bool
sysprof_capture_writer_define_counters (SysprofCaptureWriter        *self,
                                        int64_t                      time,
                                        int                          cpu,
                                        int32_t                      pid,
                                        const SysprofCaptureCounter *counters,
                                        unsigned int                 n_counters)
{
  SysprofCaptureCounterDefine *def;
  size_t len;
  unsigned int i;

  assert (self != NULL);
  assert (counters != NULL);

  if (n_counters == 0)
    return true;

  len = sizeof *def + (sizeof *counters * n_counters);

  def = (SysprofCaptureCounterDefine *)sysprof_capture_writer_allocate (self, &len);
  if (!def)
    return false;

  sysprof_capture_writer_frame_init (&def->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_CTRDEF);
  def->padding1 = 0;
  def->padding2 = 0;
  def->n_counters = n_counters;

  for (i = 0; i < n_counters; i++)
    {
      /* Has the counter been registered? */
      assert (counters[i].id < self->next_counter_id);
      def->counters[i] = counters[i];
    }

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_CTRDEF]++;

  return true;
}

bool
sysprof_capture_writer_set_counters (SysprofCaptureWriter             *self,
                                     int64_t                           time,
                                     int                               cpu,
                                     int32_t                           pid,
                                     const unsigned int               *counters_ids,
                                     const SysprofCaptureCounterValue *values,
                                     unsigned int                      n_counters)
{
  SysprofCaptureCounterSet *set;
  size_t len;
  unsigned int n_groups;
  unsigned int group;
  unsigned int field;
  unsigned int i;

  assert (self != NULL);
  assert (counters_ids != NULL || n_counters == 0);
  assert (values != NULL || !n_counters);

  if (n_counters == 0)
    return true;

  /* Determine how many value groups we need */
  n_groups = n_counters / SYSPROF_N_ELEMENTS (set->values[0].values);
  if ((n_groups * SYSPROF_N_ELEMENTS (set->values[0].values)) != n_counters)
    n_groups++;

  len = sizeof *set + (n_groups * sizeof (SysprofCaptureCounterValues));

  set = (SysprofCaptureCounterSet *)sysprof_capture_writer_allocate (self, &len);
  if (!set)
    return false;

  memset (set, 0, len);

  sysprof_capture_writer_frame_init (&set->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_CTRSET);
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

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_CTRSET]++;

  return true;
}

/**
 * sysprof_capture_writer_request_counter:
 *
 * This requests a series of counter identifiers for the capture.
 *
 * The returning number is always greater than zero. The resulting counter
 * values are monotonic starting from the resulting value.
 *
 * For example, if you are returned 5, and requested 3 counters, the counter
 * ids you should use are 5, 6, and 7.
 *
 * Returns: The next series of counter values or zero on failure.
 */
unsigned int
sysprof_capture_writer_request_counter (SysprofCaptureWriter *self,
                                        unsigned int          n_counters)
{
  int ret;

  assert (self != NULL);

  if (MAX_COUNTERS - n_counters < self->next_counter_id)
    return 0;

  ret = self->next_counter_id;
  self->next_counter_id += n_counters;

  return ret;
}

bool
_sysprof_capture_writer_set_time_range (SysprofCaptureWriter *self,
                                        int64_t               start_time,
                                        int64_t               end_time)
{
  ssize_t ret;

  assert (self != NULL);

do_start:
  ret = _sysprof_pwrite (self->fd,
                         &start_time,
                         sizeof (start_time),
                         offsetof (SysprofCaptureFileHeader, time));

  if (ret < 0 && errno == EAGAIN)
    goto do_start;

do_end:
  ret = _sysprof_pwrite (self->fd,
                         &end_time,
                         sizeof (end_time),
                         offsetof (SysprofCaptureFileHeader, end_time));

  if (ret < 0 && errno == EAGAIN)
    goto do_end;

  return true;
}

SysprofCaptureWriter *
sysprof_capture_writer_new_from_env (size_t buffer_size)
{
  const char *fdstr;
  int fd;

  if (!(fdstr = getenv ("SYSPROF_TRACE_FD")))
    return NULL;

  /* Make sure the clock is initialized */
  sysprof_clock_init ();

  if (!(fd = atoi (fdstr)))
    return NULL;

  /* ignore stdin/stdout/stderr */
  if (fd < 2)
    return NULL;

  return sysprof_capture_writer_new_from_fd (dup (fd), buffer_size);
}

size_t
sysprof_capture_writer_get_buffer_size (SysprofCaptureWriter *self)
{
  assert (self != NULL);

  return self->len;
}

bool
sysprof_capture_writer_add_log (SysprofCaptureWriter *self,
                                int64_t               time,
                                int                   cpu,
                                int32_t               pid,
                                int                   severity,
                                const char           *domain,
                                const char           *message)
{
  SysprofCaptureLog *ev;
  size_t message_len;
  size_t len;

  assert (self != NULL);

  if (domain == NULL)
    domain = "";

  if (message == NULL)
    message = "";
  message_len = strlen (message) + 1;

  len = sizeof *ev + message_len;
  ev = (SysprofCaptureLog *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_LOG);

  ev->severity = severity & 0xFFFF;
  ev->padding1 = 0;
  ev->padding2 = 0;
  _sysprof_strlcpy (ev->domain, domain, sizeof ev->domain);
  memcpy (ev->message, message, message_len);

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_LOG]++;

  return true;
}

bool
sysprof_capture_writer_add_file (SysprofCaptureWriter *self,
                                 int64_t               time,
                                 int                   cpu,
                                 int32_t               pid,
                                 const char           *path,
                                 bool                  is_last,
                                 const uint8_t        *data,
                                 size_t                data_len)
{
  SysprofCaptureFileChunk *ev;
  size_t len;

  assert (self != NULL);

  len = sizeof *ev + data_len;
  ev = (SysprofCaptureFileChunk *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_FILE_CHUNK);

  ev->padding1 = 0;
  ev->is_last = !!is_last;
  ev->len = data_len;
  _sysprof_strlcpy (ev->path, path, sizeof ev->path);
  memcpy (ev->data, data, data_len);

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_FILE_CHUNK]++;

  return true;
}

bool
sysprof_capture_writer_add_file_fd (SysprofCaptureWriter *self,
                                    int64_t               time,
                                    int                   cpu,
                                    int32_t               pid,
                                    const char           *path,
                                    int                   fd)
{
  uint8_t data[(4096*4L) - sizeof(SysprofCaptureFileChunk)];

  assert (self != NULL);

  for (;;)
    {
      bool is_last;
      ssize_t n_read;

    again:
      n_read = read (fd, data, sizeof data);
      if (n_read < 0 && errno == EAGAIN)
        goto again;

      is_last = n_read == 0;

      if (!sysprof_capture_writer_add_file (self, time, cpu, pid, path, is_last, data, n_read))
        return false;

      if (is_last)
        break;
    }

  return true;
}

bool
sysprof_capture_writer_add_allocation (SysprofCaptureWriter  *self,
                                       int64_t                time,
                                       int                    cpu,
                                       int32_t                pid,
                                       int32_t                tid,
                                       SysprofCaptureAddress  alloc_addr,
                                       int64_t                alloc_size,
                                       SysprofBacktraceFunc   backtrace_func,
                                       void                  *backtrace_data)
{
  SysprofCaptureAllocation *ev;
  size_t len;
  int n_addrs;

  assert (self != NULL);
  assert (backtrace_func != NULL);

  len = sizeof *ev + (MAX_UNWIND_DEPTH * sizeof (SysprofCaptureAddress));
  ev = (SysprofCaptureAllocation *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  if ((n_addrs = backtrace_func (ev->addrs, MAX_UNWIND_DEPTH, backtrace_data)) < 0)
    n_addrs = 0;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_ALLOCATION);

  ev->alloc_size = alloc_size;
  ev->alloc_addr = alloc_addr;
  ev->padding1 = 0;
  ev->tid = tid;
  ev->n_addrs = 0;

  if (n_addrs <= MAX_UNWIND_DEPTH)
    ev->n_addrs = n_addrs;

  if (ev->n_addrs < MAX_UNWIND_DEPTH)
    {
      size_t diff = (sizeof (SysprofCaptureAddress) * (MAX_UNWIND_DEPTH - ev->n_addrs));

      ev->frame.len -= diff;
      self->pos -= diff;
    }

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_ALLOCATION]++;

  return true;
}

bool
sysprof_capture_writer_add_allocation_copy (SysprofCaptureWriter        *self,
                                            int64_t                      time,
                                            int                          cpu,
                                            int32_t                      pid,
                                            int32_t                      tid,
                                            SysprofCaptureAddress        alloc_addr,
                                            int64_t                      alloc_size,
                                            const SysprofCaptureAddress *addrs,
                                            unsigned int                 n_addrs)
{
  SysprofCaptureAllocation *ev;
  size_t len;

  assert (self != NULL);

  if (n_addrs > 0xFFF)
    n_addrs = 0xFFF;

  len = sizeof *ev + (n_addrs * sizeof (SysprofCaptureAddress));
  ev = (SysprofCaptureAllocation *)sysprof_capture_writer_allocate (self, &len);
  if (!ev)
    return false;

  sysprof_capture_writer_frame_init (&ev->frame,
                                     len,
                                     cpu,
                                     pid,
                                     time,
                                     SYSPROF_CAPTURE_FRAME_ALLOCATION);

  ev->alloc_size = alloc_size;
  ev->alloc_addr = alloc_addr;
  ev->padding1 = 0;
  ev->tid = tid;
  ev->n_addrs = n_addrs;

  memcpy (ev->addrs, addrs, sizeof (SysprofCaptureAddress) * n_addrs);

  self->stat.frame_count[SYSPROF_CAPTURE_FRAME_ALLOCATION]++;

  return true;
}

bool
_sysprof_capture_writer_add_raw (SysprofCaptureWriter      *self,
                                 const SysprofCaptureFrame *fr)
{
  void *begin;
  size_t len;

  assert (self != NULL);
  assert ((fr->len & 0x7) == 0);
  assert (fr->type < SYSPROF_CAPTURE_FRAME_LAST);

  len = fr->len;

  if (!(begin = sysprof_capture_writer_allocate (self, &len)))
    return false;

  assert (fr->len == len);
  assert (fr->type < SYSPROF_CAPTURE_FRAME_LAST);

  memcpy (begin, fr, fr->len);

  if (fr->type < SYSPROF_N_ELEMENTS (self->stat.frame_count))
    self->stat.frame_count[fr->type]++;

  return true;
}

int
_sysprof_capture_writer_dup_fd (SysprofCaptureWriter *self)
{
  assert (self != NULL);

  if (self->fd == -1)
    return -1;

  return dup (self->fd);
}
