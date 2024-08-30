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
#import "TextAnimationTypes.h"
#import "TextIterator.h"
#import "VisibleUnits.h"
#import "WebContentReader.h"
#import <ranges>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(WritingToolsController);
WTF_MAKE_TZONE_ALLOCATED_IMPL_NESTED(WritingToolsControllerEditingScope, WritingToolsController::EditingScope);

#pragma mark - EditingScope

WritingToolsController::EditingScope::EditingScope(Document& document)
    : m_document(&document)
    , m_editingWasSuppressed(document.editor().suppressEditingForWritingTools())
{
    document.editor().setSuppressEditingForWritingTools(false);
}

WritingToolsController::EditingScope::~EditingScope()
{
    m_document->editor().setSuppressEditingForWritingTools(m_editingWasSuppressed);
}

#pragma mark - Overloaded TextIterator-based static functions.

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

#pragma mark - Static utility helper methods.

static std::optional<SimpleRange> contextRangeForSession(Document& document, const std::optional<WritingTools::Session>& session)
{
    // If the selection is a range, the range of the context should be the range of the paragraph
    // surrounding the selection range, unless such a range is empty.
    //
    // Otherwise, the range of the context should be the entire editable range, if not empty.

    auto selection = document.selection().selection();

    if (session && session->compositionType == WritingTools::Session::CompositionType::SmartReply) {
        // The session context range for Smart Replies should only be the selected text range (it should not be expanded).
        return selection.firstRange();
    }

    if (!session || session->compositionType != WritingTools::Session::CompositionType::Compose) {
        // If the session is a Compose session, the range should be the range of the entire editable content.

        if (selection.isRange()) {
            auto startOfFirstParagraph = startOfParagraph(selection.start());
            auto endOfLastParagraph = endOfParagraph(selection.end());

            auto paragraphRange = makeSimpleRange(startOfFirstParagraph, endOfLastParagraph);

            if (paragraphRange && hasAnyPlainText(*paragraphRange, defaultTextIteratorBehaviors))
                return paragraphRange;
        }
    }

    if (selection.isNone())
        return makeRangeSelectingNodeContents(document);

    auto startOfFirstEditableContent = startOfEditableContent(selection.start());
    auto endOfLastEditableContent = endOfEditableContent(selection.end());

    auto editableContentRange = makeSimpleRange(startOfFirstEditableContent, endOfLastEditableContent);
    if (editableContentRange && hasAnyPlainText(*editableContentRange, defaultTextIteratorBehaviors))
        return editableContentRange;

    return selection.firstRange();
}

#pragma mark - WritingToolsController implementation.

WritingToolsController::WritingToolsController(Page& page)
    : m_page(page)
{
}

#pragma mark - Delegate methods.

void WritingToolsController::willBeginWritingToolsSession(const std::optional<WritingTools::Session>& session, CompletionHandler<void(const Vector<WritingTools::Context>&)>&& completionHandler)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::willBeginWritingToolsSession (%s)", session ? session->identifier.toString().utf8().data() : "");

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    auto contextRange = contextRangeForSession(*document, session);
    if (!contextRange) {
        RELEASE_LOG(WritingTools, "WritingToolsController::willBeginWritingToolsSession (%s) => no context range", session ? session->identifier.toString().utf8().data() : "");
        completionHandler({ });
        return;
    }

    if (session && session->compositionType == WritingTools::Session::CompositionType::SmartReply) {
        // Smart replies are a unique use case of the Writing Tools delegate methods;
        // - they do not require any context range or attributed string returned back to them from the context
        // - the session context range should only be the selected text range (it should not be expanded)

        ASSERT(session->type == WritingTools::Session::Type::Composition);

        m_state = CompositionState { { }, { WritingToolsCompositionCommand::create(Ref { *document }, *contextRange) }, *session };

        completionHandler({ { WTF::UUID { 0 }, AttributedString::fromNSAttributedString(adoptNS([[NSAttributedString alloc] initWithString:@""])), CharacterRange { 0, 0 } } });
        return;
    }

    // The attributed string produced uses all `IncludedElement`s so that no information is lost; each element
    // will be encoded as an NSTextAttachment.

    auto selectedTextRange = document->selection().selection().firstRange();

    auto attributedStringFromRange = editingAttributedString(*contextRange, { IncludedElement::Images, IncludedElement::Attachments, IncludedElement::PreservedContent });
    auto selectedTextCharacterRange = selectedTextRange ? characterRange(*contextRange, *selectedTextRange) : CharacterRange { };

    if (attributedStringFromRange.string.isEmpty())
        RELEASE_LOG(WritingTools, "WritingToolsController::willBeginWritingToolsSession (%s) => attributed string is empty", session ? session->identifier.toString().utf8().data() : "");

    if (!session) {
        // If there is no session, this implies that the Writing Tools delegate is used for the "non-inline editing" case;
        // as such, no mutating delegate methods will be invoked, and so there need not be any state tracked.

        completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
        return;
    }

    switch (session->type) {
    case WritingTools::Session::Type::Proofreading:
        m_state = ProofreadingState { createLiveRange(*contextRange), *session, 0 };
        break;

    case WritingTools::Session::Type::Composition:
        // A sentinel command is always initially created to represent the initial state;
        // the command itself is never applied or unapplied.
        //
        // The range associated with each command is the resulting context range after the command is applied.
        m_state = CompositionState { { }, { WritingToolsCompositionCommand::create(Ref { *document }, *contextRange) }, *session };
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

    document->editor().setSuppressEditingForWritingTools(true);

    completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
}

void WritingToolsController::didBeginWritingToolsSession(const WritingTools::Session&, const Vector<WritingTools::Context>& contexts)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::didBeginWritingToolsSession [received contexts: %zu]", contexts.size());
}

void WritingToolsController::proofreadingSessionDidReceiveSuggestions(const WritingTools::Session&, const Vector<WritingTools::TextSuggestion>& suggestions, const WritingTools::Context& context, bool finished)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::proofreadingSessionDidReceiveSuggestion [received suggestions: %zu, finished: %d]", suggestions.size(), finished);

    CheckedPtr state = currentState<WritingTools::Session::Type::Proofreading>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().removeInitialTextAnimationForActiveWritingToolsSession();

    document->selection().clear();

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

        auto markerData = DocumentMarker::WritingToolsTextSuggestionData { originalString.string, suggestion.identifier, DocumentMarker::WritingToolsTextSuggestionData::State::Accepted };
        addMarker(newResolvedRange, DocumentMarker::Type::WritingToolsTextSuggestion, markerData);

        state->replacementLocationOffset += static_cast<int>(suggestion.replacement.length()) - static_cast<int>(suggestion.originalRange.length);
    }

    if (finished) {
        document->editor().setSuppressEditingForWritingTools(false);
        document->selection().setSelection({ sessionRange });
    }
}

void WritingToolsController::proofreadingSessionDidUpdateStateForSuggestion(const WritingTools::Session&, WritingTools::TextSuggestion::State newTextSuggestionState, const WritingTools::TextSuggestion& textSuggestion, const WritingTools::Context&)
{
    CheckedPtr state = currentState<WritingTools::Session::Type::Proofreading>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    RELEASE_LOG(WritingTools, "WritingToolsController::proofreadingSessionDidUpdateStateForSuggestion (%s) [new state: %hhu, suggestion: %s]", state->session.identifier.toString().utf8().data(), enumToUnderlyingType(newTextSuggestionState), textSuggestion.identifier.toString().utf8().data());

    RefPtr document = this->document();
    if (!document) {
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

        m_page->chrome().client().proofreadingSessionShowDetailsForSuggestionWithIDRelativeToRect(textSuggestion.identifier, rect);

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

void WritingToolsController::showSelection() const
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr state = std::get_if<CompositionState>(&m_state);
    if (!state)
        return;

    if (state->reappliedCommands.isEmpty()) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto selectionRange = state->reappliedCommands.last()->endingSelection().firstRange();
    if (!selectionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto visibleSelection = VisibleSelection { *selectionRange };
    if (visibleSelection.isNoneOrOrphaned())
        return;

    document->selection().setSelection(visibleSelection);
}

void WritingToolsController::compositionSessionDidFinishReplacement()
{
    // An empty optional range implies that an animation should be considered to have already been finished.
    m_page->chrome().client().addDestinationTextAnimationForActiveWritingToolsSession(std::nullopt, ""_s);
}

void WritingToolsController::compositionSessionDidFinishReplacement(const CharacterRange& updatedRange, const String& replacementText)
{
    m_page->chrome().client().addDestinationTextAnimationForActiveWritingToolsSession(updatedRange, replacementText);
}

void WritingToolsController::compositionSessionDidReceiveTextWithReplacementRangeAsync(const AttributedString& attributedText, const CharacterRange& range, const WritingTools::Context& context, bool finished, TextAnimationRunMode runMode)
{
    RefPtr document = this->document();
    if (!document) {
        compositionSessionDidFinishReplacement();
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state) {
        compositionSessionDidFinishReplacement();
        ASSERT_NOT_REACHED();
        return;
    }

    if (runMode == WebCore::TextAnimationRunMode::DoNotRun) {
        compositionSessionDidFinishReplacement();
        return;
    }

    auto contextTextCharacterCount = context.attributedText.string.length();

    // Precondition: the range is always relative to the context's attributed text, so by definition it must
    // be strictly less than the length of the attributed string.
    if (UNLIKELY(contextTextCharacterCount < range.location + range.length)) {
        RELEASE_LOG_ERROR(WritingTools, "WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange (%s) => trying to replace a range larger than the context range (context range length: %u, range.location %llu, range.length %llu)", state->session.identifier.toString().utf8().data(), contextTextCharacterCount, range.location, range.length);
        compositionSessionDidFinishReplacement();
        ASSERT_NOT_REACHED();
        return;
    }

    // When `didReceiveText` is invoked multiple times, subsequent invocations will always have their replacement text as a superset
    // of the prior invocations' text. Therefore, this can effectively be modeled as the prior replacement being undone, and then the
    // current replacement being applied.
    //
    // This is specifically needed in the case where tables and/or lists are the replacement text, since it is impossible to construct
    // a selection that is just "outside" one of these elements.

    Ref currentCommand = state->reappliedCommands.takeLast();

    // Save all the existing transparent document markers before the command is undone, since such
    // operation is not guaranteed to persist all the markers; some may be discarded if the nodes end
    // up changing.
    //
    // The character ranges are relative to the range of the document, since the document node is the
    // only guaranteed common ancestor node before the undo and after the replace.

    Vector<std::tuple<CharacterRange, WTF::UUID>> existingTransparentContentDocumentMarkers;

    auto documentRange = makeRangeSelectingNodeContents(*document);
    document->markers().forEach(documentRange, { DocumentMarker::Type::TransparentContent }, [&](auto& node, auto& marker) mutable {
        auto markerRange = makeSimpleRange(node, marker);
        auto data = std::get<DocumentMarker::TransparentContentData>(marker.data());

        auto resolvedCharacterRange = characterRange(documentRange, markerRange);
        existingTransparentContentDocumentMarkers.append({ resolvedCharacterRange, data.uuid });

        return false;
    });

    // The prior replacement command must be undone in such a way as to not have it be added to the undo stack
    currentCommand->ensureComposition().unapply(EditCommandComposition::AddToUndoStack::No);

    // Now that the prior replacement command is undone, remove and replace it with a fresh command, with the same range.
    // The same range is used since it represents the end of the previous command, and is only updated again when `finished` is `true`,
    // at which point this will not be invoked again.

    auto currentContextRange = currentCommand->endingContextRange();
    state->reappliedCommands.append(WritingToolsCompositionCommand::create(Ref { *document }, currentContextRange));

    // The current session context range is always the range associated with the most recently applied command.
    auto sessionRange = state->reappliedCommands.last()->endingContextRange();
    auto sessionRangeCharacterCount = characterCount(sessionRange);

    if (UNLIKELY(range.length + sessionRangeCharacterCount < contextTextCharacterCount)) {
        RELEASE_LOG_ERROR(WritingTools, "WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange (%s) => the range offset by the character count delta must have a non-negative size (context range length: %u, range.length %llu, session length: %llu)", state->session.identifier.toString().utf8().data(), contextTextCharacterCount, range.length, sessionRangeCharacterCount);
        compositionSessionDidFinishReplacement();
        ASSERT_NOT_REACHED();
        return;
    }

    // The character count delta is `sessionRangeCharacterCount - contextTextCharacterCount`;
    // the above check ensures that the full range length expression will never underflow.
    auto characterCountDelta = sessionRangeCharacterCount - contextTextCharacterCount;
    auto adjustedCharacterRange = CharacterRange { range.location, range.length + characterCountDelta };
    auto resolvedRange = resolveCharacterRange(sessionRange, adjustedCharacterRange);

    // Prefer using any attributes that `attributedText` may have; however, if it has none,
    // just conduct the replacement so that it matches the style of its surrounding text.
    //
    // This will always be the case for Smart Replies, which creates it's own attributed text
    // without WebKit providing the attributes.

    auto commandState = finished ? WritingToolsCompositionCommand::State::Complete : WritingToolsCompositionCommand::State::InProgress;
    replaceContentsOfRangeInSession(*state, resolvedRange, attributedText, commandState);

    // Restore the transparent content document markers after the undo + replace is complete.
    // Since only some markers may have been discarded, all transparent content markers are first
    // removed, and then the saved ones are re-added. This ensures that there are not duplicate
    // markers added.
    //
    // This methodology is valid since subsequent replacements always have a prefix that is their prior replacement.

    document->markers().removeMarkers({ WebCore::DocumentMarker::Type::TransparentContent });

    for (const auto& [characterRange, markerIdentifier] : existingTransparentContentDocumentMarkers) {
        auto resolvedRange = resolveCharacterRange(documentRange, characterRange);
        document->markers().addTransparentContentMarker(resolvedRange, markerIdentifier);
    }

    if (runMode == TextAnimationRunMode::OnlyReplaceText) {
        compositionSessionDidFinishReplacement();
        return;
    }

    // FIXME: We won't be setting the selection after every replace, we need a different way to
    // calculate this range.
    auto selectionRange = state->reappliedCommands.last()->endingSelection().firstRange();
    if (!selectionRange) {
        compositionSessionDidFinishReplacement();
        ASSERT_NOT_REACHED();
        return;
    }

    auto rangeAfterReplace = characterRange(sessionRange, *selectionRange);

    compositionSessionDidFinishReplacement(rangeAfterReplace, attributedText.string);
    document->selection().clear();
}

void WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange(const WritingTools::Session&, const AttributedString& attributedText, const CharacterRange& range, const WritingTools::Context& context, bool finished)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::compositionSessionDidReceiveTextWithReplacementRange [range: %llu, %llu; finished: %d]", range.location, range.length, finished);

    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().removeInitialTextAnimationForActiveWritingToolsSession();

    auto addDestinationTextAnimation = [weakThis = WeakPtr { *this }, attributedText, range, context, finished](TextAnimationRunMode runMode) mutable {
        weakThis->compositionSessionDidReceiveTextWithReplacementRangeAsync(attributedText, range, context, finished, runMode);
    };

#if PLATFORM(MAC)
    m_page->chrome().client().addSourceTextAnimationForActiveWritingToolsSession(range, attributedText.string, WTFMove(addDestinationTextAnimation));
#else
    addDestinationTextAnimation(WebCore::TextAnimationRunMode::RunAnimation);
#endif
}

template<>
void WritingToolsController::writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Proofreading>(WritingTools::Action action)
{
    CheckedPtr state = currentState<WritingTools::Session::Type::Proofreading>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    RELEASE_LOG(WritingTools, "WritingToolsController::writingToolsSessionDidReceiveAction<Proofreading> (%s) [action: %hhu]", state->session.identifier.toString().utf8().data(), enumToUnderlyingType(action));

    RefPtr document = this->document();
    if (!document) {
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

        auto newData = DocumentMarker::WritingToolsTextSuggestionData { currentText, oldData.suggestionID, newState };
        auto newOffsetRange = OffsetRange { offsetRange.start, offsetRange.end + previousText.length() - currentText.length() };

        markers.addMarker(node, DocumentMarker { DocumentMarker::Type::WritingToolsTextSuggestion, newOffsetRange, WTFMove(newData) });

        return false;
    });
}

template<>
void WritingToolsController::writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Composition>(WritingTools::Action action)
{
    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    RELEASE_LOG(WritingTools, "WritingToolsController::writingToolsSessionDidReceiveAction<Composition> [action: %hhu]", enumToUnderlyingType(action));

    switch (action) {
    case WritingTools::Action::ShowOriginal: {
        showOriginalCompositionForSession();
        return;
    }

    case WritingTools::Action::ShowRewritten: {
        showRewrittenCompositionForSession();
        return;
    }

    case WritingTools::Action::Restart: {
        restartCompositionForSession();
        return;
    }
    }
}

void WritingToolsController::writingToolsSessionDidReceiveAction(const WritingTools::Session& session, WritingTools::Action action)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::writingToolsSessionDidReceiveAction [action: %hhu]", enumToUnderlyingType(action));

    switch (session.type) {
    case WritingTools::Session::Type::Proofreading: {
        writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Proofreading>(action);
        break;
    }

    case WritingTools::Session::Type::Composition: {
        writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Composition>(action);
        break;
    }
    }
}

template<>
void WritingToolsController::didEndWritingToolsSession<WritingTools::Session::Type::Proofreading>(bool accepted)
{
    RefPtr document = this->document();

    CheckedPtr state = currentState<WritingTools::Session::Type::Proofreading>();
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
void WritingToolsController::didEndWritingToolsSession<WritingTools::Session::Type::Composition>(bool accepted)
{
    if (accepted)
        return;

    // If the session was not accepted, undo all the changes. This is essentially just the same as invoking the "show original" action.

    writingToolsSessionDidReceiveAction<WritingTools::Session::Type::Composition>(WritingTools::Action::ShowOriginal);
}

void WritingToolsController::didEndWritingToolsSession(const WritingTools::Session& session, bool accepted)
{
    RELEASE_LOG(WritingTools, "WritingToolsController::didEndWritingToolsSession [accepted: %d]", accepted);

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    document->editor().setSuppressEditingForWritingTools(false);

    switch (session.type) {
    case WritingTools::Session::Type::Proofreading:
        didEndWritingToolsSession<WritingTools::Session::Type::Proofreading>(accepted);
        break;
    case WritingTools::Session::Type::Composition:
        didEndWritingToolsSession<WritingTools::Session::Type::Composition>(accepted);
        break;
    }

    auto sessionRange = activeSessionRange();
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().removeInitialTextAnimationForActiveWritingToolsSession();

    m_page->chrome().client().removeTransparentMarkersForActiveWritingToolsSession();

    m_state = { };
}

#pragma mark - Methods invoked via editing.

void WritingToolsController::updateStateForSelectedSuggestionIfNeeded()
{
    // Optimization: If there are no ongoing sessions, there is no need for any of this logic to
    // be executed, since there will be no relevant document markers anyways.
    CheckedPtr state = currentState<WritingTools::Session::Type::Proofreading>();
    if (!state)
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

    m_page->chrome().client().proofreadingSessionUpdateStateForSuggestionWithID(WritingTools::TextSuggestion::State::Reviewing, data.suggestionID);
}

static bool appliedCommandIsWritingToolsCommand(const Vector<Ref<WritingToolsCompositionCommand>>& commands, EditCommandComposition* composition)
{
    return std::ranges::any_of(commands, [composition](const auto& command) {
        return &command->ensureComposition() == composition;
    });
}

void WritingToolsController::respondToUnappliedEditing(EditCommandComposition* composition)
{
    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state)
        return;

    if (!appliedCommandIsWritingToolsCommand(state->reappliedCommands, composition))
        return;

    state->unappliedCommands.append(state->reappliedCommands.takeLast());
}

void WritingToolsController::respondToReappliedEditing(EditCommandComposition* composition)
{
    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state)
        return;

    if (!appliedCommandIsWritingToolsCommand(state->unappliedCommands, composition))
        return;

    state->reappliedCommands.append(state->unappliedCommands.takeLast());
}

#pragma mark - Private instance helper methods.

std::optional<SimpleRange> WritingToolsController::activeSessionRange() const
{
    return WTF::switchOn(m_state,
        [](std::monostate) -> std::optional<SimpleRange> { return std::nullopt; },
        [](const ProofreadingState& state) -> std::optional<SimpleRange> { return makeSimpleRange(state.contextRange); },
        [](const CompositionState& state) -> std::optional<SimpleRange> { return state.reappliedCommands.last()->currentContextRange(); }
    );
}

template<WritingTools::Session::Type Type>
WritingToolsController::StateFromSessionType<Type>::Value* WritingToolsController::currentState()
{
    return std::get_if<typename WritingToolsController::StateFromSessionType<Type>::Value>(&m_state);
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

void WritingToolsController::showOriginalCompositionForSession()
{
    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& stack = state->reappliedCommands;

    // Ensure that the sentinel command is never undone or removed from the stack.
    while (stack.size() > 1) {
        auto oldSize = stack.size();

        // Each call to `unapply` indirectly results in a call to `respondToUnappliedEditing`, which decrements the size of the stack.
        stack.last()->ensureComposition().unapply();

        RELEASE_ASSERT(oldSize > stack.size());
    }
}

void WritingToolsController::showRewrittenCompositionForSession()
{
    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& stack = state->unappliedCommands;

    m_page->chrome().client().setIsInRedo(true);

    while (!stack.isEmpty()) {
        auto oldSize = stack.size();

        // Each call to `reapply` indirectly results in a call to `respondToReappliedEditing`, which decrements the size of the stack.
        stack.last()->ensureComposition().reapply();

        RELEASE_ASSERT(oldSize > stack.size());
    }

    m_page->chrome().client().setIsInRedo(false);
}

void WritingToolsController::restartCompositionForSession()
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    CheckedPtr state = currentState<WritingTools::Session::Type::Composition>();
    if (!state) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_page->chrome().client().clearAnimationsForActiveWritingToolsSession();

    // Don't animate smart replies, they are animated by UIKit/AppKit.
    if (state->session.compositionType != WebCore::WritingTools::Session::CompositionType::SmartReply) {
        document->selection().clear();
        m_page->chrome().client().addInitialTextAnimationForActiveWritingToolsSession();
    }

    // The stack will never be empty as the sentinel command always exists.
    auto currentContextRange = state->reappliedCommands.last()->endingContextRange();
    state->reappliedCommands.append(WritingToolsCompositionCommand::create(Ref { *document }, currentContextRange));
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

    document->markers().forEach(outerRange, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&textSuggestionID, &targetNode, &targetMarker](auto& node, auto& marker) mutable {
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

    document->markers().forEach(range, { DocumentMarker::Type::WritingToolsTextSuggestion }, [&range, &targetNode, &targetMarker](auto& node, auto& marker) mutable {
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

void WritingToolsController::replaceContentsOfRangeInSession(ProofreadingState& state, const SimpleRange& range, const String& replacementText)
{
    RefPtr document = this->document();

    auto sessionRange = makeSimpleRange(state.contextRange);

    auto sessionRangeCount = characterCount(sessionRange);
    auto resolvedCharacterRange = characterRange(sessionRange, range);

    document->selection().setSelection({ range });

    {
        EditingScope editingScope { *document };
        document->editor().replaceSelectionWithText(replacementText, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, EditAction::InsertReplacement);
    }

    auto selection = document->selection().selection();

    auto newSessionRange = rangeExpandedAroundRangeByCharacters(selection, resolvedCharacterRange.location, sessionRangeCount - (resolvedCharacterRange.location + resolvedCharacterRange.length));
    if (!newSessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    state.contextRange = createLiveRange(*newSessionRange);
}

void WritingToolsController::replaceContentsOfRangeInSession(CompositionState& state, const SimpleRange& range, const AttributedString& replacementText, WritingToolsCompositionCommand::State commandState)
{
    RefPtr fragment = createFragment(*document()->frame(), replacementText.nsAttributedString().get(), { FragmentCreationOptions::NoInterchangeNewlines, FragmentCreationOptions::SanitizeMarkup });
    if (!fragment) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto hasAttributes = std::ranges::any_of(replacementText.attributes, [](auto& rangeAndAttributeValues) {
        return !rangeAndAttributeValues.second.isEmpty();
    });
    auto matchStyle = hasAttributes ? WritingToolsCompositionCommand::MatchStyle::No : WritingToolsCompositionCommand::MatchStyle::Yes;

    EditingScope editingScope { *document() };
    state.reappliedCommands.last()->replaceContentsOfRangeWithFragment(WTFMove(fragment), range, matchStyle, commandState);
}

} // namespace WebKit

#endif
