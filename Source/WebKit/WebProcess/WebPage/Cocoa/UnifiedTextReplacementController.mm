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

#if ENABLE(UNIFIED_TEXT_REPLACEMENT)

#include "config.h"
#include "UnifiedTextReplacementController.h"

#include "TextIndicatorStyle.h"
#include "WebPage.h"
#include "WebUnifiedTextReplacementContextData.h"
#include <WebCore/BoundaryPoint.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/Editor.h>
#include <WebCore/FocusController.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/HTMLConverter.h>
#include <WebCore/RenderedDocumentMarker.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TextIterator.h>
#include <WebCore/VisiblePosition.h>
#include <WebCore/WebContentReader.h>

namespace WebKit {

// MARK: Static utility helper methods.

WebCore::CharacterRange UnifiedTextReplacementController::characterRange(const WebCore::SimpleRange& scope, const WebCore::SimpleRange& range)
{
    return WebCore::characterRange(scope, range, { WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForImagesOnly });
}

uint64_t UnifiedTextReplacementController::characterCount(const WebCore::SimpleRange& range)
{
    return WebCore::characterCount(range, { WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForImagesOnly });
}

WebCore::SimpleRange UnifiedTextReplacementController::resolveCharacterRange(const WebCore::SimpleRange& scope, WebCore::CharacterRange range)
{
    return WebCore::resolveCharacterRange(scope, range, { WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForImagesOnly });
}

String UnifiedTextReplacementController::plainText(const WebCore::SimpleRange& range)
{
    return WebCore::plainText(range, { WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForImagesOnly });
}

// MARK: UnifiedTextReplacementController implementation.

UnifiedTextReplacementController::UnifiedTextReplacementController(WebPage& webPage)
    : m_webPage(webPage)
{
}

void UnifiedTextReplacementController::willBeginTextReplacementSession(const std::optional<WebUnifiedTextReplacementSessionData>& session, CompletionHandler<void(const Vector<WebUnifiedTextReplacementContextData>&)>&& completionHandler)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s)", session ? session->uuid.toString().utf8().data() : "");

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    auto contextRange = m_webPage->autocorrectionContextRange();
    if (!contextRange) {
        RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => no context range", session ? session->uuid.toString().utf8().data() : "");
        completionHandler({ });
        return;
    }

    // If the UUID is invalid, the session is ephemeral.
    if (session) {
        auto liveRange = createLiveRange(*contextRange);

        ASSERT(!m_contextRanges.contains(session->uuid));

        m_contextRanges.set(session->uuid, liveRange);
        m_replacementTypes.set(session->uuid, session->replacementType);
    }

    auto selectedTextRange = document->selection().selection().firstRange();

    auto attributedStringFromRange = WebCore::editingAttributedString(*contextRange);
    auto selectedTextCharacterRange = UnifiedTextReplacementController::characterRange(*contextRange, *selectedTextRange);

    if (session) {
        auto attributedStringCharacterCount = attributedStringFromRange.string.length();
        auto contextRangeCharacterCount = UnifiedTextReplacementController::characterCount(*contextRange);

        // Postcondition: the selected text character range must be a valid range within the
        // attributed string formed by the context range; the length of the entire context range
        // being equal to the length of the attributed string implies the range is valid.
        if (UNLIKELY(attributedStringCharacterCount != contextRangeCharacterCount)) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => attributed string length (%u) != context range length (%llu)", session->uuid.toString().utf8().data(), attributedStringCharacterCount, contextRangeCharacterCount);
            ASSERT_NOT_REACHED();
            completionHandler({ });
            return;
        }
    }

    completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
}

void UnifiedTextReplacementController::didBeginTextReplacementSession(const WebUnifiedTextReplacementSessionData& session, const Vector<WebUnifiedTextReplacementContextData>& contexts)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didBeginTextReplacementSession (%s) [received contexts: %zu]", session.uuid.toString().utf8().data(), contexts.size());
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements(const WebUnifiedTextReplacementSessionData& session, const Vector<WebTextReplacementData>& replacements, const WebUnifiedTextReplacementContextData& context, bool finished)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements (%s) [received replacements: %zu, finished: %d]", session.uuid.toString().utf8().data(), replacements.size(), finished);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT(m_contextRanges.contains(session.uuid));

    m_webPage->removeTextIndicatorStyleForID(session.uuid);

    document->selection().clear();

    auto sessionRange = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    // The tracking of the additional replacement location offset needs to be scoped to a particular instance
    // of this class, instead of just this function, because the function may need to be called multiple times.
    // This ensures that subsequent calls of this function should effectively be treated as just more iterations
    // of the following for-loop.

    auto& additionalOffset = m_replacementLocationOffsets.add(session.uuid, 0).iterator->value;

    for (const auto& replacementData : replacements) {
        auto locationWithOffset = replacementData.originalRange.location + additionalOffset;

        auto resolvedRange = UnifiedTextReplacementController::resolveCharacterRange(*sessionRange, { locationWithOffset, replacementData.originalRange.length });

        replaceContentsOfRangeInSession(session.uuid, resolvedRange, replacementData.replacement);

        sessionRange = contextRangeForSessionWithUUID(session.uuid);
        if (!sessionRange) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto newRangeWithOffset = WebCore::CharacterRange { locationWithOffset, replacementData.replacement.length() };
        auto newResolvedRange = UnifiedTextReplacementController::resolveCharacterRange(*sessionRange, newRangeWithOffset);

        auto originalString = [context.attributedText.nsAttributedString() attributedSubstringFromRange:replacementData.originalRange];

        auto markerData = WebCore::DocumentMarker::UnifiedTextReplacementData { originalString.string, replacementData.uuid, session.uuid, WebCore::DocumentMarker::UnifiedTextReplacementData::State::Pending };
        addMarker(newResolvedRange, WebCore::DocumentMarker::Type::UnifiedTextReplacement, markerData);

        additionalOffset += static_cast<int>(replacementData.replacement.length()) - static_cast<int>(replacementData.originalRange.length);
    }

    if (finished)
        document->selection().setSelection({ *sessionRange });
}

void UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement(const WebUnifiedTextReplacementSessionData& session, WebTextReplacementData::State state, const WebTextReplacementData& replacement, const WebUnifiedTextReplacementContextData& context)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement (%s) [new state: %hhu, replacement: %s]", session.uuid.toString().utf8().data(), enumToUnderlyingType(state), replacement.uuid.toString().utf8().data());

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto nodeAndMarker = findReplacementMarkerByUUID(*sessionRange, replacement.uuid);
    if (!nodeAndMarker)
        return;

    auto& [node, marker] = *nodeAndMarker;

    auto rangeToReplace = WebCore::makeSimpleRange(node, marker);

    switch (state) {
    case WebTextReplacementData::State::Active: {
        document->selection().setSelection({ rangeToReplace });
        document->selection().revealSelection();

        auto rect = document->view()->contentsToRootView(WebCore::unionRect(WebCore::RenderObject::absoluteTextRects(rangeToReplace)));

        if (CheckedPtr renderStyle = node.renderStyle()) {
            const auto& font = node.renderStyle()->fontCascade();
            auto [_, height] = WebCore::DocumentMarkerController::markerYPositionAndHeightForFont(font);

            rect.setY(rect.y() + std::round(height / 2.0));
        }

        m_webPage->textReplacementSessionShowInformationForReplacementWithUUIDRelativeToRect(session.uuid, replacement.uuid, rect);

        return;
    }

    case WebTextReplacementData::State::Reverted: {
        auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());

        auto offsetRange = WebCore::OffsetRange { marker.startOffset(), marker.endOffset() };
        document->markers().removeMarkers(node, offsetRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement });

        replaceContentsOfRangeInSession(session.uuid, rangeToReplace, data.originalText);

        return;
    }

    default:
        return;
    }
}

template<>
void UnifiedTextReplacementController::didEndTextReplacementSession<WebUnifiedTextReplacementSessionData::ReplacementType::PlainText>(const WebUnifiedTextReplacementSessionData& session, bool accepted)
{
    RefPtr document = this->document();

    auto sessionRange = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& markers = document->markers();

    markers.forEach<WebCore::DocumentMarkerController::IterationDirection::Backwards>(*sessionRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement }, [&](auto& node, auto& marker) {
        auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());

        auto offsetRange = WebCore::OffsetRange { marker.startOffset(), marker.endOffset() };

        auto rangeToReplace = makeSimpleRange(node, marker);

        markers.removeMarkers(node, offsetRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement });

        if (!accepted && data.state != WebCore::DocumentMarker::UnifiedTextReplacementData::State::Reverted)
            replaceContentsOfRangeInSession(session.uuid, rangeToReplace, data.originalText);

        return false;
    });
}

template<>
void UnifiedTextReplacementController::didEndTextReplacementSession<WebUnifiedTextReplacementSessionData::ReplacementType::RichText>(const WebUnifiedTextReplacementSessionData& session, bool accepted)
{
    if (accepted)
        return;

    RefPtr document = this->document();

    textReplacementSessionPerformEditActionForRichText(*document, session, WebTextReplacementData::EditAction::Undo);
}

void UnifiedTextReplacementController::didEndTextReplacementSession(const WebUnifiedTextReplacementSessionData& session, bool accepted)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didEndTextReplacementSession (%s) [accepted: %d]", session.uuid.toString().utf8().data(), accepted);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto replacementType = m_replacementTypes.get(session.uuid);
    switch (replacementType) {
    case WebUnifiedTextReplacementSessionData::ReplacementType::PlainText:
        didEndTextReplacementSession<WebUnifiedTextReplacementSessionData::ReplacementType::PlainText>(session, accepted);
        break;
    case WebUnifiedTextReplacementSessionData::ReplacementType::RichText:
        didEndTextReplacementSession<WebUnifiedTextReplacementSessionData::ReplacementType::RichText>(session, accepted);
        break;
    }

    m_webPage->removeTextIndicatorStyleForID(session.uuid);

    auto sessionRange = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    document->selection().setSelection({ *sessionRange });

    m_textIndicatorCharacterRangesForSessions.removeFirstMatching([&](auto& candidateSession) {
        return candidateSession.first == session.uuid;
    });
    removeTransparentMarkersForSession(session.uuid, RemoveAllMarkersForSession::Yes);

    m_replacementTypes.remove(session.uuid);
    m_contextRanges.remove(session.uuid);
    m_originalDocumentNodes.remove(session.uuid);
    m_replacedDocumentNodes.remove(session.uuid);
    m_replacementLocationOffsets.remove(session.uuid);
}

void UnifiedTextReplacementController::removeTransparentMarkersForUUID(const WTF::UUID& uuid)
{

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    document->markers().removeMarkers({ WebCore::DocumentMarker::Type::TransparentContent }, [&](const WebCore::DocumentMarker& marker) {
        return std::get<WebCore::DocumentMarker::TransparentContentData>(marker.data()).uuid == uuid ? WebCore::FilterMarkerResult::Remove : WebCore::FilterMarkerResult::Keep;
    });
}

void UnifiedTextReplacementController::removeTransparentMarkersForSession(const WTF::UUID& uuid, RemoveAllMarkersForSession removeAll)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    for (auto [sessionUUID, remainingRangeData] : m_remainingRangesForSessions) {
        if ((sessionUUID == uuid && removeAll == RemoveAllMarkersForSession::Yes) || (remainingRangeData.first == uuid))
            removeTransparentMarkersForUUID(remainingRangeData.first);
    }

    if (auto sessionRange = contextRangeForSessionWithUUID(uuid)) {
        removeTransparentMarkersForUUID(uuid);
        if (removeAll == RemoveAllMarkersForSession::No)
            return;

        for (auto session : m_textIndicatorCharacterRangesForSessions) {
            if (session.first == uuid) {
                for (auto textIndicatorCharacterRange : session.second)
                    removeTransparentMarkersForUUID(textIndicatorCharacterRange.first);
            }
        }
    }

    for (auto session : m_textIndicatorCharacterRangesForSessions) {
        for (auto textIndicatorCharacterRange : session.second) {
            if (textIndicatorCharacterRange.first == uuid) {
                if (auto sessionRange = contextRangeForSessionWithUUID(session.first))
                    removeTransparentMarkersForUUID(uuid);
            }
        }
    }
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange(const WebUnifiedTextReplacementSessionData& session, const WebCore::AttributedString& attributedText, const WebCore::CharacterRange& range, const WebUnifiedTextReplacementContextData& context, bool finished)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) [range: %llu, %llu]", session.uuid.toString().utf8().data(), range.location, range.length);

    auto contextTextCharacterCount = context.attributedText.string.length();

    // Precondition: the range is always relative to the context's attributed text, so by definition it must
    // be strictly less than the length of the attributed string.
    if (UNLIKELY(contextTextCharacterCount < range.location + range.length)) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => trying to replace a range larger than the context range (context range length: %u, range.location %llu, range.length %llu)", session.uuid.toString().utf8().data(), contextTextCharacterCount, range.location, range.length);
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_webPage->removeTextIndicatorStyleForID(session.uuid);

    document->selection().clear();

    auto sessionRangeCharacterCount = UnifiedTextReplacementController::characterCount(*sessionRange);

    if (UNLIKELY(range.length + sessionRangeCharacterCount < contextTextCharacterCount)) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => the range offset by the character count delta must have a non-negative size (context range length: %u, range.length %llu, session length: %llu)", session.uuid.toString().utf8().data(), contextTextCharacterCount, range.length, sessionRangeCharacterCount);
        ASSERT_NOT_REACHED();
        return;
    }

    // The character count delta is `sessionRangeCharacterCount - contextTextCharacterCount`;
    // the above check ensures that the full range length expression will never underflow.

    auto characterCountDelta = sessionRangeCharacterCount - contextTextCharacterCount;
    auto resolvedRange = UnifiedTextReplacementController::resolveCharacterRange(*sessionRange, { range.location, range.length + characterCountDelta });

    if (!m_originalDocumentNodes.contains(session.uuid)) {
        auto contents = m_contextRanges.get(session.uuid)->cloneContents();
        if (contents.hasException()) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => exception when cloning contents", session.uuid.toString().utf8().data());
            ASSERT_NOT_REACHED();
            return;
        }

        m_originalDocumentNodes.set(session.uuid, contents.returnValue()); // Deep clone.
    }

    RefPtr fragment = WebCore::createFragment(*document->frame(), attributedText.nsAttributedString().get(), { WebCore::FragmentCreationOptions::NoInterchangeNewlines, WebCore::FragmentCreationOptions::SanitizeMarkup });
    if (!fragment) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto subCharacterRange = [](WebCore::CharacterRange superRange, WebCore::CharacterRange previousRange) {
        auto location = previousRange.location + previousRange.length;
        auto length = superRange.length - previousRange.length;
        if (superRange.length < previousRange.length) {
            ASSERT_NOT_REACHED();
            return WebCore::CharacterRange { 0, 0 };
        }
        return WebCore::CharacterRange { location, length };
    };

    auto replacedRange = resolvedRange;
    for (auto [sessionUUID, sessionRangeVector] : m_textIndicatorCharacterRangesForSessions) {
        if (sessionUUID != session.uuid)
            continue;

        // sessionRangeVector.last() will always be valid because when creating
        // the structure we always create the vector with an element.
        auto replaceCharacterRange = subCharacterRange(range, sessionRangeVector.last().second);
        replacedRange = UnifiedTextReplacementController::resolveCharacterRange(*sessionRange, replaceCharacterRange);
        break;
    }

#if PLATFORM(MAC)
    auto sourceTextIndicatorUUID = WTF::UUID::createVersion4();
    m_webPage->createTextIndicatorForRange(replacedRange, [sourceTextIndicatorUUID, weakWebPage = WeakPtr { m_webPage }](std::optional<WebCore::TextIndicatorData>&& textIndicatorData) {
        if (!weakWebPage)
            return;
        RefPtr protectedWebPage = weakWebPage.get();
        if (textIndicatorData)
            protectedWebPage->addTextIndicatorStyleForID(sourceTextIndicatorUUID, { WebKit::TextIndicatorStyle::Source, WTF::UUID(WTF::UUID::emptyValue)  }, *textIndicatorData);
    });
#endif

    replaceContentsOfRangeInSession(session.uuid, resolvedRange, *fragment);

    auto finalTextIndicatorUUID = WTF::UUID::createVersion4();
    auto characterRangeAfterReplace = WebCore::CharacterRange(range.location, range.length + characterCountDelta);

    auto sessionRangeAfterReplace = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRangeAfterReplace) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto replacedRangeAfterReplace = UnifiedTextReplacementController::resolveCharacterRange(*sessionRangeAfterReplace, characterRangeAfterReplace);

    auto remainingRange = makeRangeSelectingNodeContents(*document);
    remainingRange.start.container = replacedRangeAfterReplace.endContainer();
    remainingRange.start.offset = replacedRangeAfterReplace.endOffset();

    auto remainingRangeUUID = WTF::UUID::createVersion4();
    m_remainingRangesForSessions.append({ session.uuid, { remainingRangeUUID, remainingRange } });

    m_webPage->createTextIndicatorForRange(replacedRangeAfterReplace, [finalTextIndicatorUUID, remainingRangeUUID, weakWebPage = WeakPtr { m_webPage }](std::optional<WebCore::TextIndicatorData>&& textIndicatorData) {
        if (!weakWebPage)
            return;
        RefPtr protectedWebPage = weakWebPage.get();
        if (textIndicatorData)
            protectedWebPage->addTextIndicatorStyleForID(finalTextIndicatorUUID, { TextIndicatorStyle::Final, remainingRangeUUID }, *textIndicatorData);
    });

    bool attachedToExistingSession = false;
    for (auto& [sessionUUID, sessionRangeVector] : m_textIndicatorCharacterRangesForSessions) {
        if (sessionUUID == session.uuid) {
            // sessionRangeVector.last() will always be valid because when creating
            // the structure we always create the vector with an element.
            auto characterRange = subCharacterRange(range, sessionRangeVector.last().second);
#if PLATFORM(MAC)
            sessionRangeVector.append({ sourceTextIndicatorUUID, characterRange });
#endif
            sessionRangeVector.append({ finalTextIndicatorUUID, characterRange });

            attachedToExistingSession = true;
            break;
        }
    }
    if (!attachedToExistingSession) {
#if PLATFORM(MAC)
        m_textIndicatorCharacterRangesForSessions.append({ session.uuid, { { sourceTextIndicatorUUID, characterRangeAfterReplace }, { finalTextIndicatorUUID , characterRangeAfterReplace } } });
#else
        m_textIndicatorCharacterRangesForSessions.append({ session.uuid, { { finalTextIndicatorUUID, characterRangeAfterReplace } } });
#endif
    }
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction(const WebUnifiedTextReplacementSessionData& session, WebTextReplacementData::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) [action: %hhu]", session.uuid.toString().utf8().data(), enumToUnderlyingType(action));

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto replacementType = m_replacementTypes.get(session.uuid);
    switch (replacementType) {
    case WebUnifiedTextReplacementSessionData::ReplacementType::PlainText:
        return textReplacementSessionPerformEditActionForPlainText(*document, session, action);
    case WebUnifiedTextReplacementSessionData::ReplacementType::RichText:
        return textReplacementSessionPerformEditActionForRichText(*document, session, action);
    }
}

void UnifiedTextReplacementController::textReplacementSessionPerformEditActionForPlainText(WebCore::Document& document, const WebUnifiedTextReplacementSessionData& session, WebTextReplacementData::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionPerformEditActionForPlainText (%s) [action: %hhu]", session.uuid.toString().utf8().data(), enumToUnderlyingType(action));

    auto sessionRange = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& markers = document.markers();

    markers.forEach<WebCore::DocumentMarkerController::IterationDirection::Backwards>(*sessionRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement }, [&](auto& node, auto& marker) {
        auto rangeToReplace = WebCore::makeSimpleRange(node, marker);

        auto currentText = UnifiedTextReplacementController::plainText(rangeToReplace);

        auto oldData = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());
        auto previousText = oldData.originalText;
        auto offsetRange = WebCore::OffsetRange { marker.startOffset(), marker.endOffset() };

        markers.removeMarkers(node, offsetRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement });

        auto newState = [&] {
            switch (action) {
            case WebTextReplacementData::EditAction::Undo:
                return WebCore::DocumentMarker::UnifiedTextReplacementData::State::Reverted;

            case WebTextReplacementData::EditAction::Redo:
                return WebCore::DocumentMarker::UnifiedTextReplacementData::State::Pending;

            default:
                ASSERT_NOT_REACHED();
                return WebCore::DocumentMarker::UnifiedTextReplacementData::State::Pending;
            }
        }();

        replaceContentsOfRangeInSession(session.uuid, rangeToReplace, previousText);

        auto newData = WebCore::DocumentMarker::UnifiedTextReplacementData { currentText, oldData.uuid, session.uuid, newState };
        auto newOffsetRange = WebCore::OffsetRange { offsetRange.start, offsetRange.end + previousText.length() - currentText.length() };

        markers.addMarker(node, WebCore::DocumentMarker { WebCore::DocumentMarker::Type::UnifiedTextReplacement, newOffsetRange, WTFMove(newData) });

        return false;
    });
}

void UnifiedTextReplacementController::textReplacementSessionPerformEditActionForRichText(WebCore::Document& document, const WebUnifiedTextReplacementSessionData& session, WebTextReplacementData::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionPerformEditActionForRichText (%s) [action: %hhu]", session.uuid.toString().utf8().data(), enumToUnderlyingType(action));

    auto sessionRange = contextRangeForSessionWithUUID(session.uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (m_originalDocumentNodes.isEmpty())
        return;

    auto contents = m_contextRanges.get(session.uuid)->cloneContents();
    if (contents.hasException()) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) => exception when cloning contents", session.uuid.toString().utf8().data());
        return;
    }

    switch (action) {
    case WebTextReplacementData::EditAction::Undo: {
        RefPtr originalFragment = m_originalDocumentNodes.take(session.uuid);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        m_replacedDocumentNodes.set(session.uuid, contents.returnValue()); // Deep clone.
        replaceContentsOfRangeInSession(session.uuid, *sessionRange, *originalFragment);

        break;
    }

    case WebTextReplacementData::EditAction::Redo: {
        RefPtr originalFragment = m_replacedDocumentNodes.take(session.uuid);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        m_replacedDocumentNodes.set(session.uuid, contents.returnValue()); // Deep clone.
        replaceContentsOfRangeInSession(session.uuid, *sessionRange, *originalFragment);

        break;
    }

    case WebTextReplacementData::EditAction::UndoAll: {
        RefPtr originalFragment = m_originalDocumentNodes.take(session.uuid);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        replaceContentsOfRangeInSession(session.uuid, *sessionRange, *originalFragment);
        m_replacedDocumentNodes.remove(session.uuid);

        break;
    }
    }

    RefPtr updatedLiveRange = m_contextRanges.get(session.uuid);
    if (!updatedLiveRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    switch (action) {
    case WebTextReplacementData::EditAction::Undo:
    case WebTextReplacementData::EditAction::UndoAll: {
        auto updatedContents = updatedLiveRange->cloneContents();
        if (updatedContents.hasException()) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) => exception when cloning contents after action", session.uuid.toString().utf8().data());
            return;
        }

        m_originalDocumentNodes.set(session.uuid, updatedContents.returnValue()); // Deep clone.

        break;
    }

    case WebTextReplacementData::EditAction::Redo:
        break;
    }
}

void UnifiedTextReplacementController::updateStateForSelectedReplacementIfNeeded()
{
    // Optimization: If there are no ongoing sessions, there is no need for any of this logic to
    // be executed, since there will be no relevant document markers anyways.
    if (m_contextRanges.isEmpty())
        return;

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto selectionRange = document->selection().selection().firstRange();
    if (!selectionRange)
        return;

    if (!document->selection().isCaret())
        return;

    auto nodeAndMarker = findReplacementMarkerContainingRange(*selectionRange);
    if (!nodeAndMarker)
        return;

    auto& [node, marker] = *nodeAndMarker;
    auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());

    m_webPage->textReplacementSessionUpdateStateForReplacementWithUUID(data.sessionUUID, WebTextReplacementData::State::Active, data.uuid);
}

// MARK: Private instance helper methods.

std::optional<WebCore::SimpleRange> UnifiedTextReplacementController::contextRangeForSessionWithUUID(const WTF::UUID& uuid) const
{
    return WebCore::makeSimpleRange(m_contextRanges.get(uuid));
}

std::optional<WebCore::SimpleRange> UnifiedTextReplacementController::contextRangeForSessionOrRangeWithUUID(const WTF::UUID& uuid) const
{
    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (sessionRange)
        return sessionRange;

    for (auto [sessionUUID, textIndicatorUUIDtoRanges] : m_textIndicatorCharacterRangesForSessions) {
        for (auto [textIndicatorUUID, textIndicatorRange] : textIndicatorUUIDtoRanges) {
            if (textIndicatorUUID == uuid) {
                auto fullSessionRange = contextRangeForSessionWithUUID(sessionUUID);
                if (fullSessionRange)
                    return UnifiedTextReplacementController::resolveCharacterRange(*fullSessionRange, textIndicatorRange);
            }
        }
    }

    for (auto [sessionUUID, remainingRangeUUIDtoRanges] : m_remainingRangesForSessions) {
        if (remainingRangeUUIDtoRanges.first == uuid)
            return remainingRangeUUIDtoRanges.second;
    }

    return std::nullopt;
}

RefPtr<WebCore::Document> UnifiedTextReplacementController::document() const
{
    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RefPtr frame = corePage->checkedFocusController()->focusedOrMainFrame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return frame->document();
}

std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> UnifiedTextReplacementController::findReplacementMarkerByUUID(const WebCore::SimpleRange& outerRange, const WTF::UUID& replacementUUID) const
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr<WebCore::Node> targetNode;
    WeakPtr<WebCore::DocumentMarker> targetMarker;

    document->markers().forEach(outerRange, { WebCore::DocumentMarker::Type::UnifiedTextReplacement }, [&replacementUUID, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());
        if (data.uuid != replacementUUID)
            return false;

        targetNode = &node;
        targetMarker = &marker;

        return true;
    });

    if (targetNode && targetMarker)
        return { { *targetNode, *targetMarker } };

    return std::nullopt;
}

std::optional<std::tuple<WebCore::Node&, WebCore::DocumentMarker&>> UnifiedTextReplacementController::findReplacementMarkerContainingRange(const WebCore::SimpleRange& range) const
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr<WebCore::Node> targetNode;
    WeakPtr<WebCore::DocumentMarker> targetMarker;

    document->markers().forEach(range, { WebCore::DocumentMarker::Type::UnifiedTextReplacement }, [&range, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<WebCore::DocumentMarker::UnifiedTextReplacementData>(marker.data());

        auto markerRange = WebCore::makeSimpleRange(node, marker);
        if (!WebCore::contains(WebCore::TreeType::ComposedTree, markerRange, range))
            return false;

        targetNode = &node;
        targetMarker = &marker;

        return true;
    });

    if (targetNode && targetMarker)
        return { { *targetNode, *targetMarker } };

    return std::nullopt;
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSessionInternal(const WTF::UUID& uuid, const WebCore::SimpleRange& range, WTF::Function<void(WebCore::Editor&)>&& replacementOperation)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithUUID(uuid);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRangeCount = UnifiedTextReplacementController::characterCount(*sessionRange);

    auto resolvedCharacterRange = UnifiedTextReplacementController::characterRange(*sessionRange, range);
    document->selection().setSelection({ range });

    replacementOperation(document->editor());

    auto selectedTextRange = document->selection().selection().firstRange();
    if (!selectedTextRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto extendedPosition = [](const WebCore::BoundaryPoint& point, uint64_t characterCount, WebCore::SelectionDirection direction) {
        auto visiblePosition = WebCore::VisiblePosition { WebCore::makeContainerOffsetPosition(point) };

        for (uint64_t i = 0; i < characterCount; ++i) {
            auto nextVisiblePosition = WebCore::positionOfNextBoundaryOfGranularity(visiblePosition, WebCore::TextGranularity::CharacterGranularity, direction);
            if (nextVisiblePosition.isNull())
                break;

            visiblePosition = nextVisiblePosition;
        }

        return visiblePosition;
    };

    auto extendedSelection = [extendedPosition](const WebCore::SimpleRange& range, uint64_t charactersToExtendBackwards, uint64_t charactersToExtendForwards) {
        auto start = extendedPosition(range.start, charactersToExtendBackwards, WebCore::SelectionDirection::Backward);
        auto end = extendedPosition(range.end, charactersToExtendForwards, WebCore::SelectionDirection::Forward);

        return WebCore::makeSimpleRange(start, end);
    };

    auto newSessionRange = extendedSelection(*selectedTextRange, resolvedCharacterRange.location, sessionRangeCount - (resolvedCharacterRange.location + resolvedCharacterRange.length));
    if (!newSessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto updatedLiveRange = WebCore::createLiveRange(*newSessionRange);
    m_contextRanges.set(uuid, updatedLiveRange);
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSession(const WTF::UUID& uuid, const WebCore::SimpleRange& range, const String& replacementText)
{
    replaceContentsOfRangeInSessionInternal(uuid, range, [&replacementText](WebCore::Editor& editor) {
        editor.replaceSelectionWithText(replacementText, WebCore::Editor::SelectReplacement::Yes, WebCore::Editor::SmartReplace::No, WebCore::EditAction::InsertReplacement);
    });
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSession(const WTF::UUID& uuid, const WebCore::SimpleRange& range, WebCore::DocumentFragment& fragment)
{
    replaceContentsOfRangeInSessionInternal(uuid, range, [&fragment](WebCore::Editor& editor) {
        editor.replaceSelectionWithFragment(fragment, WebCore::Editor::SelectReplacement::Yes, WebCore::Editor::SmartReplace::No, WebCore::Editor::MatchStyle::No, WebCore::EditAction::InsertReplacement);
    });
}

} // namespace WebKit

#endif
