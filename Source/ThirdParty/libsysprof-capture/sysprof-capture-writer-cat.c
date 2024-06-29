/* sysprof-capture-writer-cat.c
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
#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>

#include "sysprof-capture.h"
#include "sysprof-capture-util-private.h"
#include "sysprof-macros-internal.h"

typedef struct
{
  uint64_t src;
  uint64_t dst;
} TranslateItem;

typedef struct
{
  TranslateItem *items;
  size_t n_items;
  size_t n_items_allocated;
} TranslateTable;

enum {
  TRANSLATE_ADDR,
  TRANSLATE_CTR,
  N_TRANSLATE
};

static void
translate_table_clear (TranslateTable *tables,
                       unsigned int    table)
{
  TranslateTable *table_ptr = &tables[table];

  sysprof_clear_pointer (&table_ptr->items, free);
  table_ptr->n_items = 0;
  table_ptr->n_items_allocated = 0;
}

static int
compare_by_src (const void *a,
                const void *b)
{
  const TranslateItem *itema = a;
  const TranslateItem *itemb = b;

  if (itema->src < itemb->src)
    return -1;
  else if (itema->src > itemb->src)
    return 1;
  else
    return 0;
}

static void
translate_table_sort (TranslateTable *tables,
                      unsigned int    table)
{
  TranslateTable *table_ptr = &tables[table];

  if (table_ptr->items)
    qsort (table_ptr->items, table_ptr->n_items, sizeof (*table_ptr->items), compare_by_src);
}

static void
translate_table_add (TranslateTable *tables,
                     unsigned int    table,
                     uint64_t        src,
                     uint64_t        dst)
{
  TranslateTable *table_ptr = &tables[table];
  const TranslateItem item = { src, dst };

  if (table_ptr->n_items == table_ptr->n_items_allocated)
    {
      table_ptr->n_items_allocated = (table_ptr->n_items_allocated > 0) ? table_ptr->n_items_allocated * 2 : 4;
      table_ptr->items = _sysprof_reallocarray (table_ptr->items, table_ptr->n_items_allocated, sizeof (*table_ptr->items));
      assert (table_ptr->items != NULL);
    }

  table_ptr->items[table_ptr->n_items++] = item;
  assert (table_ptr->n_items <= table_ptr->n_items_allocated);
}

static uint64_t
translate_table_translate (TranslateTable *tables,
                           unsigned int    table,
                           uint64_t        src)
{
  TranslateTable *table_ptr = &tables[table];
  const TranslateItem *item;
  TranslateItem key = { src, 0 };

  if (table == TRANSLATE_ADDR)
    {
      if ((src & SYSPROF_CAPTURE_JITMAP_MARK) == 0)
        return src;
    }

  if (table_ptr->items == NULL)
    return src;

  item = bsearch (&key,
                  table_ptr->items,
                  table_ptr->n_items,
                  sizeof (*table_ptr->items),
                  compare_by_src);

  return item != NULL ? item->dst : src;
}

bool
sysprof_capture_writer_cat (SysprofCaptureWriter *self,
                            SysprofCaptureReader *reader)
{
  TranslateTable tables[N_TRANSLATE] = { 0, };
  SysprofCaptureFrameType type;
  int64_t start_time;
  int64_t first_start_time = INT64_MAX;
  int64_t end_time = -1;

  assert (self != NULL);
  assert (reader != NULL);

  sysprof_capture_reader_reset (reader);

  translate_table_clear (tables, TRANSLATE_CTR);
  translate_table_clear (tables, TRANSLATE_ADDR);

  start_time = sysprof_capture_reader_get_start_time (reader);

  if (start_time < first_start_time)
    first_start_time = start_time;

  /* First we need to find all the JIT maps so that we can translate
   * addresses later on and have the correct value.
   */
  while (sysprof_capture_reader_peek_type (reader, &type))
    {
      const SysprofCaptureJitmap *jitmap;
      SysprofCaptureJitmapIter iter;
      SysprofCaptureAddress addr;
      const char *name;

      if (type != SYSPROF_CAPTURE_FRAME_JITMAP)
        {
          if (!sysprof_capture_reader_skip (reader))
            goto panic;
          continue;
        }

      if (!(jitmap = sysprof_capture_reader_read_jitmap (reader)))
        goto panic;

      sysprof_capture_jitmap_iter_init (&iter, jitmap);
      while (sysprof_capture_jitmap_iter_next (&iter, &addr, &name))
        {
          uint64_t replace = sysprof_capture_writer_add_jitmap (self, name);
          /* We need to keep a table of replacement addresses so that
           * we can translate the samples into the destination address
           * space that we synthesized for the address identifier.
           */
          translate_table_add (tables, TRANSLATE_ADDR, addr, replace);
        }
    }

  translate_table_sort (tables, TRANSLATE_ADDR);

  sysprof_capture_reader_reset (reader);

  while (sysprof_capture_reader_peek_type (reader, &type))
    {
      SysprofCaptureFrame fr;

      if (sysprof_capture_reader_peek_frame (reader, &fr))
        {
          if (fr.time > end_time)
            end_time = fr.time;
        }

      switch (type)
        {
        case SYSPROF_CAPTURE_FRAME_TIMESTAMP:
          {
            const SysprofCaptureTimestamp *frame;

            if (!(frame = sysprof_capture_reader_read_timestamp (reader)))
              goto panic;

            sysprof_capture_writer_add_timestamp (self,
                                                  frame->frame.time,
                                                  frame->frame.cpu,
                                                  frame->frame.pid);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_FILE_CHUNK:
          {
            const SysprofCaptureFileChunk *frame;

            if (!(frame = sysprof_capture_reader_read_file (reader)))
              goto panic;

            sysprof_capture_writer_add_file (self,
                                             frame->frame.time,
                                             frame->frame.cpu,
                                             frame->frame.pid,
                                             frame->path,
                                             frame->is_last,
                                             frame->data,
                                             frame->len);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_LOG:
          {
            const SysprofCaptureLog *frame;

            if (!(frame = sysprof_capture_reader_read_log (reader)))
              goto panic;

            sysprof_capture_writer_add_log (self,
                                            frame->frame.time,
                                            frame->frame.cpu,
                                            frame->frame.pid,
                                            frame->severity,
                                            frame->domain,
                                            frame->message);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_MAP:
          {
            const SysprofCaptureMap *frame;

            if (!(frame = sysprof_capture_reader_read_map (reader)))
              goto panic;

            sysprof_capture_writer_add_map (self,
                                            frame->frame.time,
                                            frame->frame.cpu,
                                            frame->frame.pid,
                                            frame->start,
                                            frame->end,
                                            frame->offset,
                                            frame->inode,
                                            frame->filename);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_MARK:
          {
            const SysprofCaptureMark *frame;

            if (!(frame = sysprof_capture_reader_read_mark (reader)))
              goto panic;

            sysprof_capture_writer_add_mark (self,
                                             frame->frame.time,
                                             frame->frame.cpu,
                                             frame->frame.pid,
                                             frame->duration,
                                             frame->group,
                                             frame->name,
                                             frame->message);

            if (frame->frame.time + frame->duration > end_time)
              end_time = frame->frame.time + frame->duration;

            break;
          }

        case SYSPROF_CAPTURE_FRAME_PROCESS:
          {
            const SysprofCaptureProcess *frame;

            if (!(frame = sysprof_capture_reader_read_process (reader)))
              goto panic;

            sysprof_capture_writer_add_process (self,
                                                frame->frame.time,
                                                frame->frame.cpu,
                                                frame->frame.pid,
                                                frame->cmdline);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_FORK:
          {
            const SysprofCaptureFork *frame;

            if (!(frame = sysprof_capture_reader_read_fork (reader)))
              goto panic;

            sysprof_capture_writer_add_fork (self,
                                             frame->frame.time,
                                             frame->frame.cpu,
                                             frame->frame.pid,
                                             frame->child_pid);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_EXIT:
          {
            const SysprofCaptureExit *frame;

            if (!(frame = sysprof_capture_reader_read_exit (reader)))
              goto panic;

            sysprof_capture_writer_add_exit (self,
                                             frame->frame.time,
                                             frame->frame.cpu,
                                             frame->frame.pid);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_METADATA:
          {
            const SysprofCaptureMetadata *frame;

            if (!(frame = sysprof_capture_reader_read_metadata (reader)))
              goto panic;

            sysprof_capture_writer_add_metadata (self,
                                                 frame->frame.time,
                                                 frame->frame.cpu,
                                                 frame->frame.pid,
                                                 frame->id,
                                                 frame->metadata,
                                                 frame->frame.len - offsetof (SysprofCaptureMetadata, metadata));
            break;
          }

        case SYSPROF_CAPTURE_FRAME_DBUS_MESSAGE:
          {
            const SysprofCaptureDBusMessage *frame;

            if (!(frame = sysprof_capture_reader_read_dbus_message (reader)))
              goto panic;

            sysprof_capture_writer_add_dbus_message (self,
                                                     frame->frame.time,
                                                     frame->frame.cpu,
                                                     frame->frame.pid,
                                                     frame->bus_type,
                                                     frame->flags,
                                                     frame->message,
                                                     frame->message_len);
            break;
          }

        case SYSPROF_CAPTURE_FRAME_SAMPLE:
          {
            const SysprofCaptureSample *frame;

            if (!(frame = sysprof_capture_reader_read_sample (reader)))
              goto panic;

            {
              SysprofCaptureAddress addrs[frame->n_addrs];

              for (unsigned int z = 0; z < frame->n_addrs; z++)
                addrs[z] = translate_table_translate (tables, TRANSLATE_ADDR, frame->addrs[z]);

              sysprof_capture_writer_add_sample (self,
                                                 frame->frame.time,
                                                 frame->frame.cpu,
                                                 frame->frame.pid,
                                                 frame->tid,
                                                 addrs,
                                                 frame->n_addrs);
            }

            break;
          }

        case SYSPROF_CAPTURE_FRAME_TRACE:
          {
            const SysprofCaptureTrace *frame;

            if (!(frame = sysprof_capture_reader_read_trace (reader)))
              goto panic;

            {
              SysprofCaptureAddress addrs[frame->n_addrs];

              for (unsigned int z = 0; z < frame->n_addrs; z++)
                addrs[z] = translate_table_translate (tables, TRANSLATE_ADDR, frame->addrs[z]);

              sysprof_capture_writer_add_trace (self,
                                                frame->frame.time,
                                                frame->frame.cpu,
                                                frame->frame.pid,
                                                frame->tid,
                                                addrs,
                                                frame->n_addrs,
                                                frame->entering);
            }

            break;
          }

        case SYSPROF_CAPTURE_FRAME_CTRDEF:
          {
            const SysprofCaptureCounterDefine *frame;

            if (!(frame = sysprof_capture_reader_read_counter_define (reader)))
              goto panic;

            {
              SysprofCaptureCounter *counters = calloc (frame->n_counters, sizeof (*counters));
              size_t n_counters = 0;
              if (counters == NULL)
                goto panic;

              for (unsigned int z = 0; z < frame->n_counters; z++)
                {
                  SysprofCaptureCounter c = frame->counters[z];
                  unsigned int src = c.id;

                  c.id = sysprof_capture_writer_request_counter (self, 1);

                  if (c.id != src)
                    translate_table_add (tables, TRANSLATE_CTR, src, c.id);

                  counters[n_counters++] = c;
                }

              sysprof_capture_writer_define_counters (self,
                                                      frame->frame.time,
                                                      frame->frame.cpu,
                                                      frame->frame.pid,
                                                      counters,
                                                      n_counters);

              translate_table_sort (tables, TRANSLATE_CTR);
            }

            break;
          }

        case SYSPROF_CAPTURE_FRAME_CTRSET:
          {
            const SysprofCaptureCounterSet *frame;

            if (!(frame = sysprof_capture_reader_read_counter_set (reader)))
              goto panic;

            {
              unsigned int *ids = NULL;
              SysprofCaptureCounterValue *values = NULL;
              size_t n_elements = 0;
              size_t n_elements_allocated = 0;

              for (unsigned int z = 0; z < frame->n_values; z++)
                {
                  const SysprofCaptureCounterValues *v = &frame->values[z];

                  for (unsigned int y = 0; y < SYSPROF_N_ELEMENTS (v->ids); y++)
                    {
                      if (v->ids[y])
                        {
                          unsigned int dst = translate_table_translate (tables, TRANSLATE_CTR, v->ids[y]);
                          SysprofCaptureCounterValue value = v->values[y];

                          if (n_elements == n_elements_allocated)
                            {
                              n_elements_allocated = (n_elements_allocated > 0) ? n_elements_allocated * 2 : 4;
                              ids = _sysprof_reallocarray (ids, n_elements_allocated, sizeof (*ids));
                              values = _sysprof_reallocarray (values, n_elements_allocated, sizeof (*values));
                              if (ids == NULL || values == NULL)
                                goto panic;
                            }

                          ids[n_elements] = dst;
                          values[n_elements] = value;
                          n_elements++;
                          assert (n_elements <= n_elements_allocated);
                        }
                    }
                }

              sysprof_capture_writer_set_counters (self,
                                                   frame->frame.time,
                                                   frame->frame.cpu,
                                                   frame->frame.pid,
                                                   ids,
                                                   values,
                                                   n_elements);
            }

            break;
          }

        case SYSPROF_CAPTURE_FRAME_JITMAP:
          /* We already did this */
          if (!sysprof_capture_reader_skip (reader))
            goto panic;
          break;

        case SYSPROF_CAPTURE_FRAME_ALLOCATION: {
          const SysprofCaptureAllocation *frame;

          if (!(frame = sysprof_capture_reader_read_allocation (reader)))
            goto panic;

          sysprof_capture_writer_add_allocation_copy (self,
                                                      frame->frame.time,
                                                      frame->frame.cpu,
                                                      frame->frame.pid,
                                                      frame->tid,
                                                      frame->alloc_addr,
                                                      frame->alloc_size,
                                                      frame->addrs,
                                                      frame->n_addrs);
          break;
        }

        case SYSPROF_CAPTURE_FRAME_OVERLAY:
          {
            const SysprofCaptureOverlay *frame;
            const char *src;
            const char *dst;

            if (!(frame = sysprof_capture_reader_read_overlay (reader)))
              goto panic;

            /* This should have been verified alrady when decoding */
            assert (frame->frame.len >= (sizeof *frame + frame->src_len + 1 + frame->dst_len + 1));

            src = &frame->data[0];
            dst = &frame->data[frame->src_len+1];

            sysprof_capture_writer_add_overlay (self,
                                                frame->frame.time,
                                                frame->frame.cpu,
                                                frame->frame.pid,
                                                frame->layer,
                                                src,
                                                dst);
            break;
          }

        default:
          /* Silently drop, which is better than looping. We could potentially
           * copy this over using the raw bytes at some point.
           */
          sysprof_capture_reader_skip (reader);
          break;
        }
    }

  sysprof_capture_writer_flush (self);

  /* do this after flushing as it uses pwrite() to replace data */
  _sysprof_capture_writer_set_time_range (self, first_start_time, end_time);

  translate_table_clear (tables, TRANSLATE_ADDR);
  translate_table_clear (tables, TRANSLATE_CTR);

  return true;

panic:
  translate_table_clear (tables, TRANSLATE_ADDR);
  translate_table_clear (tables, TRANSLATE_CTR);

  errno = EIO;
  return false;
}
