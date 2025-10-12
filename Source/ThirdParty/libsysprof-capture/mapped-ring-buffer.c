/* mapped-ring-buffer.c
 *
 * Copyright 2020-2025 Christian Hergert <chergert@redhat.com>
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
#include <stdint.h>
#include <stdio.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <unistd.h>

#include "sysprof-capture-util-private.h"
#include "sysprof-macros-internal.h"
#include "sysprof-platform.h"

#include "mapped-ring-buffer.h"

#define DEFAULT_N_PAGES 63
#define BUFFER_MAX_SIZE ((UINT32_MAX/2)-_sysprof_getpagesize())
#define SHM_COLOUR 0x00400000

enum {
  MODE_READER    = 1,
  MODE_WRITER    = 2,
  MODE_READWRITE = 3,
};

/*
 * MappedRingHeader is the header of the first page of the
 * buffer. We use the whole buffer so that we can double map
 * the body of the buffer.
 */
typedef struct _MappedRingHeader
{
  uint32_t head;
  uint32_t tail;
  uint32_t offset;
  uint32_t size;
} MappedRingHeader;

SYSPROF_STATIC_ASSERT (sizeof (MappedRingHeader) == 16, "MappedRingHeader changed size");

/*
 * MappedRingBuffer is used to wrap both the reader and writer
 * mapping structures.
 */
struct _MappedRingBuffer
{
  volatile int  ref_count;
  int           mode;
  int           fd;
  void         *map;
  size_t        body_size;
  size_t        page_size;
  unsigned      has_failed : 1;
};

static inline MappedRingHeader *
get_header (MappedRingBuffer *self)
{
  return (MappedRingHeader *)self->map;
}

static inline void *
get_body_at_pos (MappedRingBuffer *self,
                 size_t            pos)
{
  assert (pos < (self->body_size + self->body_size));

  return (uint8_t *)self->map + self->page_size + pos;
}

static void *
map_head_and_body_twice (int    fd,
                         size_t head_size,
                         size_t body_size)
{
  void *map;
  void *second;

  /* First we map FD to the total size we want so that we can be
   * certain the OS will give us a contiguous mapping for our buffers
   * even though we can't dereference a portion of the mapping yet.
   *
   * We'll overwrite the secondary mapping in a moment to deal with
   * wraparound for the ring buffer.
   */
  map = mmap (NULL,
              head_size + body_size + body_size,
              PROT_READ | PROT_WRITE,
              MAP_SHARED,
              fd,
              0);

  if (map == MAP_FAILED)
    return NULL;

  /* At this point, we have [HEAD|BODY|BUSERR] mapped. But we want to map
   * the body again over what would currently cause a BusError. That way
   * when we need to wraparound we don't need to copy anything, we just
   * have to check in mapped_ring_buffer_allocate() that the size does not
   * step over what would be the real reader position.
   *
   * By mmap()'ing over the old region, the previous region is automatically
   * munmap()'d for us.
   */
  second = mmap ((uint8_t *)map + head_size + body_size,
                 body_size,
                 PROT_READ | PROT_WRITE,
                 MAP_SHARED | MAP_FIXED,
                 fd,
                 head_size);

  if (second == MAP_FAILED)
    {
      munmap (map, head_size + body_size + body_size);
      return NULL;
    }

  assert (second == (void *)((uint8_t *)map + head_size + body_size));

  return map;
}

/**
 * mapped_ring_buffer_new_reader:
 * @buffer_size: the size of the buffer, which must be page-aligned
 *
 * Creates a new #MappedRingBuffer.
 *
 * This should be called by the process reading the buffer. It should
 * then pass the FD of the buffer to another process or thread to
 * advance the ring buffer by writing data.
 *
 * The other process or thread should create a new #MappedRingBuffer
 * using mapped_ring_buffer_new_writer() with the FD retrieved from
 * the reader using mapped_ring_buffer_get_fd(). If crossing a process
 * boundary, you probably also want to dup() the FD and set O_CLOEXEC.
 *
 * @buffer_size must be a multiple of the system's page size which can
 * be retrieved using sysprof_getpagesize().
 *
 * Returns: (transfer full): a #MappedRingBuffer
 */
MappedRingBuffer *
mapped_ring_buffer_new_reader (size_t buffer_size)
{
  MappedRingBuffer *self;
  MappedRingHeader *header;
  size_t page_size;
  void *map;
  int fd;

  assert ((buffer_size % _sysprof_getpagesize ()) == 0);
  assert (buffer_size < BUFFER_MAX_SIZE);

  page_size = _sysprof_getpagesize ();

  if (buffer_size == 0)
    buffer_size = page_size * DEFAULT_N_PAGES;

#ifdef __hppa__
  /* Round buffer_size up to the shared memory colour boundary */
  buffer_size += SHM_COLOUR - 1;
  buffer_size &= ~(SHM_COLOUR - 1);
#endif

  /* Add 1 page for coordination header */
  buffer_size += page_size;

  /* Create our memfd (or tmpfs) for writing */
  if ((fd = sysprof_memfd_create ("[sysprof-ring-buffer]")) == -1)
    return NULL;

  /* Size our memfd to reserve space */
  if (ftruncate (fd, buffer_size) != 0)
    {
      close (fd);
      return NULL;
    }

  /* Map ring buffer [HEAD|BODY|BODY] */
  if (!(map = map_head_and_body_twice (fd, page_size, buffer_size - page_size)))
    {
      close (fd);
      return NULL;
    }

  /* Setup initial header */
  header = map;
  header->head = 0;
  header->tail = 0;
  header->offset = page_size;
  header->size = buffer_size - page_size;

  self = sysprof_malloc0 (sizeof (MappedRingBuffer));
  if (self == NULL)
    return NULL;

  self->ref_count = 1;
  self->mode = MODE_READER;
  self->body_size = buffer_size - page_size;
  self->fd = fd;
  self->map = map;
  self->page_size = page_size;

  return sysprof_steal_pointer (&self);
}

MappedRingBuffer *
mapped_ring_buffer_new_readwrite (size_t buffer_size)
{
  MappedRingBuffer *self;

  if ((self = mapped_ring_buffer_new_reader (buffer_size)))
    self->mode = MODE_READWRITE;

  return self;
}

/**
 * mapped_ring_buffer_new_writer:
 * @fd: a FD to map
 *
 * Creates a new #MappedRingBuffer using a copy of @fd.
 *
 * The caller may close(fd) after calling this function regardless of
 * success creating the #MappedRingBuffer.
 *
 * Returns: (transfer full) (nullable): a new #MappedRingBuffer
 */
MappedRingBuffer *
mapped_ring_buffer_new_writer (int fd)
{
  MappedRingBuffer *self;
  MappedRingHeader *header;
  ssize_t buffer_size;
  size_t page_size;
  void *map;

  assert (fd > -1);

  page_size = _sysprof_getpagesize ();

  /* Make our own copy of the FD */
  if ((fd = dup (fd)) < 0)
    {
      fprintf (stderr, "Failed to dup() fd, cannot continue\n");
      return NULL;
    }

  /* Seek to end to get buffer size */
  if ((buffer_size = lseek (fd, 0, SEEK_END)) < 0)
    {
      fprintf (stderr, "Failed to seek to end of file. Cannot determine buffer size.\n");
      return NULL;
    }

  /* Ensure non-zero sized buffer */
  if (buffer_size < (page_size + page_size))
    {
      fprintf (stderr, "Buffer is too small, cannot continue.\n");
      return NULL;
    }

  /* Make sure it is less than our max size */
  if ((buffer_size - page_size) > BUFFER_MAX_SIZE)
    {
      fprintf (stderr, "Buffer is too large, cannot continue.\n");
      return NULL;
    }

  /* Ensure we have page-aligned buffer */
  if ((buffer_size % page_size) != 0)
    {
      fprintf (stderr, "Invalid buffer size, not page aligned.\n");
      return NULL;
    }

  /* Map ring buffer [HEAD|BODY|BODY] */
  if (!(map = map_head_and_body_twice (fd, page_size, buffer_size - page_size)))
    {
      close (fd);
      return NULL;
    }

  /* Validate we got proper data in header */
  header = map;
  if (header->offset != page_size ||
      header->size != (buffer_size - page_size))
    {
      munmap (map, page_size + ((buffer_size - page_size) * 2));
      close (fd);
      return NULL;
    }

  self = sysprof_malloc0 (sizeof (MappedRingBuffer));
  if (self == NULL)
    {
      munmap (map, page_size + ((buffer_size - page_size) * 2));
      close (fd);
      return NULL;
    }

  self->ref_count = 1;
  self->mode = MODE_WRITER;
  self->fd = fd;
  self->body_size = buffer_size - page_size;
  self->map = map;
  self->page_size = page_size;

  return sysprof_steal_pointer (&self);
}

static void
mapped_ring_buffer_finalize (MappedRingBuffer *self)
{
  if (self->map != NULL)
    {
      munmap (self->map, self->page_size + self->body_size + self->body_size);
      self->map = NULL;
    }

  if (self->fd != -1)
    {
      close (self->fd);
      self->fd = -1;
    }

  free (self);
}

void
mapped_ring_buffer_unref (MappedRingBuffer *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  if (__atomic_fetch_sub (&self->ref_count, 1, __ATOMIC_SEQ_CST) == 1)
    mapped_ring_buffer_finalize (self);
}

MappedRingBuffer *
mapped_ring_buffer_ref (MappedRingBuffer *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  __atomic_fetch_add (&self->ref_count, 1, __ATOMIC_SEQ_CST);

  return self;
}

int
mapped_ring_buffer_get_fd (MappedRingBuffer *self)
{
  assert (self != NULL);

  return self->fd;
}

/**
 * mapped_ring_buffer_allocate:
 * @self: a #MappedRingBuffer
 *
 * Ensures that @length bytes are available at the next position in
 * the ring buffer and returns a pointer to the beginning of that zone.
 *
 * If the reader has not read enough bytes to allow @length to be added
 * then a mark will be added or incremented notifying the peer of how
 * many records they have lost and %NULL is returned.
 *
 * You must always check for %NULL before dereferencing the result of
 * this function as space may not be immediately available.
 *
 * This only ensure that the space is available for you to write. To
 * notify the peer that the zone is ready for reading you must call
 * mapped_ring_buffer_advance() with the number of bytes to advance.
 * This is useful in case you need to allocate more memory than you
 * might need up-front but commit a smaller amount.
 *
 * Returns: (nullable): a pointer to data of at least @length bytes
 *   or %NULL if there is not enough space.
 */
void *
mapped_ring_buffer_allocate (MappedRingBuffer *self,
                             size_t            length)
{
  MappedRingHeader *header;
  uint32_t headpos;
  uint32_t tailpos;

  assert (self != NULL);
  assert (self->mode & MODE_WRITER);
  assert (length > 0);
  assert (length < self->body_size);
  assert ((length & 0x7) == 0);

  for (unsigned i = 0; i < 1000; i++)
    {
      header = get_header (self);
      __atomic_load (&header->head, &headpos, __ATOMIC_SEQ_CST);
      __atomic_load (&header->tail, &tailpos, __ATOMIC_SEQ_CST);

      /* We need to check that there is enough space for @length at the
       * current position in the write buffer. We cannot fully catch up
       * to head, we must be at least one byte short of it. If we do not
       * have enough space, then return NULL.
       *
       * When we have finished writing our frame data, we will push the tail
       * forward with an atomic write.
       */

      if (tailpos == headpos)
        return get_body_at_pos (self, tailpos);

      if (headpos < tailpos)
        headpos += self->body_size;

      if (tailpos + length < headpos)
        return get_body_at_pos (self, tailpos);

      if (self->has_failed)
        break;

      usleep (1000); /* 1 msec */
    }

  self->has_failed = true;

  return NULL;
}

/**
 * mapped_ring_buffer_advance:
 * @self: a #MappedRingBuffer
 * @length: a 8-byte aligned length
 *
 * Advances the ring buffer @length bytes forward. @length must be
 * 8-byte aligned so that the buffer may avoid memcpy() to read
 * framing data.
 *
 * This should only be called by a writer created with
 * mapped_ring_buffer_new_writer().
 *
 * Call this after writing your data into the buffer using
 * mapped_ring_buffer_allocate().
 *
 * It is a programming error to call this with a value greater
 * than was called to mapped_ring_buffer_allocate().
 */
void
mapped_ring_buffer_advance (MappedRingBuffer *self,
                            size_t            length)
{
  MappedRingHeader *header;
  uint32_t tail;

  assert (self != NULL);
  assert (self->mode & MODE_WRITER);
  assert (length > 0);
  assert (length < self->body_size);
  assert ((length & 0x7) == 0);

  header = get_header (self);
  tail = header->tail;

  /* Calculate the new tail position */
  tail = tail + length;
  if (tail >= self->body_size)
    tail -= self->body_size;

  /* We have already checked that we could advance the buffer when the
   * application called mapped_ring_buffer_allocate(), so at this point
   * we just update the position as the only way the head could have
   * moved is forward.
   */
  __atomic_store (&header->tail, &tail, __ATOMIC_SEQ_CST);
}

/**
 * mapped_ring_buffer_drain:
 * @self: a #MappedRingBuffer
 * @callback: (scope call): a callback to execute for each frame
 * @user_data: closure data for @callback
 *
 * Drains the buffer by calling @callback for each frame.
 *
 * This should only be called by a reader created with
 * mapped_ring_buffer_new_reader().
 *
 * Returns: %TRUE if the buffer was drained, %FALSE if @callback prematurely
 *   returned while draining.
 */
bool
mapped_ring_buffer_drain (MappedRingBuffer         *self,
                          MappedRingBufferCallback  callback,
                          void                     *user_data)
{
  MappedRingHeader *header;
  uint32_t headpos;
  uint32_t tailpos;

  assert (self != NULL);
  assert (self->mode & MODE_READER);
  assert (callback != NULL);

  header = get_header (self);
  __atomic_load (&header->head, &headpos, __ATOMIC_SEQ_CST);
  __atomic_load (&header->tail, &tailpos, __ATOMIC_SEQ_CST);

  assert (headpos < self->body_size);
  assert (tailpos < self->body_size);

  if (headpos == tailpos)
    return true;

  /* If head needs to wrap around to get to tail, we can just rely on
   * our double mapping instead actually manually wrapping/copying data.
   */
  if (tailpos < headpos)
    tailpos += self->body_size;

  assert (headpos < tailpos);

  while (headpos < tailpos)
    {
      const void *data = get_body_at_pos (self, headpos);
      size_t len = tailpos - headpos;
      uint32_t new_headpos;

      if (!callback (data, &len, user_data))
        return false;

      if (len > (tailpos - headpos))
        return false;

      headpos += len;

      if (headpos >= self->body_size)
        new_headpos = headpos - self->body_size;
      else
        new_headpos = headpos;

      __atomic_store (&header->head, &new_headpos, __ATOMIC_SEQ_CST);
    }

  return true;
}

/**
 * mapped_ring_buffer_is_empty:
 * @self: a #MappedRingBuffer
 *
 * Checks whether the ring buffer is currently empty.
 *
 * This should only be called by a reader created with
 * mapped_ring_buffer_new_reader().
 *
 * Returns: %TRUE if the buffer is empty, %FALSE otherwise
 */
bool
mapped_ring_buffer_is_empty (MappedRingBuffer *self)
{
  MappedRingHeader *header;
  uint32_t headpos, tailpos;

  header = get_header (self);

  __atomic_load (&header->head, &headpos, __ATOMIC_SEQ_CST);
  __atomic_load (&header->tail, &tailpos, __ATOMIC_SEQ_CST);

  return headpos == tailpos;
}

/**
 * mapped_ring_buffer_clear:
 * @self: a #MappedRingBuffer
 *
 * Resets the head and tail positions back to 0.
 *
 * This function is only safe to call when you control both the reader
 * and writer sides with mapped_ring_buffer_new_readwrite(), or are in
 * control of when each side reads or writes.
 */
void
mapped_ring_buffer_clear (MappedRingBuffer *self)
{
  MappedRingHeader *header;

  assert (self != NULL);

  header = get_header (self);
  header->head = 0;
  header->tail = 0;
}
