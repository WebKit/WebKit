/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#if ENABLE(WRITING_TOOLS)

#if USE(APPLE_INTERNAL_SDK)

#import <WritingTools/WTSession_Private.h>
#import <WritingTools/WritingTools.h>

#if PLATFORM(MAC)

using PlatformWritingToolsBehavior = NSWritingToolsBehavior;

constexpr auto PlatformWritingToolsBehaviorNone = NSWritingToolsBehaviorNone;
constexpr auto PlatformWritingToolsBehaviorDefault = NSWritingToolsBehaviorDefault;
constexpr auto PlatformWritingToolsBehaviorLimited = NSWritingToolsBehaviorLimited;
constexpr auto PlatformWritingToolsBehaviorComplete = NSWritingToolsBehaviorComplete;

// FIXME: (rdar://130540028) Remove uses of the old WritingToolsAllowedInputOptions API in favor of the new WritingToolsResultOptions API, and remove staging.

using PlatformWritingToolsResultOptions = NSUInteger;

constexpr auto PlatformWritingToolsResultPlainText = (PlatformWritingToolsResultOptions)(1 << 0);
constexpr auto PlatformWritingToolsResultRichText = (PlatformWritingToolsResultOptions)(1 << 1);
constexpr auto PlatformWritingToolsResultList = (PlatformWritingToolsResultOptions)(1 << 2);
constexpr auto PlatformWritingToolsResultTable = (PlatformWritingToolsResultOptions)(1 << 3);

#else

using PlatformWritingToolsBehavior = UIWritingToolsBehavior;

constexpr auto PlatformWritingToolsBehaviorNone = UIWritingToolsBehaviorNone;
constexpr auto PlatformWritingToolsBehaviorDefault = UIWritingToolsBehaviorDefault;
constexpr auto PlatformWritingToolsBehaviorLimited = UIWritingToolsBehaviorLimited;
constexpr auto PlatformWritingToolsBehaviorComplete = UIWritingToolsBehaviorComplete;

// FIXME: (rdar://130540028) Remove uses of the old WritingToolsAllowedInputOptions API in favor of the new WritingToolsResultOptions API, and remove staging.

using PlatformWritingToolsResultOptions = NSUInteger;

constexpr auto PlatformWritingToolsResultPlainText = (PlatformWritingToolsResultOptions)(1 << 0);
constexpr auto PlatformWritingToolsResultRichText = (PlatformWritingToolsResultOptions)(1 << 1);
constexpr auto PlatformWritingToolsResultList = (PlatformWritingToolsResultOptions)(1 << 2);
constexpr auto PlatformWritingToolsResultTable = (PlatformWritingToolsResultOptions)(1 << 3);

#endif

#else

#error Symbols must be forward declared once used with non-internal SDKS.

#endif // USE(APPLE_INTERNAL_SDK)

#endif // ENABLE(WRITING_TOOLS)
