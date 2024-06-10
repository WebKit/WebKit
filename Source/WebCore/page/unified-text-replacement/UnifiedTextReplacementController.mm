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

#include "Chrome.h"
#include "ChromeClient.h"
#include "DocumentInlines.h"
#include "DocumentMarkerController.h"
#include "Editor.h"
#include "FocusController.h"
#include "FrameSelection.h"
#include "GeometryUtilities.h"
#include "HTMLConverter.h"
#include "Logging.h"
#include "RenderedDocumentMarker.h"
#include "TextIterator.h"
#include "VisibleUnits.h"
#include "WebContentReader.h"

namespace WebCore {

#pragma mark - Static utility helper methods.

static constexpr auto defaultTextIteratorBehaviors = TextIteratorBehaviors {
    TextIteratorBehavior::EmitsObjectReplacementCharactersForImages,
#if ENABLE(ATTACHMENT_ELEMENT)
    TextIteratorBehavior::EmitsObjectReplacementCharactersForAttachments
#endif
};

CharacterRange UnifiedTextReplacementController::characterRange(const SimpleRange& scope, const SimpleRange& range)
{
    return WebCore::characterRange(scope, range, defaultTextIteratorBehaviors);
}

uint64_t UnifiedTextReplacementController::characterCount(const SimpleRange& range)
{
    return WebCore::characterCount(range, defaultTextIteratorBehaviors);
}

SimpleRange UnifiedTextReplacementController::resolveCharacterRange(const SimpleRange& scope, CharacterRange range)
{
    return WebCore::resolveCharacterRange(scope, range, defaultTextIteratorBehaviors);
}

String UnifiedTextReplacementController::plainText(const SimpleRange& range)
{
    return WebCore::plainText(range, defaultTextIteratorBehaviors);
}

#pragma mark - UnifiedTextReplacementController implementation.

UnifiedTextReplacementController::UnifiedTextReplacementController(Page& page)
    : m_page(page)
{
}

static std::optional<SimpleRange> contextRangeForDocument(const Document& document)
{
    auto selection = document.selection().selection();

    if (selection.isRange()) {
        auto startOfFirstParagraph = startOfParagraph(selection.start());
        auto endOfLastParagraph = endOfParagraph(selection.end());

        auto paragraphRange = makeSimpleRange(startOfFirstParagraph, endOfLastParagraph);

        if (paragraphRange && hasAnyPlainText(*paragraphRange, defaultTextIteratorBehaviors))
            return paragraphRange;
    }

    auto startOfFirstEditableContent = startOfEditableContent(selection.start());
    auto endOfLastEditableContent = endOfEditableContent(selection.end());

    auto editableContentRange = makeSimpleRange(startOfFirstEditableContent, endOfLastEditableContent);
    if (editableContentRange && hasAnyPlainText(*editableContentRange, defaultTextIteratorBehaviors))
        return editableContentRange;

    return selection.firstRange();
}

void UnifiedTextReplacementController::willBeginTextReplacementSession(const std::optional<UnifiedTextReplacement::Session>& session, CompletionHandler<void(const Vector<UnifiedTextReplacement::Context>&)>&& completionHandler)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s)", session ? session->identifier.toString().utf8().data() : "");

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    auto contextRange = contextRangeForDocument(*document);

    if (!contextRange) {
        RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => no context range", session ? session->identifier.toString().utf8().data() : "");
        completionHandler({ });
        return;
    }

    // If the session does not exist, the session is ephemeral.
    if (session) {
        auto liveRange = createLiveRange(*contextRange);

        ASSERT(!m_contextRanges.contains(session->identifier));

        m_contextRanges.set(session->identifier, liveRange);
    }

    auto selectedTextRange = document->selection().selection().firstRange();

    auto attributedStringFromRange = editingAttributedString(*contextRange, { IncludedElement::Images, IncludedElement::Attachments });
    auto selectedTextCharacterRange = characterRange(*contextRange, *selectedTextRange);

    if (attributedStringFromRange.string.isEmpty())
        RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => attributed string is empty", session ? session->identifier.toString().utf8().data() : "");

    if (session) {
        auto attributedStringCharacterCount = attributedStringFromRange.string.length();
        auto contextRangeCharacterCount = characterCount(*contextRange);

        // Postcondition: the selected text character range must be a valid range within the
        // attributed string formed by the context range; the length of the entire context range
        // being equal to the length of the attributed string implies the range is valid.
        if (UNLIKELY(attributedStringCharacterCount != contextRangeCharacterCount)) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => attributed string length (%u) != context range length (%llu)", session->identifier.toString().utf8().data(), attributedStringCharacterCount, contextRangeCharacterCount);
            ASSERT_NOT_REACHED();
            completionHandler({ });
            return;
        }
    }

    completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
}

void UnifiedTextReplacementController::didBeginTextReplacementSession(const UnifiedTextReplacement::Session& session, const Vector<UnifiedTextReplacement::Context>& contexts)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didBeginTextReplacementSession (%s) [received contexts: %zu]", session.identifier.toString().utf8().data(), contexts.size());
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements(const UnifiedTextReplacement::Session& session, const Vector<UnifiedTextReplacement::Replacement>& replacements, const UnifiedTextReplacement::Context& context, bool finished)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements (%s) [received replacements: %zu, finished: %d]", session.identifier.toString().utf8().data(), replacements.size(), finished);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    ASSERT(m_contextRanges.contains(session.identifier));

    // FIXME: Text indicator styles are not used within this method, so is this still needed?
    m_page->chrome().client().removeTextIndicatorStyleForID(session.identifier);

    document->selection().clear();

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    // The tracking of the additional replacement location offset needs to be scoped to a particular instance
    // of this class, instead of just this function, because the function may need to be called multiple times.
    // This ensures that subsequent calls of this function should effectively be treated as just more iterations
    // of the following for-loop.

    auto& additionalOffset = m_replacementLocationOffsets.add(session.identifier, 0).iterator->value;

    for (const auto& replacementData : replacements) {
        auto locationWithOffset = replacementData.originalRange.location + additionalOffset;

        auto resolvedRange = resolveCharacterRange(*sessionRange, { locationWithOffset, replacementData.originalRange.length });

        replaceContentsOfRangeInSession(session.identifier, resolvedRange, replacementData.replacement);

        sessionRange = contextRangeForSessionWithID(session.identifier);
        if (!sessionRange) {
            ASSERT_NOT_REACHED();
            return;
        }

        auto newRangeWithOffset = CharacterRange { locationWithOffset, replacementData.replacement.length() };
        auto newResolvedRange = resolveCharacterRange(*sessionRange, newRangeWithOffset);

        auto originalString = [context.attributedText.nsAttributedString() attributedSubstringFromRange:replacementData.originalRange];

        auto markerData = DocumentMarker::UnifiedTextReplacementData { originalString.string, replacementData.identifier, session.identifier, DocumentMarker::UnifiedTextReplacementData::State::Pending };
        addMarker(newResolvedRange, DocumentMarker::Type::UnifiedTextReplacement, markerData);

        additionalOffset += static_cast<int>(replacementData.replacement.length()) - static_cast<int>(replacementData.originalRange.length);
    }

    if (finished)
        document->selection().setSelection({ *sessionRange });
}

void UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement(const UnifiedTextReplacement::Session& session, UnifiedTextReplacement::Replacement::State state, const UnifiedTextReplacement::Replacement& replacement, const UnifiedTextReplacement::Context&)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement (%s) [new state: %hhu, replacement: %s]", session.identifier.toString().utf8().data(), enumToUnderlyingType(state), replacement.identifier.toString().utf8().data());

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto nodeAndMarker = findReplacementMarkerByID(*sessionRange, replacement.identifier);
    if (!nodeAndMarker)
        return;

    auto& [node, marker] = *nodeAndMarker;

    auto rangeToReplace = makeSimpleRange(node, marker);

    switch (state) {
    case UnifiedTextReplacement::Replacement::State::Active: {
        document->selection().setSelection({ rangeToReplace });
        document->selection().revealSelection();

        auto rect = document->view()->contentsToRootView(unionRect(RenderObject::absoluteTextRects(rangeToReplace)));

        if (CheckedPtr renderStyle = node.renderStyle()) {
            const auto& font = renderStyle->fontCascade();
            auto [_, height] = DocumentMarkerController::markerYPositionAndHeightForFont(font);

            rect.setY(rect.y() + std::round(height / 2.0));
        }

        m_page->chrome().client().textReplacementSessionShowInformationForReplacementWithIDRelativeToRect(session.identifier, replacement.identifier, rect);

        return;
    }

    case UnifiedTextReplacement::Replacement::State::Reverted: {
        auto data = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());

        auto offsetRange = OffsetRange { marker.startOffset(), marker.endOffset() };
        document->markers().removeMarkers(node, offsetRange, { DocumentMarker::Type::UnifiedTextReplacement });

        replaceContentsOfRangeInSession(session.identifier, rangeToReplace, data.originalText);

        return;
    }

    default:
        return;
    }
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange(const UnifiedTextReplacement::Session& session, const AttributedString& attributedText, const CharacterRange& range, const UnifiedTextReplacement::Context& context, bool finished)
{
    auto hasAttributes = attributedText.attributes.containsIf([](const auto& rangeAndAttributeValues) {
        return !rangeAndAttributeValues.second.isEmpty();
    });

    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) [range: %llu, %llu; has attributes: %d; finished: %d]", session.identifier.toString().utf8().data(), range.location, range.length, hasAttributes, finished);

    auto contextTextCharacterCount = context.attributedText.string.length();

    // Precondition: the range is always relative to the context's attributed text, so by definition it must
    // be strictly less than the length of the attributed string.
    if (UNLIKELY(contextTextCharacterCount < range.location + range.length)) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => trying to replace a range larger than the context range (context range length: %u, range.location %llu, range.length %llu)", session.identifier.toString().utf8().data(), contextTextCharacterCount, range.location, range.length);
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().removeTextIndicatorStyleForID(session.identifier);

    document->selection().clear();

    auto sessionRangeCharacterCount = characterCount(*sessionRange);

    if (UNLIKELY(range.length + sessionRangeCharacterCount < contextTextCharacterCount)) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => the range offset by the character count delta must have a non-negative size (context range length: %u, range.length %llu, session length: %llu)", session.identifier.toString().utf8().data(), contextTextCharacterCount, range.length, sessionRangeCharacterCount);
        ASSERT_NOT_REACHED();
        return;
    }

    // The character count delta is `sessionRangeCharacterCount - contextTextCharacterCount`;
    // the above check ensures that the full range length expression will never underflow.

    auto characterCountDelta = sessionRangeCharacterCount - contextTextCharacterCount;
    auto adjustedCharacterRange = CharacterRange { range.location, range.length + characterCountDelta };
    auto resolvedRange = resolveCharacterRange(*sessionRange, adjustedCharacterRange);

    if (!m_originalDocumentNodes.contains(session.identifier)) {
        auto contents = m_contextRanges.get(session.identifier)->cloneContents();
        if (contents.hasException()) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => exception when cloning contents", session.identifier.toString().utf8().data());
            ASSERT_NOT_REACHED();
            return;
        }

        m_originalDocumentNodes.set(session.identifier, contents.returnValue()); // Deep clone.
    }

    RefPtr fragment = createFragment(*document->frame(), attributedText.nsAttributedString().get(), { FragmentCreationOptions::NoInterchangeNewlines, FragmentCreationOptions::SanitizeMarkup });
    if (!fragment) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().addSourceTextIndicatorStyle(session.identifier, range);

    replaceContentsOfRangeInSession(session.identifier, resolvedRange, *fragment, hasAttributes ? MatchStyle::No : MatchStyle::Yes);

    m_page->chrome().client().addDestinationTextIndicatorStyle(session.identifier, adjustedCharacterRange);
}

template<>
void UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction<UnifiedTextReplacement::Session::ReplacementType::PlainText>(const UnifiedTextReplacement::Session& session, UnifiedTextReplacement::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction<PlainText> (%s) [action: %hhu]", session.identifier.toString().utf8().data(), enumToUnderlyingType(action));

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& markers = document->markers();

    markers.forEach<DocumentMarkerController::IterationDirection::Backwards>(*sessionRange, { DocumentMarker::Type::UnifiedTextReplacement }, [&](auto& node, auto& marker) {
        auto rangeToReplace = makeSimpleRange(node, marker);

        auto currentText = plainText(rangeToReplace);

        auto oldData = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());
        auto previousText = oldData.originalText;
        auto offsetRange = OffsetRange { marker.startOffset(), marker.endOffset() };

        markers.removeMarkers(node, offsetRange, { DocumentMarker::Type::UnifiedTextReplacement });

        auto newState = [&] {
            switch (action) {
            case UnifiedTextReplacement::EditAction::Undo:
                return DocumentMarker::UnifiedTextReplacementData::State::Reverted;

            case UnifiedTextReplacement::EditAction::Redo:
                return DocumentMarker::UnifiedTextReplacementData::State::Pending;

            default:
                ASSERT_NOT_REACHED();
                return DocumentMarker::UnifiedTextReplacementData::State::Pending;
            }
        }();

        replaceContentsOfRangeInSession(session.identifier, rangeToReplace, previousText);

        auto newData = DocumentMarker::UnifiedTextReplacementData { currentText, oldData.replacementID, session.identifier, newState };
        auto newOffsetRange = OffsetRange { offsetRange.start, offsetRange.end + previousText.length() - currentText.length() };

        markers.addMarker(node, DocumentMarker { DocumentMarker::Type::UnifiedTextReplacement, newOffsetRange, WTFMove(newData) });

        return false;
    });
}

template<>
void UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction<UnifiedTextReplacement::Session::ReplacementType::RichText>(const UnifiedTextReplacement::Session& session, UnifiedTextReplacement::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction<RichText> (%s) [action: %hhu]", session.identifier.toString().utf8().data(), enumToUnderlyingType(action));

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (m_originalDocumentNodes.isEmpty())
        return;

    auto contents = m_contextRanges.get(session.identifier)->cloneContents();
    if (contents.hasException()) {
        RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) => exception when cloning contents", session.identifier.toString().utf8().data());
        return;
    }

    switch (action) {
    case UnifiedTextReplacement::EditAction::Undo: {
        RefPtr originalFragment = m_originalDocumentNodes.take(session.identifier);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        m_replacedDocumentNodes.set(session.identifier, contents.returnValue()); // Deep clone.
        replaceContentsOfRangeInSession(session.identifier, *sessionRange, *originalFragment);

        break;
    }

    case UnifiedTextReplacement::EditAction::Redo: {
        RefPtr originalFragment = m_replacedDocumentNodes.take(session.identifier);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        m_replacedDocumentNodes.set(session.identifier, contents.returnValue()); // Deep clone.
        replaceContentsOfRangeInSession(session.identifier, *sessionRange, *originalFragment);

        break;
    }

    case UnifiedTextReplacement::EditAction::UndoAll: {
        RefPtr originalFragment = m_originalDocumentNodes.take(session.identifier);
        if (!originalFragment) {
            ASSERT_NOT_REACHED();
            return;
        }

        replaceContentsOfRangeInSession(session.identifier, *sessionRange, *originalFragment);
        m_replacedDocumentNodes.remove(session.identifier);

        break;
    }
    }

    RefPtr updatedLiveRange = m_contextRanges.get(session.identifier);
    if (!updatedLiveRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    switch (action) {
    case UnifiedTextReplacement::EditAction::Undo:
    case UnifiedTextReplacement::EditAction::UndoAll: {
        auto updatedContents = updatedLiveRange->cloneContents();
        if (updatedContents.hasException()) {
            RELEASE_LOG_ERROR(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) => exception when cloning contents after action", session.identifier.toString().utf8().data());
            return;
        }

        m_originalDocumentNodes.set(session.identifier, updatedContents.returnValue()); // Deep clone.

        break;
    }

    case UnifiedTextReplacement::EditAction::Redo:
        break;
    }
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction(const UnifiedTextReplacement::Session& session, UnifiedTextReplacement::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) [action: %hhu]", session.identifier.toString().utf8().data(), enumToUnderlyingType(action));

    switch (session.replacementType) {
    case UnifiedTextReplacement::Session::ReplacementType::PlainText:
        return textReplacementSessionDidReceiveEditAction<UnifiedTextReplacement::Session::ReplacementType::PlainText>(session, action);
    case UnifiedTextReplacement::Session::ReplacementType::RichText:
        return textReplacementSessionDidReceiveEditAction<UnifiedTextReplacement::Session::ReplacementType::RichText>(session, action);
    }
}

template<>
void UnifiedTextReplacementController::didEndTextReplacementSession<UnifiedTextReplacement::Session::ReplacementType::PlainText>(const UnifiedTextReplacement::Session& session, bool accepted)
{
    RefPtr document = this->document();

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& markers = document->markers();

    markers.forEach<DocumentMarkerController::IterationDirection::Backwards>(*sessionRange, { DocumentMarker::Type::UnifiedTextReplacement }, [&](auto& node, auto& marker) {
        auto data = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());

        auto offsetRange = OffsetRange { marker.startOffset(), marker.endOffset() };

        auto rangeToReplace = makeSimpleRange(node, marker);

        markers.removeMarkers(node, offsetRange, { DocumentMarker::Type::UnifiedTextReplacement });

        if (!accepted && data.state != DocumentMarker::UnifiedTextReplacementData::State::Reverted)
            replaceContentsOfRangeInSession(session.identifier, rangeToReplace, data.originalText);

        return false;
    });
}

template<>
void UnifiedTextReplacementController::didEndTextReplacementSession<UnifiedTextReplacement::Session::ReplacementType::RichText>(const UnifiedTextReplacement::Session& session, bool accepted)
{
    if (accepted)
        return;

    textReplacementSessionDidReceiveEditAction<UnifiedTextReplacement::Session::ReplacementType::RichText>(session, UnifiedTextReplacement::EditAction::Undo);
}

void UnifiedTextReplacementController::didEndTextReplacementSession(const UnifiedTextReplacement::Session& session, bool accepted)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didEndTextReplacementSession (%s) [accepted: %d]", session.identifier.toString().utf8().data(), accepted);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    switch (session.replacementType) {
    case UnifiedTextReplacement::Session::ReplacementType::PlainText:
        didEndTextReplacementSession<UnifiedTextReplacement::Session::ReplacementType::PlainText>(session, accepted);
        break;
    case UnifiedTextReplacement::Session::ReplacementType::RichText:
        didEndTextReplacementSession<UnifiedTextReplacement::Session::ReplacementType::RichText>(session, accepted);
        break;
    }

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().removeTextIndicatorStyleForID(session.identifier);

    if (session.correctionType != UnifiedTextReplacement::Session::CorrectionType::Spelling)
        document->selection().setSelection({ *sessionRange });

    m_page->chrome().client().cleanUpTextStylesForSessionID(session.identifier);

    m_contextRanges.remove(session.identifier);
    m_originalDocumentNodes.remove(session.identifier);
    m_replacedDocumentNodes.remove(session.identifier);
    m_replacementLocationOffsets.remove(session.identifier);
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
    auto data = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());

    m_page->chrome().client().textReplacementSessionUpdateStateForReplacementWithID(data.sessionID, UnifiedTextReplacement::Replacement::State::Active, data.replacementID);
}

#pragma mark - Private instance helper methods.

std::optional<SimpleRange> UnifiedTextReplacementController::contextRangeForSessionWithID(const UnifiedTextReplacement::Session::ID& sessionID) const
{
    return makeSimpleRange(m_contextRanges.get(sessionID));
}

RefPtr<Document> UnifiedTextReplacementController::document() const
{
    if (!m_page) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    RefPtr frame = m_page->checkedFocusController()->focusedOrMainFrame();
    if (!frame) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return frame->document();
}

std::optional<std::tuple<Node&, DocumentMarker&>> UnifiedTextReplacementController::findReplacementMarkerByID(const SimpleRange& outerRange, const UnifiedTextReplacement::Replacement::ID& replacementID) const
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr<Node> targetNode;
    WeakPtr<DocumentMarker> targetMarker;

    document->markers().forEach(outerRange, { DocumentMarker::Type::UnifiedTextReplacement }, [&replacementID, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());
        if (data.replacementID != replacementID)
            return false;

        targetNode = &node;
        targetMarker = &marker;

        return true;
    });

    if (targetNode && targetMarker)
        return { { *targetNode, *targetMarker } };

    return std::nullopt;
}

std::optional<std::tuple<Node&, DocumentMarker&>> UnifiedTextReplacementController::findReplacementMarkerContainingRange(const SimpleRange& range) const
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr<Node> targetNode;
    WeakPtr<DocumentMarker> targetMarker;

    document->markers().forEach(range, { DocumentMarker::Type::UnifiedTextReplacement }, [&range, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());

        auto markerRange = makeSimpleRange(node, marker);
        if (!contains(TreeType::ComposedTree, markerRange, range))
            return false;

        targetNode = &node;
        targetMarker = &marker;

        return true;
    });

    if (targetNode && targetMarker)
        return { { *targetNode, *targetMarker } };

    return std::nullopt;
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSessionInternal(const UnifiedTextReplacement::Session::ID& sessionID, const SimpleRange& range, WTF::Function<void(Editor&)>&& replacementOperation)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = contextRangeForSessionWithID(sessionID);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRangeCount = characterCount(*sessionRange);

    auto resolvedCharacterRange = characterRange(*sessionRange, range);
    document->selection().setSelection({ range });

    replacementOperation(document->editor());

    auto selectedTextRange = document->selection().selection().firstRange();
    if (!selectedTextRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto extendedPosition = [](const BoundaryPoint& point, uint64_t characterCount, SelectionDirection direction) {
        auto visiblePosition = VisiblePosition { makeContainerOffsetPosition(point) };

        for (uint64_t i = 0; i < characterCount; ++i) {
            auto nextVisiblePosition = positionOfNextBoundaryOfGranularity(visiblePosition, TextGranularity::CharacterGranularity, direction);
            if (nextVisiblePosition.isNull())
                break;

            visiblePosition = nextVisiblePosition;
        }

        return visiblePosition;
    };

    auto extendedSelection = [extendedPosition](const SimpleRange& range, uint64_t charactersToExtendBackwards, uint64_t charactersToExtendForwards) {
        auto start = extendedPosition(range.start, charactersToExtendBackwards, SelectionDirection::Backward);
        auto end = extendedPosition(range.end, charactersToExtendForwards, SelectionDirection::Forward);

        return makeSimpleRange(start, end);
    };

    auto newSessionRange = extendedSelection(*selectedTextRange, resolvedCharacterRange.location, sessionRangeCount - (resolvedCharacterRange.location + resolvedCharacterRange.length));
    if (!newSessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto updatedLiveRange = createLiveRange(*newSessionRange);
    m_contextRanges.set(sessionID, updatedLiveRange);
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSession(const UnifiedTextReplacement::Session::ID& sessionID, const SimpleRange& range, const String& replacementText)
{
    replaceContentsOfRangeInSessionInternal(sessionID, range, [&replacementText](Editor& editor) {
        editor.replaceSelectionWithText(replacementText, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, EditAction::InsertReplacement);
    });
}

void UnifiedTextReplacementController::replaceContentsOfRangeInSession(const UnifiedTextReplacement::Session::ID& sessionID, const SimpleRange& range, DocumentFragment& fragment, MatchStyle matchStyle)
{
    replaceContentsOfRangeInSessionInternal(sessionID, range, [&fragment, matchStyle](Editor& editor) {
        editor.replaceSelectionWithFragment(fragment, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, matchStyle == MatchStyle::Yes ? Editor::MatchStyle::Yes : Editor::MatchStyle::No, EditAction::InsertReplacement);
    });
}

} // namespace WebKit

#endif
