/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <os/log.h>

#if USE(APPLE_INTERNAL_SDK)

#include <os/log_private.h>

#else

typedef uint32_t os_trace_mode_t;

OS_ENUM(_os_trace_commonmodes, os_trace_mode_t,
    OS_TRACE_MODE_INFO                          = 0x01,
    OS_TRACE_MODE_DEBUG                         = 0x02,
    OS_TRACE_MODE_BACKTRACE                     = 0x04,
    OS_TRACE_MODE_STREAM_LIVE                   = 0x08,
    OS_TRACE_MODE_OFF                           = 0x0400,
);

#endif

WTF_EXTERN_C_BEGIN

OS_EXPORT OS_NOTHROW OS_NOT_TAIL_CALLED OS_NONNULL5
void os_log_with_args(os_log_t oslog, os_log_type_t type, const char *format, va_list args, void *ret_addr);

OS_EXPORT OS_NOTHROW
void os_trace_set_mode(os_trace_mode_t mode);

WTF_EXTERN_C_END
