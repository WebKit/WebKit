/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/Assertions.h>

// Called from the AXObjectCache constructor.
void setAccessibilityLogChannelEnabled(bool);

bool isAccessibilityLogChannelEnabled();

// Use AX_BROKEN_ASSERT when a non-fatal assertion is failing even though it should never happen.
// It will change it to a release log, but only if the accessibility log channel is enabled. On macOS:
//     defaults write -g WebCoreLogging Accessibility
// Monitor with:
//     log stream --process Safari --predicate 'subsystem="com.apple.WebKit" AND category="Accessibility"'
#define AX_BROKEN_ASSERT(assertion, ...) do { \
    if (isAccessibilityLogChannelEnabled()) { \
        RELEASE_LOG_ERROR_IF(!(assertion), Accessibility, "BROKEN ASSERTION FAILED in %s(%d) : %s\n", __FILE__, __LINE__, WTF_PRETTY_FUNCTION); \
    } \
} while (0)

// Enable this in order to get debug asserts, which are called too frequently to be enabled
// by default.
#define AX_DEBUG_ASSERTS_ENABLED 0

#if AX_DEBUG_ASSERTS_ENABLED
#define AX_DEBUG_ASSERT(assertion) ASSERT(assertion)
#else
#define AX_DEBUG_ASSERT(assertion) ((void)0)
#endif
