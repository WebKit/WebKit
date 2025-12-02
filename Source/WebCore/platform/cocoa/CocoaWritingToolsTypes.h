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

#import <wtf/Platform.h>

#if ENABLE(WRITING_TOOLS)

#if PLATFORM(IOS_FAMILY)

#import <UIKit/UITextInputTraits.h>

using CocoaWritingToolsBehavior = UIWritingToolsBehavior;

constexpr auto CocoaWritingToolsBehaviorNone = UIWritingToolsBehaviorNone;
constexpr auto CocoaWritingToolsBehaviorDefault = UIWritingToolsBehaviorDefault;
constexpr auto CocoaWritingToolsBehaviorLimited = UIWritingToolsBehaviorLimited;
constexpr auto CocoaWritingToolsBehaviorComplete = UIWritingToolsBehaviorComplete;

using CocoaWritingToolsResultOptions = UIWritingToolsResultOptions;

constexpr auto CocoaWritingToolsResultPlainText = UIWritingToolsResultPlainText;
constexpr auto CocoaWritingToolsResultRichText = UIWritingToolsResultRichText;
constexpr auto CocoaWritingToolsResultList = UIWritingToolsResultList;
constexpr auto CocoaWritingToolsResultTable = UIWritingToolsResultTable;

#else

#import <AppKit/NSTextCheckingClient.h>

using CocoaWritingToolsBehavior = NSWritingToolsBehavior;

constexpr auto CocoaWritingToolsBehaviorNone = NSWritingToolsBehaviorNone;
constexpr auto CocoaWritingToolsBehaviorDefault = NSWritingToolsBehaviorDefault;
constexpr auto CocoaWritingToolsBehaviorLimited = NSWritingToolsBehaviorLimited;
constexpr auto CocoaWritingToolsBehaviorComplete = NSWritingToolsBehaviorComplete;

using CocoaWritingToolsResultOptions = NSWritingToolsResultOptions;

constexpr auto CocoaWritingToolsResultPlainText = NSWritingToolsResultPlainText;
constexpr auto CocoaWritingToolsResultRichText = NSWritingToolsResultRichText;
constexpr auto CocoaWritingToolsResultList = NSWritingToolsResultList;
constexpr auto CocoaWritingToolsResultTable = NSWritingToolsResultTable;

#endif // PLATFORM(IOS_FAMILY)

#endif // ENABLE(WRITING_TOOLS)
