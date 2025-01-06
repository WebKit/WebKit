/* sysprof-capture-cursor.c
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
#include <stdlib.h>

#include "sysprof-capture-condition.h"
#include "sysprof-capture-cursor.h"
#include "sysprof-capture-reader.h"
#include "sysprof-capture-util-private.h"
#include "sysprof-macros-internal.h"

#define READ_DELEGATE(f) ((ReadDelegate)(f))

typedef const SysprofCaptureFrame *(*ReadDelegate) (SysprofCaptureReader *);

struct _SysprofCaptureCursor
{
  volatile int          ref_count;
  SysprofCaptureCondition **conditions;  /* (nullable) (owned) */
  size_t                    n_conditions;
  SysprofCaptureReader *reader;
  unsigned int          reversed : 1;
};

static void
sysprof_capture_cursor_finalize (SysprofCaptureCursor *self)
{
  for (size_t i = 0; i < self->n_conditions; i++)
    sysprof_capture_condition_unref (self->conditions[i]);
  sysprof_clear_pointer (&self->conditions, free);
  sysprof_clear_pointer (&self->reader, sysprof_capture_reader_unref);
  free (self);
}

static SysprofCaptureCursor *
sysprof_capture_cursor_init (void)
{
  SysprofCaptureCursor *self;

  self = sysprof_malloc0 (sizeof (SysprofCaptureCursor));
  if (self == NULL)
    return NULL;

  self->conditions = NULL;
  self->n_conditions = 0;
  self->ref_count = 1;

  return sysprof_steal_pointer (&self);
}

/**
 * sysprof_capture_cursor_ref:
 * @self: a #SysprofCaptureCursor
 *
 * Returns: (transfer full): @self
 *
 * Since: 3.34
 */
SysprofCaptureCursor *
sysprof_capture_cursor_ref (SysprofCaptureCursor *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  __atomic_fetch_add (&self->ref_count, 1, __ATOMIC_SEQ_CST);
  return self;
}

/**
 * sysprof_capture_cursor_unref:
 * @self: a #SysprofCaptureCursor
 *
 * Since: 3.34
 */
void
sysprof_capture_cursor_unref (SysprofCaptureCursor *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  if (__atomic_fetch_sub (&self->ref_count, 1, __ATOMIC_SEQ_CST) == 1)
    sysprof_capture_cursor_finalize (self);
}

/**
 * sysprof_capture_cursor_foreach:
 * @self: a #SysprofCaptureCursor
 * @callback: (scope call): a closure to execute
 * @user_data: user data for @callback
 *
 */
void
sysprof_capture_cursor_foreach (SysprofCaptureCursor         *self,
                                SysprofCaptureCursorCallback  callback,
                                void                         *user_data)
{
  assert (self != NULL);
  assert (callback != NULL);

  if (self->reader == NULL)
    return;

  for (;;)
    {
      const SysprofCaptureFrame *frame;
      SysprofCaptureFrameType type = 0;
      ReadDelegate delegate = NULL;

      if (!sysprof_capture_reader_peek_type (self->reader, &type))
        return;

      switch (type)
        {
        case SYSPROF_CAPTURE_FRAME_TIMESTAMP:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_timestamp);
          break;

        case SYSPROF_CAPTURE_FRAME_SAMPLE:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_sample);
          break;

        case SYSPROF_CAPTURE_FRAME_TRACE:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_trace);
          break;

        case SYSPROF_CAPTURE_FRAME_LOG:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_log);
          break;

        case SYSPROF_CAPTURE_FRAME_MAP:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_map);
          break;

        case SYSPROF_CAPTURE_FRAME_MARK:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_mark);
          break;

        case SYSPROF_CAPTURE_FRAME_PROCESS:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_process);
          break;

        case SYSPROF_CAPTURE_FRAME_FORK:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_fork);
          break;

        case SYSPROF_CAPTURE_FRAME_EXIT:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_exit);
          break;

        case SYSPROF_CAPTURE_FRAME_JITMAP:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_jitmap);
          break;

        case SYSPROF_CAPTURE_FRAME_CTRDEF:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_counter_define);
          break;

        case SYSPROF_CAPTURE_FRAME_CTRSET:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_counter_set);
          break;

        case SYSPROF_CAPTURE_FRAME_METADATA:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_metadata);
          break;

        case SYSPROF_CAPTURE_FRAME_DBUS_MESSAGE:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_dbus_message);
          break;

        case SYSPROF_CAPTURE_FRAME_FILE_CHUNK:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_file);
          break;

        case SYSPROF_CAPTURE_FRAME_ALLOCATION:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_allocation);
          break;

        case SYSPROF_CAPTURE_FRAME_OVERLAY:
          delegate = READ_DELEGATE (sysprof_capture_reader_read_overlay);
          break;

        default:
          if (!sysprof_capture_reader_skip (self->reader))
            return;
          delegate = NULL;
          break;
        }

      if (delegate == NULL)
        continue;

      if (NULL == (frame = delegate (self->reader)))
        return;

      if (self->n_conditions == 0)
        {
          if (!callback (frame, user_data))
            return;
        }
      else
        {
          for (size_t i = 0; i < self->n_conditions; i++)
            {
              const SysprofCaptureCondition *condition = self->conditions[i];

              if (sysprof_capture_condition_match (condition, frame))
                {
                  if (!callback (frame, user_data))
                    return;
                  break;
                }
            }
        }
    }
}

void
sysprof_capture_cursor_reset (SysprofCaptureCursor *self)
{
  assert (self != NULL);

  if (self->reader != NULL)
    sysprof_capture_reader_reset (self->reader);
}

void
sysprof_capture_cursor_reverse (SysprofCaptureCursor *self)
{
  assert (self != NULL);

  self->reversed = !self->reversed;
}

/**
 * sysprof_capture_cursor_add_condition:
 * @self: An #SysprofCaptureCursor
 * @condition: (transfer full): An #SysprofCaptureCondition
 *
 * Adds the condition to the cursor. This condition must evaluate to
 * true or a given #SysprofCapureFrame will not be matched.
 */
void
sysprof_capture_cursor_add_condition (SysprofCaptureCursor    *self,
                                      SysprofCaptureCondition *condition)
{
  assert (self != NULL);
  assert (condition != NULL);

  /* Grow the array linearly to keep the code simple: there are typically 0 or 1
   * conditions applied to a given cursor, just after it’s constructed.
   *
   * FIXME: There’s currently no error reporting from this function, so ENOMEM
   * results in an abort. */
  self->conditions = _sysprof_reallocarray (self->conditions,
                                            ++self->n_conditions,
                                            sizeof (*self->conditions));
  assert (self->conditions != NULL);

  self->conditions[self->n_conditions - 1] = sysprof_steal_pointer (&condition);
}

/**
 * sysprof_capture_cursor_new:
 * @self: a #SysprofCaptureCursor
 *
 * Returns: (transfer full) (nullable): a new cursor for @reader
 */
SysprofCaptureCursor *
sysprof_capture_cursor_new (SysprofCaptureReader *reader)
{
  SysprofCaptureCursor *self;

  self = sysprof_capture_cursor_init ();

  if (reader != NULL)
    {
      self->reader = sysprof_capture_reader_copy (reader);
      sysprof_capture_reader_reset (self->reader);
    }

  return self;
}

/**
 * sysprof_capture_cursor_get_reader:
 *
 * Gets the underlying reader that is used by the cursor.
 *
 * Returns: (transfer none) (nullable): An #SysprofCaptureReader
 */
SysprofCaptureReader *
sysprof_capture_cursor_get_reader (SysprofCaptureCursor *self)
{
  assert (self != NULL);

  return self->reader;
}
