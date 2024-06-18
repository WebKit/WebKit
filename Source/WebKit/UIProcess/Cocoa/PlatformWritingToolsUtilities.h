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

#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

namespace UnifiedTextReplacement {
enum class EditAction : uint8_t;
enum class ReplacementBehavior : uint8_t;
enum class ReplacementState : uint8_t;
enum class SessionCorrectionType : uint8_t;
enum class SessionReplacementType : uint8_t;

struct Context;
struct Replacement;
struct Session;
}

}

namespace WebKit {

#pragma mark - Conversions from web types to platform types.

PlatformWritingToolsBehavior convertToPlatformWritingToolsBehavior(WebCore::UnifiedTextReplacement::ReplacementBehavior);

WTTextSuggestionState convertToPlatformTextSuggestionState(WebCore::UnifiedTextReplacement::ReplacementState);

RetainPtr<WTContext> convertToPlatformContext(const WebCore::UnifiedTextReplacement::Context&);

#pragma mark - Conversions from platform types to web types.

WebCore::UnifiedTextReplacement::ReplacementBehavior convertToWebWritingToolsBehavior(PlatformWritingToolsBehavior);

WebCore::UnifiedTextReplacement::ReplacementState convertToWebTextSuggestionState(WTTextSuggestionState);

WebCore::UnifiedTextReplacement::EditAction convertToWebAction(WTAction);

WebCore::UnifiedTextReplacement::SessionReplacementType convertToWebSessionType(WTSessionType);

WebCore::UnifiedTextReplacement::SessionCorrectionType convertToWebCompositionSessionType(WTCompositionSessionType);

std::optional<WebCore::UnifiedTextReplacement::Context> convertToWebContext(WTContext *);

std::optional<WebCore::UnifiedTextReplacement::Session> convertToWebSession(WTSession *);

std::optional<WebCore::UnifiedTextReplacement::Replacement> convertToWebTextSuggestion(WTTextSuggestion *);

} // namespace WebKit

#endif
