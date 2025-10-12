/*
 * Copyright (C) 2021-2025 Apple Inc. All rights reserved.
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

#include <wtf/Compiler.h>

DECLARE_SYSTEM_HEADER

#if PLATFORM(MAC) || PLATFORM(IOS) || PLATFORM(MACCATALYST) || PLATFORM(WATCHOS) \
    || PLATFORM(APPLETV) || PLATFORM(VISION)
#if USE(APPLE_INTERNAL_SDK) && !defined(__swift__)
#include <os/reason_private.h>
#else

WTF_EXTERN_C_BEGIN

void abort_with_reason(uint32_t reasonNamespace, uint64_t reasonCode, const char *reasonString, uint64_t reasonFlags) __attribute__((noreturn, cold));

int os_fault_with_payload(uint32_t reasonNamespace, uint64_t reasonCode, void *payload, uint32_t payloadSize, const char *reasonString, uint64_t reasonFlags) __attribute__((cold));

int terminate_with_reason(int pid, uint32_t reasonNamespace, uint64_t reasonCode, const char *reasonString, uint64_t reasonFlags);

WTF_EXTERN_C_END

#endif // USE(APPLE_INTERNAL_SDK) && !defined(__swift__)

#else

#define abort_with_reason(reason_namespace, reason_code, reason_string, reason_flags)  CRASH()

#endif

#if !USE(APPLE_INTERNAL_SDK) || defined(__swift__)
#define OS_REASON_FLAG_NO_CRASH_REPORT     0x1
#define OS_REASON_FLAG_SECURITY_SENSITIVE  0x1000
#define OS_REASON_WEBKIT 31
#endif
