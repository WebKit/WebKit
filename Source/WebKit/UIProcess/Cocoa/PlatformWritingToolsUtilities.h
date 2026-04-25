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

#import <WebCore/CocoaWritingToolsTypes.h>
#import <WebCore/WritingToolsTypes.h>
#import <pal/spi/cocoa/WritingToolsSPI.h>
#import <wtf/RetainPtr.h>

namespace WebCore {

namespace WritingTools {
enum class Behavior : uint8_t;
enum class EditAction : uint8_t;
enum class ReplacementBehavior : uint8_t;
enum class ReplacementState : uint8_t;
enum class RequestedTool : uint16_t;
enum class SessionCompositionType : uint8_t;
enum class SessionType : uint8_t;
enum class TextSuggestionState : uint8_t;

struct Context;
struct Replacement;
struct Session;
}

}

namespace WebKit {

#pragma mark - Conversions from web types to platform types.

CocoaWritingToolsBehavior NODELETE convertToCocoaWritingToolsBehavior(WebCore::WritingTools::Behavior);

WTRequestedTool NODELETE convertToPlatformRequestedTool(WebCore::WritingTools::RequestedTool);

WTTextSuggestionState NODELETE convertToPlatformTextSuggestionState(WebCore::WritingTools::TextSuggestionState);

RetainPtr<WTContext> convertToPlatformContext(const WebCore::WritingTools::Context&);

#pragma mark - Conversions from platform types to web types.

WebCore::WritingTools::Behavior NODELETE convertToWebWritingToolsBehavior(CocoaWritingToolsBehavior);

WebCore::WritingTools::RequestedTool NODELETE convertToWebRequestedTool(WTRequestedTool);

WebCore::WritingTools::TextSuggestionState NODELETE convertToWebTextSuggestionState(WTTextSuggestionState);

WebCore::WritingTools::Action NODELETE convertToWebAction(WTAction);

WebCore::WritingTools::SessionType NODELETE convertToWebSessionType(WTSessionType);

WebCore::WritingTools::SessionCompositionType NODELETE convertToWebCompositionSessionType(WTCompositionSessionType);

std::optional<WebCore::WritingTools::Context> convertToWebContext(WTContext *);

std::optional<WebCore::WritingTools::Session> convertToWebSession(WTSession *);

std::optional<WebCore::WritingTools::TextSuggestion> convertToWebTextSuggestion(WTTextSuggestion *);

} // namespace WebKit

#endif
