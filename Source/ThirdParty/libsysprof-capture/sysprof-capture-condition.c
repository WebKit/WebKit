/* sysprof-capture-condition.c
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
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "sysprof-capture-condition.h"
#include "sysprof-capture-util-private.h"
#include "sysprof-macros-internal.h"

/**
 * SECTION:sysprof-capture-condition
 * @title: SysprofCaptureCondition
 *
 * The #SysprofCaptureCondition type is an abstraction on an operation
 * for a sort of AST to the #SysprofCaptureCursor. The goal is that if
 * we abstract the types of fields we want to match in the cursor
 * that we can opportunistically add indexes to speed up the operation
 * later on without changing the implementation of cursor consumers.
 */

typedef enum
{
  SYSPROF_CAPTURE_CONDITION_AND,
  SYSPROF_CAPTURE_CONDITION_OR,
  SYSPROF_CAPTURE_CONDITION_WHERE_TYPE_IN,
  SYSPROF_CAPTURE_CONDITION_WHERE_TIME_BETWEEN,
  SYSPROF_CAPTURE_CONDITION_WHERE_PID_IN,
  SYSPROF_CAPTURE_CONDITION_WHERE_COUNTER_IN,
  SYSPROF_CAPTURE_CONDITION_WHERE_FILE,
} SysprofCaptureConditionType;

struct _SysprofCaptureCondition
{
  volatile int ref_count;
  SysprofCaptureConditionType type;
  union {
    struct {
      SysprofCaptureFrameType *data;
      size_t len;
    } where_type_in;
    struct {
      int64_t begin;
      int64_t end;
    } where_time_between;
    struct {
      int32_t *data;
      size_t len;
    } where_pid_in;
    struct {
      unsigned int *data;
      size_t len;
    } where_counter_in;
    struct {
      SysprofCaptureCondition *left;
      SysprofCaptureCondition *right;
    } and, or;
    char *where_file;
  } u;
};

bool
sysprof_capture_condition_match (const SysprofCaptureCondition *self,
                                 const SysprofCaptureFrame     *frame)
{
  assert (self != NULL);
  assert (frame != NULL);

  switch (self->type)
    {
    case SYSPROF_CAPTURE_CONDITION_AND:
      return sysprof_capture_condition_match (self->u.and.left, frame) &&
             sysprof_capture_condition_match (self->u.and.right, frame);

    case SYSPROF_CAPTURE_CONDITION_OR:
      return sysprof_capture_condition_match (self->u.or.left, frame) ||
             sysprof_capture_condition_match (self->u.or.right, frame);

    case SYSPROF_CAPTURE_CONDITION_WHERE_TYPE_IN:
      for (size_t i = 0; i < self->u.where_type_in.len; i++)
        {
          if (frame->type == self->u.where_type_in.data[i])
            return true;
        }
      return false;

    case SYSPROF_CAPTURE_CONDITION_WHERE_TIME_BETWEEN:
      return (frame->time >= self->u.where_time_between.begin && frame->time <= self->u.where_time_between.end);

    case SYSPROF_CAPTURE_CONDITION_WHERE_PID_IN:
      for (size_t i = 0; i < self->u.where_pid_in.len; i++)
        {
          if (frame->pid == self->u.where_pid_in.data[i])
            return true;
        }
      return false;

    case SYSPROF_CAPTURE_CONDITION_WHERE_COUNTER_IN:
      if (frame->type == SYSPROF_CAPTURE_FRAME_CTRSET)
        {
          const SysprofCaptureCounterSet *set = (SysprofCaptureCounterSet *)frame;

          for (size_t i = 0; i < self->u.where_counter_in.len; i++)
            {
              unsigned int counter = self->u.where_counter_in.data[i];

              for (unsigned int j = 0; j < set->n_values; j++)
                {
                  if (counter == set->values[j].ids[0] ||
                      counter == set->values[j].ids[1] ||
                      counter == set->values[j].ids[2] ||
                      counter == set->values[j].ids[3] ||
                      counter == set->values[j].ids[4] ||
                      counter == set->values[j].ids[5] ||
                      counter == set->values[j].ids[6] ||
                      counter == set->values[j].ids[7])
                    return true;
                }
            }
        }
      else if (frame->type == SYSPROF_CAPTURE_FRAME_CTRDEF)
        {
          const SysprofCaptureCounterDefine *def = (SysprofCaptureCounterDefine *)frame;

          for (size_t i = 0; i < self->u.where_counter_in.len; i++)
            {
              unsigned int counter = self->u.where_counter_in.data[i];

              for (unsigned int j = 0; j < def->n_counters; j++)
                {
                  if (def->counters[j].id == counter)
                    return true;
                }
            }
        }

      return false;

    case SYSPROF_CAPTURE_CONDITION_WHERE_FILE:
      if (frame->type != SYSPROF_CAPTURE_FRAME_FILE_CHUNK)
        return false;

      if (self->u.where_file == NULL)
        return false;

      return strcmp (((const SysprofCaptureFileChunk *)frame)->path, self->u.where_file) == 0;

    default:
      break;
    }

  sysprof_assert_not_reached ();

  return false;
}

static SysprofCaptureCondition *
sysprof_capture_condition_init (void)
{
  SysprofCaptureCondition *self;

  self = sysprof_malloc0 (sizeof (SysprofCaptureCondition));
  if (self == NULL)
    return NULL;

  self->ref_count = 1;

  return sysprof_steal_pointer (&self);
}

/* Returns NULL on allocation failure. */
SysprofCaptureCondition *
sysprof_capture_condition_copy (const SysprofCaptureCondition *self)
{
  switch (self->type)
    {
    case SYSPROF_CAPTURE_CONDITION_AND:
      return sysprof_capture_condition_new_and (
        sysprof_capture_condition_copy (self->u.and.left),
        sysprof_capture_condition_copy (self->u.and.right));

    case SYSPROF_CAPTURE_CONDITION_OR:
      return sysprof_capture_condition_new_or (
        sysprof_capture_condition_copy (self->u.or.left),
        sysprof_capture_condition_copy (self->u.or.right));

    case SYSPROF_CAPTURE_CONDITION_WHERE_TYPE_IN:
      return sysprof_capture_condition_new_where_type_in (
          self->u.where_type_in.len,
          self->u.where_type_in.data);

    case SYSPROF_CAPTURE_CONDITION_WHERE_TIME_BETWEEN:
      return sysprof_capture_condition_new_where_time_between (
        self->u.where_time_between.begin,
        self->u.where_time_between.end);

    case SYSPROF_CAPTURE_CONDITION_WHERE_PID_IN:
      return sysprof_capture_condition_new_where_pid_in (
          self->u.where_pid_in.len,
          self->u.where_pid_in.data);

    case SYSPROF_CAPTURE_CONDITION_WHERE_COUNTER_IN:
      return sysprof_capture_condition_new_where_counter_in (
          self->u.where_counter_in.len,
          self->u.where_counter_in.data);

    case SYSPROF_CAPTURE_CONDITION_WHERE_FILE:
      return sysprof_capture_condition_new_where_file (self->u.where_file);

    default:
      break;
    }

  sysprof_assert_not_reached ();
  return NULL;
}

static void
sysprof_capture_condition_finalize (SysprofCaptureCondition *self)
{
  switch (self->type)
    {
    case SYSPROF_CAPTURE_CONDITION_AND:
      sysprof_capture_condition_unref (self->u.and.left);
      sysprof_capture_condition_unref (self->u.and.right);
      break;

    case SYSPROF_CAPTURE_CONDITION_OR:
      sysprof_capture_condition_unref (self->u.or.left);
      sysprof_capture_condition_unref (self->u.or.right);
      break;

    case SYSPROF_CAPTURE_CONDITION_WHERE_TYPE_IN:
      free (self->u.where_type_in.data);
      break;

    case SYSPROF_CAPTURE_CONDITION_WHERE_TIME_BETWEEN:
      break;

    case SYSPROF_CAPTURE_CONDITION_WHERE_PID_IN:
      free (self->u.where_pid_in.data);
      break;

    case SYSPROF_CAPTURE_CONDITION_WHERE_COUNTER_IN:
      free (self->u.where_counter_in.data);
      break;

    case SYSPROF_CAPTURE_CONDITION_WHERE_FILE:
      free (self->u.where_file);
      break;

    default:
      sysprof_assert_not_reached ();
      break;
    }

  free (self);
}

SysprofCaptureCondition *
sysprof_capture_condition_ref (SysprofCaptureCondition *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  __atomic_fetch_add (&self->ref_count, 1, __ATOMIC_SEQ_CST);
  return self;
}

void
sysprof_capture_condition_unref (SysprofCaptureCondition *self)
{
  assert (self != NULL);
  assert (self->ref_count > 0);

  if (__atomic_fetch_sub (&self->ref_count, 1, __ATOMIC_SEQ_CST) == 1)
    sysprof_capture_condition_finalize (self);
}

/* Returns NULL on allocation failure. */
SysprofCaptureCondition *
sysprof_capture_condition_new_where_type_in (unsigned int                   n_types,
                                             const SysprofCaptureFrameType *types)
{
  SysprofCaptureCondition *self;

  assert (types != NULL);

  self = sysprof_capture_condition_init ();
  if (self == NULL)
    return NULL;

  self->type = SYSPROF_CAPTURE_CONDITION_WHERE_TYPE_IN;
  self->u.where_type_in.data = calloc (n_types, sizeof (SysprofCaptureFrameType));
  if (self->u.where_type_in.data == NULL)
    return NULL;
  self->u.where_type_in.len = n_types;
  memcpy (self->u.where_type_in.data, types, sizeof (SysprofCaptureFrameType) * n_types);

  return self;
}

/* Returns NULL on allocation failure. */
SysprofCaptureCondition *
sysprof_capture_condition_new_where_time_between (int64_t begin_time,
                                                  int64_t end_time)
{
  SysprofCaptureCondition *self;

  if SYSPROF_UNLIKELY (begin_time > end_time)
    {
      int64_t tmp = begin_time;
      begin_time = end_time;
      end_time = tmp;
    }

  self = sysprof_capture_condition_init ();
  if (self == NULL)
    return NULL;

  self->type = SYSPROF_CAPTURE_CONDITION_WHERE_TIME_BETWEEN;
  self->u.where_time_between.begin = begin_time;
  self->u.where_time_between.end = end_time;

  return self;
}

/* Returns NULL on allocation failure. */
SysprofCaptureCondition *
sysprof_capture_condition_new_where_pid_in (unsigned int   n_pids,
                                            const int32_t *pids)
{
  SysprofCaptureCondition *self;

  assert (pids != NULL);

  self = sysprof_capture_condition_init ();
  if (self == NULL)
    return NULL;

  self->type = SYSPROF_CAPTURE_CONDITION_WHERE_PID_IN;
  self->u.where_pid_in.data = calloc (n_pids, sizeof (int32_t));
  if (self->u.where_pid_in.data == NULL)
    {
      free (self);
      return NULL;
    }
  self->u.where_pid_in.len = n_pids;
  memcpy (self->u.where_pid_in.data, pids, sizeof (int32_t) * n_pids);

  return self;
}

/* Returns NULL on allocation failure. */
SysprofCaptureCondition *
sysprof_capture_condition_new_where_counter_in (unsigned int        n_counters,
                                                const unsigned int *counters)
{
  SysprofCaptureCondition *self;

  assert (counters != NULL || n_counters == 0);

  self = sysprof_capture_condition_init ();
  if (self == NULL)
    return NULL;

  self->type = SYSPROF_CAPTURE_CONDITION_WHERE_COUNTER_IN;
  self->u.where_counter_in.data = calloc (n_counters, sizeof (unsigned int));
  if (n_counters > 0 && self->u.where_counter_in.data == NULL)
    {
      free (self);
      return NULL;
    }
  self->u.where_counter_in.len = n_counters;

  if (n_counters > 0)
    memcpy (self->u.where_counter_in.data, counters, sizeof (unsigned int) * n_counters);

  return self;
}

/**
 * sysprof_capture_condition_new_and:
 * @left: (transfer full): An #SysprofCaptureCondition
 * @right: (transfer full): An #SysprofCaptureCondition
 *
 * Creates a new #SysprofCaptureCondition that requires both left and right
 * to evaluate to %TRUE.
 *
 * Returns: (transfer full) (nullable): A new #SysprofCaptureCondition, or %NULL
 *    on allocation failure.
 */
SysprofCaptureCondition *
sysprof_capture_condition_new_and (SysprofCaptureCondition *left,
                                   SysprofCaptureCondition *right)
{
  SysprofCaptureCondition *self;

  assert (left != NULL);
  assert (right != NULL);

  self = sysprof_capture_condition_init ();
  if (self == NULL)
    return NULL;

  self->type = SYSPROF_CAPTURE_CONDITION_AND;
  self->u.and.left = left;
  self->u.and.right = right;

  return self;
}

/**
 * sysprof_capture_condition_new_or:
 * @left: (transfer full): An #SysprofCaptureCondition
 * @right: (transfer full): An #SysprofCaptureCondition
 *
 * Creates a new #SysprofCaptureCondition that requires either left and right
 * to evaluate to %TRUE.
 *
 * Returns: (transfer full) (nullable): A new #SysprofCaptureCondition, or %NULL
 *    on allocation failure.
 */
SysprofCaptureCondition *
sysprof_capture_condition_new_or (SysprofCaptureCondition *left,
                                  SysprofCaptureCondition *right)
{
  SysprofCaptureCondition *self;

  assert (left != NULL);
  assert (right != NULL);

  self = sysprof_capture_condition_init ();
  if (self == NULL)
    return NULL;

  self->type = SYSPROF_CAPTURE_CONDITION_OR;
  self->u.or.left = left;
  self->u.or.right = right;

  return self;
}

/**
 * sysprof_capture_condition_new_where_file:
 * @path: a file path to lookup
 *
 * Creates a new condition that matches #SysprofCaptureFileChunk frames
 * which contain the path @path.
 *
 * Returns: (transfer full) (nullable): a new #SysprofCaptureCondition, or %NULL
 *    on allocation failure.
 */
SysprofCaptureCondition *
sysprof_capture_condition_new_where_file (const char *path)
{
  SysprofCaptureCondition *self;

  assert (path != NULL);

  self = sysprof_capture_condition_init ();
  if (self == NULL)
    return NULL;

  self->type = SYSPROF_CAPTURE_CONDITION_WHERE_FILE;
  self->u.where_file = sysprof_strdup (path);
  if (self->u.where_file == NULL)
    {
      free (self);
      return NULL;
    }

  return self;
}
