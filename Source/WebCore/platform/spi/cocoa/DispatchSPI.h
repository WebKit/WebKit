/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef DispatchSPI_h
#define DispatchSPI_h

#include <dispatch/dispatch.h>

#if USE(APPLE_INTERNAL_SDK)

// FIXME: As a workaround for <rdar://problem/18337182>, we conditionally enclose the header
// <dispatch/private.h> in an extern "C" linkage block to make it suitable for C++ use.
#ifdef __cplusplus
extern "C" {
#endif

#include <dispatch/private.h>

#ifdef __cplusplus
}
#endif

#else // USE(APPLE_INTERNAL_SDK)

enum {
    DISPATCH_MEMORYSTATUS_PRESSURE_NORMAL = 0x01,
    DISPATCH_MEMORYSTATUS_PRESSURE_WARN = 0x02,
    DISPATCH_MEMORYSTATUS_PRESSURE_CRITICAL = 0x04,
};
#define DISPATCH_SOURCE_TYPE_MEMORYSTATUS (&_dispatch_source_type_memorystatus)

#endif

EXTERN_C const struct dispatch_source_type_s _dispatch_source_type_memorystatus;

#endif // DispatchSPI_h
