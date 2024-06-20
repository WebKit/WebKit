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
#import "WritingToolsController.h"

#import "Chrome.h"
#import "ChromeClient.h"
#import "CompositeEditCommand.h"
#import "DocumentInlines.h"
#import "DocumentMarkerController.h"
#import "Editor.h"
#import "FocusController.h"
#import "FrameSelection.h"
#import "GeometryUtilities.h"
#import "HTMLConverter.h"
#import "Logging.h"
#import "NodeRenderStyle.h"
#import "RenderedDocumentMarker.h"
#import "TextIterator.h"
#import "VisibleUnits.h"
#import "WebContentReader.h"
#import <ranges>

namespace WebCore {

#pragma mark - Static utility helper methods.

// To maintain consistency between the traversals of `TextIterator` and `HTMLConverter` within the controller,
// all the WebCore functions which rely on `TextIterator` must use the matching behaviors.
//
// For ease of use, these wrapper functions exist as static class methods on `WritingToolsController` so that
// invoking them without any namespace prefix will default to using these preferred versions.

static constexpr auto defaultTextIteratorBehaviors = TextIteratorBehaviors {
    TextIteratorBehavior::EmitsObjectReplacementCharactersForImages,
#if ENABLE(ATTACHMENT_ELEMENT)
    TextIteratorBehavior::EmitsObjectReplacementCharactersForAttachments
#endif
};

CharacterRange WritingToolsController::characterRange(const SimpleRange& scope, const SimpleRange& range)
{
    return WebCore::characterRange(scope, range, defaultTextIteratorBehaviors);
}

uint64_t WritingToolsController::characterCount(const SimpleRange& range)
{
    return WebCore::characterCount(range, defaultTextIteratorBehaviors);
}

SimpleRange WritingToolsController::resolveCharacterRange(const SimpleRange& scope, CharacterRange range)
{
    return WebCore::resolveCharacterRange(scope, range, defaultTextIteratorBehaviors);
}

String WritingToolsController::plainText(const SimpleRange& range)
{
    return WebCore::plainText(range, defaultTextIteratorBehaviors);
}

#pragma mark - WritingToolsController implementation.

WritingToolsController::WritingToolsController(Page& page)
    : m_page(page)
{
}

static std::optional<SimpleRange> contextRangeForDocument(const Document& document)
{
    // If the selection is a range, the range of the context should be the range of the paragraph
    // surrounding the selection range, unless such a range is empty.
    //
    // Otherwise, the range of the context should be the entire editable range, if not empty.

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

void WritingToolsController::willBeginWritingToolsSession(const std::optional<WritingTools::Session>& session, CompletionHandler<void(const Vector<WritingTools::Context>&)>&& completionHandler)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::willBeginWritingToolsSession (%s)", session ? session->identifier.toString().utf8().data() : "");

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    auto contextRange = contextRangeForDocument(*document);
    if (!contextRange) {
        RELEASE_LOG(WritingTools, "WritingToolsController::willBeginWritingToolsSession (%s) => no context range", session ? session->identifier.toString().utf8().data() : "");
        completionHandler({ });
        return;
    }

    auto selectedTextRange = document->selection().selection().firstRange();

    if (session && session->compositionType == WritingTools::Session::CompositionType::SmartReply) {
        // Smart replies are a unique use case of the Writing Tools delegate methods;
        // - they do not require any context range or attributed string returned back to them from the context
        // - the session context range should only be the selected text range (it should not be expanded)

        ASSERT(session->type == WritingTools::Session::Type::Composition);

        auto liveRange = createLiveRange(*selectedTextRange);
        m_states.set(session->identifier, CompositionState { liveRange, { } });

        completionHandler({ { WTF::UUID { 0 }, AttributedString::fromNSAttributedString(adoptNS([[NSAttributedString alloc] initWithString:@""])), CharacterRange { 0, 0 } } });
        return;
    }

    // The attributed string produced uses all `IncludedElement`s so that no information is lost; each element
    // will be encoded as an NSTextAttachment.

    auto attributedStringFromRange = editingAttributedString(*contextRange, { IncludedElement::Images, IncludedElement::Attachments });
    auto selectedTextCharacterRange = characterRange(*contextRange, *selectedTextRange);

    if (attributedStringFromRange.string.isEmpty())
        RELEASE_LOG(WritingTools, "WritingToolsController::willBeginWritingToolsSession (%s) => attributed string is empty", session ? session->identifier.toString().utf8().data() : "");

    if (!session) {
        // If there is no session, this implies that the Writing Tools delegate is used for the "non-inline editing" case;
        // as such, no mutating delegate methods will be invoked, and so there need not be any state tracked.

        completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
        return;
    }

    ASSERT(!m_states.contains(session->identifier));

    auto liveRange = createLiveRange(*contextRange);

    switch (session->type) {
    case WritingTools::Session::Type::Proofreading:
        m_states.set(session->identifier, ProofreadingState { liveRange, 0 });
        break;

    case WritingTools::Session::Type::Composition:
        m_states.set(session->identifier, CompositionState { liveRange, { } });
        break;
    }

    auto attributedStringCharacterCount = attributedStringFromRange.string.length();
    auto contextRangeCharacterCount = characterCount(*contextRange);

    // Postcondition: the selected text character range must be a valid range within the
    // attributed string formed by the context range; the length of the entire context range
    // being equal to the length of the attributed string implies the range is valid.
    if (UNLIKELY(attributedStringCharacterCount != contextRangeCharacterCount)) {
        RELEASE_LOG_ERROR(WritingTools, "WritingToolsController::willBeginWritingToolsSession (%s) => attributed string length (%u) != context range length (%llu)", session->identifier.toString().utf8().data(), attributedStringCharacterCount, contextRangeCharacterCount);
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
}

void WritingToolsController::didBeginWritingToolsSession(const WritingTools::Session& session, const Vector<WritingTools::Context>& contexts)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::didBeginWritingToolsSession (%s) [received contexts: %zu]", session.identifier.toString().utf8().data(), contexts.size());
}

void WritingToolsController::proofreadingSessionDidReceiveSuggestions(const WritingTools::Session& session, const Vector<WritingTools::TextSuggestion>& suggestions, const WritingTools::Context& context, bool finished)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::proofreadingSessionDidReceiveSuggestions (%s) [received suggestions: %zu, finished: %d]", session.identifier.toString().utf8().data(), suggestions.size(), finished);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    // FIXME: Text indicator styles are not used within this method, so is this still needed?
    m_page->chrome().client().removeTextAnimationForID(session.identifier);

    document->selection().clear();

    CheckedPtr state = stateForSession<WritingTools::Session::Type::Proofreading>(session);
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = makeSimpleRange(state->contextRange);

    // The tracking of the additional replacement location offset needs to be scoped to a particular instance
    // of this class, instead of just this function, because the function may need to be called multiple times.
    // This ensures that subsequent calls of this function should effectively be treated as just more iterations
    // of the following for-loop.

    for (const auto& suggestion : suggestions) {
        // When receiving the suggestions from a proofreading session, immediately replace all the corresponding
        // original text with the replacement text, and add a document marker to each to track them and to be able
        // to add an underline beneath them.

        auto locationWithOffset = suggestion.originalRange.location + state->replacementLocationOffset;

        auto resolvedRange = resolveCharacterRange(sessionRange, { locationWithOffset, suggestion.originalRange.length });

        replaceContentsOfRangeInSession(*state, resolvedRange, suggestion.replacement);

        // After replacement, the session range is "stale", so it needs to be re-computed before being used again.

        sessionRange = makeSimpleRange(state->contextRange);

        auto newRangeWithOffset = CharacterRange { locationWithOffset, suggestion.replacement.length() };
        auto newResolvedRange = resolveCharacterRange(sessionRange, newRangeWithOffset);

        auto originalString = [context.attributedText.nsAttributedString() attributedSubstringFromRange:suggestion.originalRange];

        auto markerData = DocumentMarker::WritingToolsTextSuggestionData { originalString.string, suggestion.identifier, session.identifier, DocumentMarker::WritingToolsTextSuggestionData::State::Accepted };
        addMarker(newResolvedRange, DocumentMarker::Type::WritingToolsTextSuggestion, markerData);

        state->replacementLocationOffset += static_cast<int>(suggestion.replacement.length()) - static_cast<int>(suggestion.originalRange.length);
    }

    if (finished)
        document->selection().setSelection({ sessionRange });
}

void WritingToolsController::proofreadingSessionDidUpdateStateForSuggestion(const WritingTools::Session& session, WritingTools::TextSuggestion::State newTextSuggestionState, const WritingTools::TextSuggestion& textSuggestion, const WritingTools::Context&)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::proofreadingSessionDidUpdateStateForSuggestion (%s) [new state: %hhu, suggestion: %s]", session.identifier.toString().utf8().data(), enumToUnderlyingType(newTextSuggestionState), textSuggestion.identifier.toString().utf8().data());

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr state = stateForSession<WritingTools::Session::Type::Proofreading>(session);
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = makeSimpleRange(state->contextRange);

    auto nodeAndMarker = findTextSuggestionMarkerByID(sessionRange, textSuggestion.identifier);
    if (!nodeAndMarker)
        return;

    auto& [node, marker] = *nodeAndMarker;

    auto rangeToReplace = makeSimpleRange(node, marker);

    switch (newTextSuggestionState) {
    case WritingTools::TextSuggestion::State::Reviewing: {
        // When a given suggestion is "active" / being reviewed, it should be selected, revealed,
        // and then the details popover should be shown for it.

        document->selection().setSelection({ rangeToReplace });
        document->selection().revealSelection();

        // Ensure that the details popover is moved down a tiny bit so that it does not overlap the suggestion underline.

        auto rect = document->view()->contentsToRootView(unionRect(RenderObject::absoluteTextRects(rangeToReplace)));

        if (CheckedPtr renderStyle = node.renderStyle()) {
            const auto& font = renderStyle->fontCascade();
            auto [_, height] = DocumentMarkerController::markerYPositionAndHeightForFont(font);

            rect.setY(rect.y() + std::round(height / 2.0));
        }

        m_page->chrome().client().proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(session.identifier, textSuggestion.identifier, rect);

        return;
    }

    case WritingTools::TextSuggestion::State::Rejected: {
        // When a given suggestion is "reverted" / rejected, remove the marker and replace the suggested text
        // with the original text.

        auto data = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());

        auto offsetRange = OffsetRange { marker.startOffset(), marker.endOffset() };
        document->markers().removeMarkers(node, offsetRange, { DocumentMarker::Type::WritingToolsTextSuggestion });

        replaceContentsOfRangeInSession(*state, rangeToReplace, data.originalText);

        return;
    }

    default:
        return;
    }
}

void WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange(const WritingTools::Session& session, const AttributedString& attributedText, const CharacterRange& range, const WritingTools::Context& context, bool finished)
{
    auto hasAttributes = std::ranges::any_of(attributedText.attributes, [](auto& rangeAndAttributeValues) {
        return !rangeAndAttributeValues.second.isEmpty();
    });

    RELEASE_LOG(WritingTools, "WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange (%s) [range: %llu, %llu; has attributes: %d; finished: %d]", session.identifier.toString().utf8().data(), range.location, range.length, hasAttributes, finished);

    auto contextTextCharacterCount = context.attributedText.string.length();

    // Precondition: the range is always relative to the context's attributed text, so by definition it must
    // be strictly less than the length of the attributed string.
    if (UNLIKELY(contextTextCharacterCount < range.location + range.length)) {
        RELEASE_LOG_ERROR(WritingTools, "WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange (%s) => trying to replace a range larger than the context range (context range length: %u, range.location %llu, range.length %llu)", session.identifier.toString().utf8().data(), contextTextCharacterCount, range.location, range.length);
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr state = stateForSession<WritingTools::Session::Type::Composition>(session);
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().removeTextAnimationForID(session.identifier);

    document->selection().clear();

    auto sessionRange = makeSimpleRange(state->contextRange);
    auto sessionRangeCharacterCount = characterCount(sessionRange);

    if (UNLIKELY(range.length + sessionRangeCharacterCount < contextTextCharacterCount)) {
        RELEASE_LOG_ERROR(WritingTools, "WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange (%s) => the range offset by the character count delta must have a non-negative size (context range length: %u, range.length %llu, session length: %llu)", session.identifier.toString().utf8().data(), contextTextCharacterCount, range.length, sessionRangeCharacterCount);
        ASSERT_NOT_REACHED();
        return;
    }

    // The character count delta is `sessionRangeCharacterCount - contextTextCharacterCount`;
    // the above check ensures that the full range length expression will never underflow.

    auto characterCountDelta = sessionRangeCharacterCount - contextTextCharacterCount;
    auto adjustedCharacterRange = CharacterRange { range.location, range.length + characterCountDelta };
    auto resolvedRange = resolveCharacterRange(sessionRange, adjustedCharacterRange);

    RefPtr fragment = createFragment(*document->frame(), attributedText.nsAttributedString().get(), { FragmentCreationOptions::NoInterchangeNewlines, FragmentCreationOptions::SanitizeMarkup });
    if (!fragment) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().addSourceTextAnimation(session.identifier, range);

    // Prefer using any attributes that `attributedText` may have; however, if it has none,
    // just conduct the replacement so that it matches the style of its surrounding text.
    //
    // This will always be the case for Smart Replies, which creates it's own attributed text
    // without WebKit providing the attributes.

    replaceContentsOfRangeInSession(*state, resolvedRange, WTFMove(fragment), hasAttributes ? MatchStyle::No : MatchStyle::Yes);

    m_page->chrome().client().addDestinationTextAnimation(session.identifier, adjustedCharacterRange);
}

template<>
void WritingToolsController::writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Proofreading>(const WritingTools::Session& session, WritingTools::Action action)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::writingToolsSessionDidReceiveAction<Proofreading> (%s) [action: %hhu]", session.identifier.toString().utf8().data(), enumToUnderlyingType(action));

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr state = stateForSession<WritingTools::Session::Type::Proofreading>(session);
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = makeSimpleRange(state->contextRange);

    auto& markers = document->markers();

    markers.forEach<DocumentMarkerController::IterationDirection::Backwards>(sessionRange, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&](auto& node, auto& marker) {
        auto rangeToReplace = makeSimpleRange(node, marker);

        auto currentText = plainText(rangeToReplace);

        auto oldData = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());
        auto previousText = oldData.originalText;
        auto offsetRange = OffsetRange { marker.startOffset(), marker.endOffset() };

        markers.removeMarkers(node, offsetRange, { DocumentMarker::Type::WritingToolsTextSuggestion });

        auto newState = [&] {
            switch (action) {
            case WritingTools::Action::ShowOriginal:
                return DocumentMarker::WritingToolsTextSuggestionData::State::Rejected;

            case WritingTools::Action::ShowRewritten:
                return DocumentMarker::WritingToolsTextSuggestionData::State::Accepted;

            default:
                ASSERT_NOT_REACHED();
                return DocumentMarker::WritingToolsTextSuggestionData::State::Accepted;
            }
        }();

        replaceContentsOfRangeInSession(*state, rangeToReplace, previousText);

        auto newData = DocumentMarker::WritingToolsTextSuggestionData { currentText, oldData.suggestionID, session.identifier, newState };
        auto newOffsetRange = OffsetRange { offsetRange.start, offsetRange.end + previousText.length() - currentText.length() };

        markers.addMarker(node, DocumentMarker { DocumentMarker::Type::WritingToolsTextSuggestion, newOffsetRange, WTFMove(newData) });

        return false;
    });
}

template<>
void WritingToolsController::writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Composition>(const WritingTools::Session& session, WritingTools::Action action)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::writingToolsSessionDidReceiveAction<Composition> (%s) [action: %hhu]", session.identifier.toString().utf8().data(), enumToUnderlyingType(action));

    CheckedPtr state = stateForSession<WritingTools::Session::Type::Composition>(session);
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    // In composition sessions, text can be received in "chunks", and is therefore stored as a stack
    // of EditCommand's. When showing the original content, or when restarting, the commands are undone
    // starting from the last one. Likewise, when showing the rewritten content, the commands are redone
    // preserving the original order.

    switch (action) {
    case WritingTools::Action::ShowOriginal: {
        for (const auto& command : state->commands | std::views::reverse)
            command->ensureComposition().unapply();

        break;
    }

    case WritingTools::Action::ShowRewritten: {
        for (const auto& command : state->commands)
            command->ensureComposition().reapply();

        break;
    }

    case WritingTools::Action::Restart: {
        for (const auto& command : state->commands | std::views::reverse)
            command->ensureComposition().unapply();

        state->commands.clear();

        break;
    }
    }
}

void WritingToolsController::writingToolsSessionDidReceiveAction(const WritingTools::Session& session, WritingTools::Action action)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::writingToolsSessionDidReceiveAction (%s) [action: %hhu]", session.identifier.toString().utf8().data(), enumToUnderlyingType(action));

    switch (session.type) {
    case WritingTools::Session::Type::Proofreading: {
        writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Proofreading>(session, action);
        break;
    }

    case WritingTools::Session::Type::Composition: {
        writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Composition>(session, action);
        break;
    }
    }
}

template<>
void WritingToolsController::didEndWritingToolsSession<WritingTools::Session::Type::Proofreading>(const WritingTools::Session& session, bool accepted)
{
    RefPtr document = this->document();

    CheckedPtr state = stateForSession<WritingTools::Session::Type::Proofreading>(session);
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = makeSimpleRange(state->contextRange);

    auto& markers = document->markers();

    // If the session as a whole is not accepted, revert all the suggestions to their original text.

    markers.forEach<DocumentMarkerController::IterationDirection::Backwards>(sessionRange, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&](auto& node, auto& marker) {
        auto data = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());

        auto offsetRange = OffsetRange { marker.startOffset(), marker.endOffset() };

        auto rangeToReplace = makeSimpleRange(node, marker);

        markers.removeMarkers(node, offsetRange, { DocumentMarker::Type::WritingToolsTextSuggestion });

        if (!accepted && data.state != DocumentMarker::WritingToolsTextSuggestionData::State::Rejected)
            replaceContentsOfRangeInSession(*state, rangeToReplace, data.originalText);

        return false;
    });
}

template<>
void WritingToolsController::didEndWritingToolsSession<WritingTools::Session::Type::Composition>(const WritingTools::Session& session, bool accepted)
{
    if (accepted)
        return;

    // If the session was not accepted, undo all the changes. This is essentially just the same as invoking the "show original" action.

    writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Composition>(session, WritingTools::Action::ShowOriginal);
}

void WritingToolsController::didEndWritingToolsSession(const WritingTools::Session& session, bool accepted)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::didEndWritingToolsSession (%s) [accepted: %d]", session.identifier.toString().utf8().data(), accepted);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    switch (session.type) {
    case WritingTools::Session::Type::Proofreading:
        didEndWritingToolsSession<WritingTools::Session::Type::Proofreading>(session, accepted);
        break;
    case WritingTools::Session::Type::Composition:
        didEndWritingToolsSession<WritingTools::Session::Type::Composition>(session, accepted);
        break;
    }

    auto sessionRange = contextRangeForSessionWithID(session.identifier);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().removeTextAnimationForID(session.identifier);

    // At this point, the selection will be the replaced text, which is the desired behavior for
    // Smart Reply sessions. However, for others, the entire session context range should be selected.

    if (session.compositionType != WritingTools::Session::CompositionType::SmartReply)
        document->selection().setSelection({ *sessionRange });

    m_page->chrome().client().cleanUpTextAnimationsForSessionID(session.identifier);

    m_states.remove(session.identifier);
}

void WritingToolsController::updateStateForSelectedSuggestionIfNeeded()
{
    // Optimization: If there are no ongoing sessions, there is no need for any of this logic to
    // be executed, since there will be no relevant document markers anyways.
    if (m_states.isEmpty())
        return;

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    // When clicking/tapping on a word that corresponds with a suggestion, mark it as being "reviewed",
    // which will also invoke the details popover.

    auto selectionRange = document->selection().selection().firstRange();
    if (!selectionRange)
        return;

    if (!document->selection().isCaret())
        return;

    auto nodeAndMarker = findTextSuggestionMarkerContainingRange(*selectionRange);
    if (!nodeAndMarker)
        return;

    auto& [node, marker] = *nodeAndMarker;
    auto data = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());

    m_page->chrome().client().proofreadingSessionUpdateStateForSuggestionWithID(data.sessionID, WritingTools::TextSuggestion::State::Reviewing, data.suggestionID);
}

#pragma mark - Private instance helper methods.

std::optional<SimpleRange> WritingToolsController::contextRangeForSessionWithID(const WritingTools::Session::ID& sessionID) const
{
    auto it = m_states.find(sessionID);
    if (it == m_states.end())
        return std::nullopt;

    auto range = WTF::switchOn(it->value,
        [](std::monostate) -> Ref<Range> { RELEASE_ASSERT_NOT_REACHED(); },
        [](const ProofreadingState& state) { return state.contextRange; },
        [](const CompositionState& state) { return state.contextRange; }
    );

    return makeSimpleRange(range);
}

template<WritingTools::Session::Type Type>
WritingToolsController::StateFromSessionType<Type>::Value* WritingToolsController::stateForSession(const WritingTools::Session& session)
{
    if (UNLIKELY(session.type != Type)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto it = m_states.find(session.identifier);
    if (UNLIKELY(it == m_states.end())) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return std::get_if<typename WritingToolsController::StateFromSessionType<Type>::Value>(&it->value);
}

RefPtr<Document> WritingToolsController::document() const
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

std::optional<std::tuple<Node&, DocumentMarker&>> WritingToolsController::findTextSuggestionMarkerByID(const SimpleRange& outerRange, const WritingTools::TextSuggestion::ID& textSuggestionID) const
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr<Node> targetNode;
    WeakPtr<DocumentMarker> targetMarker;

    document->markers().forEach(outerRange, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&textSuggestionID, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());
        if (data.suggestionID != textSuggestionID)
            return false;

        targetNode = &node;
        targetMarker = &marker;

        return true;
    });

    if (targetNode && targetMarker)
        return { { *targetNode, *targetMarker } };

    return std::nullopt;
}

std::optional<std::tuple<Node&, DocumentMarker&>> WritingToolsController::findTextSuggestionMarkerContainingRange(const SimpleRange& range) const
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr<Node> targetNode;
    WeakPtr<DocumentMarker> targetMarker;

    document->markers().forEach(range, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&range, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<DocumentMarker::WritingToolsTextSuggestionData>(marker.data());

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

template<typename State>
void WritingToolsController::replaceContentsOfRangeInSessionInternal(State& state, const SimpleRange& range, WTF::Function<void()>&& replacementOperation)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto sessionRange = makeSimpleRange(state.contextRange);
    auto sessionRangeCount = characterCount(sessionRange);
    auto resolvedCharacterRange = characterRange(sessionRange, range);

    document->selection().setSelection({ range });

    replacementOperation();

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
    state.contextRange = updatedLiveRange;
}

void WritingToolsController::replaceContentsOfRangeInSession(ProofreadingState& state, const SimpleRange& range, const String& replacementText)
{
    replaceContentsOfRangeInSessionInternal(state, range, [&] {
        RefPtr document = this->document();
        document->editor().replaceSelectionWithText(replacementText, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, EditAction::InsertReplacement);
    });
}

void WritingToolsController::replaceContentsOfRangeInSession(CompositionState& state, const SimpleRange& range, RefPtr<DocumentFragment>&& fragment, MatchStyle matchStyle)
{
    OptionSet<ReplaceSelectionCommand::CommandOption> options { ReplaceSelectionCommand::PreventNesting, ReplaceSelectionCommand::SanitizeFragment, ReplaceSelectionCommand::SelectReplacement };
    if (matchStyle == MatchStyle::Yes)
        options.add(ReplaceSelectionCommand::MatchStyle);

    replaceContentsOfRangeInSessionInternal(state, range, [&] {
        RefPtr document = this->document();

        auto selection = document->selection().selection();
        if (selection.isNone() || !selection.isContentEditable())
            return;

        auto command = WebCore::ReplaceSelectionCommand::create(Ref { *document }, WTFMove(fragment), options, WebCore::EditAction::InsertReplacement);
        command->apply();

        // The commands are stored in the state in case the original contents need to be used again.
        state.commands.append(command);
    });
}

} // namespace WebKit

#endif
