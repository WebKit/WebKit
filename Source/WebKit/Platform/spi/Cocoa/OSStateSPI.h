/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if USE(OS_STATE)

#if USE(APPLE_INTERNAL_SDK)
#include <os/state_private.h>
#else

#include <os/base.h>

OS_ENUM(os_state_reason, uint32_t,
    OS_STATE_REASON_GENERAL          = 0x0001,
    OS_STATE_REASON_NETWORKING       = 0x0002,
    OS_STATE_REASON_CELLULAR         = 0x0004,
    OS_STATE_REASON_AUTHENTICATION   = 0x0008,
);

OS_ENUM(os_state_api, uint32_t,
    OS_STATE_API_ERROR = 1,
    OS_STATE_API_FAULT = 2,
    OS_STATE_API_REQUEST = 3,
);

OS_ENUM(os_state_data_type, uint32_t,
    OS_STATE_DATA_SERIALIZED_NSCF_OBJECT = 1,
    OS_STATE_DATA_PROTOCOL_BUFFER = 2,
    OS_STATE_DATA_CUSTOM = 3,
);

typedef struct os_state_hints_s {
    uint32_t osh_version;
    const char *osh_requestor;
    os_state_api_t osh_api;
    os_state_reason_t osh_reason;
} *os_state_hints_t;

typedef struct os_state_data_decoder_s {
    char osdd_library[64];
    char osdd_type[64];
} *os_state_data_decoder_t;

typedef struct os_state_data_s {
    os_state_data_type_t osd_type;
    IGNORE_CLANG_WARNINGS_BEGIN("packed")
    union {
        uint64_t osd_size:32;
        uint32_t osd_data_size;
    } __attribute__((packed, aligned(4)));
    IGNORE_CLANG_WARNINGS_END
    struct os_state_data_decoder_s osd_decoder;
    char osd_title[64];
    uint8_t osd_data[];
} *os_state_data_t;

typedef uint64_t os_state_handle_t;
typedef os_state_data_t (^os_state_block_t)(os_state_hints_t hints);

WTF_EXTERN_C_BEGIN

OS_EXPORT OS_NOTHROW OS_NOT_TAIL_CALLED
os_state_handle_t
os_state_add_handler(dispatch_queue_t, os_state_block_t);

WTF_EXTERN_C_END

#define OS_STATE_DATA_SIZE_NEEDED(data_size) (sizeof(struct os_state_data_s) + data_size)

#endif

#endif // USE(OS_STATE)
