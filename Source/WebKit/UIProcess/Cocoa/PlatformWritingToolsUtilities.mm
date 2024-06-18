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

#if ENABLE(WRITING_TOOLS)

#import "config.h"
#import "PlatformWritingToolsUtilities.h"

#import <WebCore/UnifiedTextReplacementTypes.h>

namespace WebKit {

#pragma mark - Conversions from web types to platform types.

PlatformWritingToolsBehavior convertToPlatformWritingToolsBehavior(WebCore::UnifiedTextReplacement::ReplacementBehavior behavior)
{
    switch (behavior) {
    case WebCore::UnifiedTextReplacement::ReplacementBehavior::None:
        return PlatformWritingToolsBehaviorNone;

    case WebCore::UnifiedTextReplacement::ReplacementBehavior::Default:
        return PlatformWritingToolsBehaviorDefault;

    case WebCore::UnifiedTextReplacement::ReplacementBehavior::Limited:
        return PlatformWritingToolsBehaviorLimited;

    case WebCore::UnifiedTextReplacement::ReplacementBehavior::Complete:
        return PlatformWritingToolsBehaviorComplete;
    }
}

WTTextSuggestionState convertToPlatformTextSuggestionState(WebCore::UnifiedTextReplacement::Replacement::State state)
{
    switch (state) {
    case WebCore::UnifiedTextReplacement::Replacement::State::Pending:
        return WTTextSuggestionStatePending;
    case WebCore::UnifiedTextReplacement::Replacement::State::Active:
        return WTTextSuggestionStateReviewing;
    case WebCore::UnifiedTextReplacement::Replacement::State::Reverted:
        return WTTextSuggestionStateRejected;
    case WebCore::UnifiedTextReplacement::Replacement::State::Invalid:
        return WTTextSuggestionStateInvalid;
    }
}

RetainPtr<WTContext> convertToPlatformContext(const WebCore::UnifiedTextReplacement::Context& contextData)
{
    return adoptNS([[WTContext alloc] initWithAttributedText:contextData.attributedText.nsAttributedString().get() range:NSMakeRange(contextData.range.location, contextData.range.length)]);
}

#pragma mark - Conversions from platform types to web types.

WebCore::UnifiedTextReplacement::ReplacementBehavior convertToWebWritingToolsBehavior(PlatformWritingToolsBehavior behavior)
{
    switch (behavior) {
    case PlatformWritingToolsBehaviorNone:
        return WebCore::UnifiedTextReplacement::ReplacementBehavior::None;

    case PlatformWritingToolsBehaviorDefault:
        return WebCore::UnifiedTextReplacement::ReplacementBehavior::Default;

    case PlatformWritingToolsBehaviorLimited:
        return WebCore::UnifiedTextReplacement::ReplacementBehavior::Limited;

    case PlatformWritingToolsBehaviorComplete:
        return WebCore::UnifiedTextReplacement::ReplacementBehavior::Complete;
    }
}

WebCore::UnifiedTextReplacement::Replacement::State convertToWebTextSuggestionState(WTTextSuggestionState state)
{
    switch (state) {
    case WTTextSuggestionStatePending:
        return WebCore::UnifiedTextReplacement::Replacement::State::Pending;
    case WTTextSuggestionStateReviewing:
        return WebCore::UnifiedTextReplacement::Replacement::State::Active;
    case WTTextSuggestionStateRejected:
        return WebCore::UnifiedTextReplacement::Replacement::State::Reverted;
    case WTTextSuggestionStateInvalid:
        return WebCore::UnifiedTextReplacement::Replacement::State::Invalid;

    // FIXME: Remove this default case once the WTTextSuggestionStateAccepted case is no longer in the build.
    default:
        ASSERT_NOT_REACHED();
        return WebCore::UnifiedTextReplacement::Replacement::State::Invalid;
    }
}

WebCore::UnifiedTextReplacement::EditAction convertToWebAction(WTAction action)
{
    switch (action) {
    case WTActionShowOriginal:
        return WebCore::UnifiedTextReplacement::EditAction::Undo;
    case WTActionShowRewritten:
        return WebCore::UnifiedTextReplacement::EditAction::Redo;
    case WTActionCompositionRestart:
        return WebCore::UnifiedTextReplacement::EditAction::UndoAll;
    }
}

WebCore::UnifiedTextReplacement::Session::ReplacementType convertToWebSessionType(WTSessionType type)
{
    switch (type) {
    case WTSessionTypeProofreading:
        return WebCore::UnifiedTextReplacement::Session::ReplacementType::PlainText;
    case WTSessionTypeComposition:
        return WebCore::UnifiedTextReplacement::Session::ReplacementType::RichText;
    }
}

WebCore::UnifiedTextReplacement::Session::CorrectionType convertToWebCompositionSessionType(WTCompositionSessionType type)
{
    switch (type) {
    case WTCompositionSessionTypeNone:
        return WebCore::UnifiedTextReplacement::Session::CorrectionType::None;

    // FIXME: Map these to specific `CorrectionType` types post-upstreaming.
    case WTCompositionSessionTypeMagic:
    case WTCompositionSessionTypeConcise:
    case WTCompositionSessionTypeFriendly:
    case WTCompositionSessionTypeProfessional:
    case WTCompositionSessionTypeOpenEnded:
    case WTCompositionSessionTypeSummary:
    case WTCompositionSessionTypeKeyPoints:
    case WTCompositionSessionTypeList:
    case WTCompositionSessionTypeTable:
    case WTCompositionSessionTypeCompose:
        return WebCore::UnifiedTextReplacement::Session::CorrectionType::Grammar;

    case WTCompositionSessionTypeSmartReply:
        return WebCore::UnifiedTextReplacement::Session::CorrectionType::Spelling;

    default:
        return WebCore::UnifiedTextReplacement::Session::CorrectionType::Grammar;
    }
}

std::optional<WebCore::UnifiedTextReplacement::Context> convertToWebContext(WTContext *context)
{
    auto contextUUID = WTF::UUID::fromNSUUID(context.uuid);
    if (!contextUUID)
        return std::nullopt;

    return { { *contextUUID, WebCore::AttributedString::fromNSAttributedString(context.attributedText), { context.range } } };
}

std::optional<WebCore::UnifiedTextReplacement::Session> convertToWebSession(WTSession *session)
{
    auto sessionUUID = WTF::UUID::fromNSUUID(session.uuid);
    if (!sessionUUID)
        return std::nullopt;

    return { { *sessionUUID, convertToWebSessionType(session.type), convertToWebCompositionSessionType(session.compositionSessionType) } };
}

std::optional<WebCore::UnifiedTextReplacement::Replacement> convertToWebTextSuggestion(WTTextSuggestion *suggestion)
{
    auto suggestionUUID = WTF::UUID::fromNSUUID(suggestion.uuid);
    if (!suggestionUUID)
        return std::nullopt;

    return { { *suggestionUUID, { suggestion.originalRange }, { suggestion.replacement }, convertToWebTextSuggestionState(suggestion.state) } };
}

} // namespace WebKit

#endif
