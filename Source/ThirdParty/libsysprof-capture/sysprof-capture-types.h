/* sysprof-capture-types.h
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

#pragma once

#include <assert.h>
#ifdef __APPLE__
# include <machine/endian.h>
#else
# include <endian.h>
#endif
#include <inttypes.h>
#include <stddef.h>
#include <stdint.h>

#include "sysprof-clock.h"
#include "sysprof-macros.h"

SYSPROF_BEGIN_DECLS

#define SYSPROF_CAPTURE_MAGIC (htole32(0xFDCA975E))
#define SYSPROF_CAPTURE_ALIGN (sizeof(SysprofCaptureAddress))

#if defined(_MSC_VER)
# define SYSPROF_ALIGNED_BEGIN(_N) __declspec(align (_N))
# define SYSPROF_ALIGNED_END(_N)
#else
# define SYSPROF_ALIGNED_BEGIN(_N)
# define SYSPROF_ALIGNED_END(_N) __attribute__((aligned ((_N))))
#endif

#define SYSPROF_CAPTURE_ADDRESS_FORMAT "0x%016" PRIx64

SYSPROF_STATIC_ASSERT (sizeof (void *) == sizeof (uintptr_t), "UINTPTR_MAX can’t be used to determine sizeof(void*) at compile time");
#if UINTPTR_MAX == 0xFFFFFFFFFFFFFFFFu
# define SYSPROF_CAPTURE_JITMAP_MARK    SYSPROF_UINT64_CONSTANT(0xE000000000000000)
#elif UINTPTR_MAX == 0xFFFFFFFF
# define SYSPROF_CAPTURE_JITMAP_MARK    SYSPROF_UINT64_CONSTANT(0xE0000000)
#else
#error Unknown UINTPTR_MAX
#endif

#define SYSPROF_CAPTURE_CURRENT_TIME   (sysprof_clock_get_current_time())
#define SYSPROF_CAPTURE_COUNTER_INT64  0
#define SYSPROF_CAPTURE_COUNTER_DOUBLE 1

typedef struct _SysprofCaptureReader    SysprofCaptureReader;
typedef struct _SysprofCaptureWriter    SysprofCaptureWriter;
typedef struct _SysprofCaptureCursor    SysprofCaptureCursor;
typedef struct _SysprofCaptureCondition SysprofCaptureCondition;

typedef uint64_t SysprofCaptureAddress;

typedef struct
{
  /*
   * The number of frames indexed by SysprofCaptureFrameType
   */
  size_t frame_count[16];

  /*
   * Padding for future expansion.
   */
  size_t padding[48];
} SysprofCaptureStat;

typedef union
{
  int64_t v64;
  double  vdbl;
} SysprofCaptureCounterValue;

typedef enum
{
  SYSPROF_CAPTURE_FRAME_TIMESTAMP    = 1,
  SYSPROF_CAPTURE_FRAME_SAMPLE       = 2,
  SYSPROF_CAPTURE_FRAME_MAP          = 3,
  SYSPROF_CAPTURE_FRAME_PROCESS      = 4,
  SYSPROF_CAPTURE_FRAME_FORK         = 5,
  SYSPROF_CAPTURE_FRAME_EXIT         = 6,
  SYSPROF_CAPTURE_FRAME_JITMAP       = 7,
  SYSPROF_CAPTURE_FRAME_CTRDEF       = 8,
  SYSPROF_CAPTURE_FRAME_CTRSET       = 9,
  SYSPROF_CAPTURE_FRAME_MARK         = 10,
  SYSPROF_CAPTURE_FRAME_METADATA     = 11,
  SYSPROF_CAPTURE_FRAME_LOG          = 12,
  SYSPROF_CAPTURE_FRAME_FILE_CHUNK   = 13,
  SYSPROF_CAPTURE_FRAME_ALLOCATION   = 14,
  SYSPROF_CAPTURE_FRAME_OVERLAY      = 15,
  SYSPROF_CAPTURE_FRAME_TRACE        = 16,
  SYSPROF_CAPTURE_FRAME_DBUS_MESSAGE = 17,
} SysprofCaptureFrameType;

/* Not part of ABI */
#define SYSPROF_CAPTURE_FRAME_LAST 18

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  uint32_t magic;
  uint32_t version : 8;
  uint32_t little_endian : 1;
  uint32_t padding : 23;
  char     capture_time[64];
  int64_t  time;
  int64_t  end_time;
  char     suffix[168];
} SysprofCaptureFileHeader
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  uint16_t len;
  int16_t  cpu;
  int32_t  pid;
  int64_t  time;
  uint32_t type : 8;
  uint32_t padding1 : 24;
  uint32_t padding2;
  uint8_t  data[0];
} SysprofCaptureFrame
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  uint32_t            layer : 8;
  uint32_t            padding : 24;
  uint32_t            src_len : 16;
  uint32_t            dst_len : 16;
  char                data[0];
} SysprofCaptureOverlay
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  uint64_t            start;
  uint64_t            end;
  uint64_t            offset;
  uint64_t            inode;
  char                filename[0];
} SysprofCaptureMap
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  uint32_t            n_jitmaps;
  uint8_t             data[0];
} SysprofCaptureJitmap
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  char                cmdline[0];
} SysprofCaptureProcess
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame   frame;
  uint32_t              n_addrs : 16;
  uint32_t              padding1 : 16;
  int32_t               tid;
  SysprofCaptureAddress addrs[0];
} SysprofCaptureSample
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame   frame;
  uint32_t              n_addrs : 16;
  uint32_t              entering : 1;
  uint32_t              padding1 : 15;
  int32_t               tid;
  SysprofCaptureAddress addrs[0];
} SysprofCaptureTrace
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  int32_t             child_pid;
} SysprofCaptureFork
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
} SysprofCaptureExit
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
} SysprofCaptureTimestamp
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  char                       category[32];
  char                       name[32];
  char                       description[52];
  uint32_t                   id : 24;
  uint32_t                   type : 8;
  SysprofCaptureCounterValue value;
} SysprofCaptureCounter
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame   frame;
  uint32_t              n_counters : 16;
  uint32_t              padding1 : 16;
  uint32_t              padding2;
  SysprofCaptureCounter counters[0];
} SysprofCaptureCounterDefine
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  /*
   * 96 bytes might seem a bit odd, but the counter frame header is 32
   * bytes.  So this makes a nice 2-cacheline aligned size which is
   * useful when the number of counters is rather small.
   */
  uint32_t                   ids[8];
  SysprofCaptureCounterValue values[8];
} SysprofCaptureCounterValues
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame         frame;
  uint32_t                    n_values : 16;
  uint32_t                    padding1 : 16;
  uint32_t                    padding2;
  SysprofCaptureCounterValues values[0];
} SysprofCaptureCounterSet
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  int64_t             duration;
  char                group[24];
  char                name[40];
  char                message[0];
} SysprofCaptureMark
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  char                id[40];
  char                metadata[0];
} SysprofCaptureMetadata
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  uint32_t            severity : 16;
  uint32_t            padding1 : 16;
  uint32_t            padding2 : 32;
  char                domain[32];
  char                message[0];
} SysprofCaptureLog
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame frame;
  uint32_t            is_last : 1;
  uint32_t            padding1 : 15;
  uint32_t            len : 16;
  char                path[256];
  uint8_t             data[0];
} SysprofCaptureFileChunk
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame   frame;
  SysprofCaptureAddress alloc_addr;
  int64_t               alloc_size;
  int32_t               tid;
  uint32_t              n_addrs : 16;
  uint32_t              padding1 : 16;
  SysprofCaptureAddress addrs[0];
} SysprofCaptureAllocation
SYSPROF_ALIGNED_END(1);

SYSPROF_ALIGNED_BEGIN(1)
typedef struct
{
  SysprofCaptureFrame   frame;
  uint16_t              bus_type : 2;
  uint16_t              flags : 14;
  uint16_t              message_len;
  uint8_t               message[0];
} SysprofCaptureDBusMessage
SYSPROF_ALIGNED_END(1);

#define SYSPROF_CAPTURE_DBUS_TYPE_SYSTEM  0
#define SYSPROF_CAPTURE_DBUS_TYPE_SESSION 1
#define SYSPROF_CAPTURE_DBUS_TYPE_A11Y    2
#define SYSPROF_CAPTURE_DBUS_TYPE_OTHER   3

#define SYSPROF_CAPTURE_DBUS_FLAGS_MESSAGE_TOO_LARGE (1<<0)

SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureFileHeader) == 256, "SysprofCaptureFileHeader changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureFrame) == 24, "SysprofCaptureFrame changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureMap) == 56, "SysprofCaptureMap changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureJitmap) == 28, "SysprofCaptureJitmap changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureProcess) == 24, "SysprofCaptureProcess changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureSample) == 32, "SysprofCaptureSample changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureTrace) == 32, "SysprofCaptureTrace changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureFork) == 28, "SysprofCaptureFork changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureExit) == 24, "SysprofCaptureExit changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureTimestamp) == 24, "SysprofCaptureTimestamp changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureCounter) == 128, "SysprofCaptureCounter changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureCounterValues) == 96, "SysprofCaptureCounterValues changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureCounterDefine) == 32, "SysprofCaptureCounterDefine changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureCounterSet) == 32, "SysprofCaptureCounterSet changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureMark) == 96, "SysprofCaptureMark changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureMetadata) == 64, "SysprofCaptureMetadata changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureLog) == 64, "SysprofCaptureLog changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureFileChunk) == 284, "SysprofCaptureFileChunk changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureAllocation) == 48, "SysprofCaptureAllocation changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureOverlay) == 32, "SysprofCaptureOverlay changed size");
SYSPROF_STATIC_ASSERT (sizeof (SysprofCaptureDBusMessage) == 28, "SysprofCaptureDBusMessage changed size");

SYSPROF_STATIC_ASSERT ((offsetof (SysprofCaptureAllocation, addrs) % SYSPROF_CAPTURE_ALIGN) == 0, "SysprofCaptureAllocation.addrs is not aligned");
SYSPROF_STATIC_ASSERT ((offsetof (SysprofCaptureSample, addrs) % SYSPROF_CAPTURE_ALIGN) == 0, "SysprofCaptureSample.addrs is not aligned");
SYSPROF_STATIC_ASSERT ((offsetof (SysprofCaptureTrace, addrs) % SYSPROF_CAPTURE_ALIGN) == 0, "SysprofCaptureTrace.addrs is not aligned");

static inline int
sysprof_capture_address_compare (SysprofCaptureAddress a,
                                 SysprofCaptureAddress b)
{
  if (a < b)
    return -1;
  if (a > b)
    return 1;
  else
    return 0;
}

/**
 * SysprofBacktraceFunc:
 * @addrs: (inout) (array length=n_addrs): an array to place addresses
 *   into the capture frame
 * @n_addrs: the length of @addrs
 * @user_data: (scope call): closure data for the callback
 *
 * Returns: the number of addresses filled in @addrs
 */
typedef int (*SysprofBacktraceFunc) (SysprofCaptureAddress *addrs,
                                     unsigned int           n_addrs,
                                     void                  *user_data);

SYSPROF_END_DECLS
