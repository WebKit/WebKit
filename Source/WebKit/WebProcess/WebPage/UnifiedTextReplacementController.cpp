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

#include "Logging.h"
#include "WebPage.h"
#include "WebUnifiedTextReplacementContextData.h"
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/Editor.h>
#include <WebCore/HTMLConverter.h>
#include <WebCore/RenderedDocumentMarker.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIterator.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_WK_TZONE_ALLOCATED_IMPL(UnifiedTextReplacementController);

static void replaceTextInRange(LocalFrame& frame, const SimpleRange& range, const String& replacementText)
{
    RefPtr document = frame.document();
    if (!document)
        return;

    WebCore::VisibleSelection visibleSelection(range);

    constexpr OptionSet temporarySelectionOptions { TemporarySelectionOption::DoNotSetFocus, TemporarySelectionOption::IgnoreSelectionChanges };
    WebCore::TemporarySelectionChange selectionChange(*document, visibleSelection, temporarySelectionOptions);

    frame.editor().replaceSelectionWithText(replacementText, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, EditAction::InsertReplacement);
}

static void replaceContentsInRange(WebCore::LocalFrame& frame, const SimpleRange& range, WebCore::DocumentFragment& fragment)
{
    RefPtr document = frame.document();
    if (!document)
        return;

    WebCore::VisibleSelection visibleSelection(range);

    constexpr OptionSet temporarySelectionOptions { WebCore::TemporarySelectionOption::DoNotSetFocus, WebCore::TemporarySelectionOption::IgnoreSelectionChanges };
    WebCore::TemporarySelectionChange selectionChange(*document, visibleSelection, temporarySelectionOptions);

    frame.editor().replaceSelectionWithFragment(fragment, Editor::SelectReplacement::Yes, Editor::SmartReplace::No, Editor::MatchStyle::Yes, EditAction::Insert);
}

static std::optional<std::tuple<Node&, DocumentMarker&>> findReplacementMarkerByUUID(WebCore::Document& document, const WTF::UUID& replacementUUID)
{
    RefPtr<Node> targetNode;
    WeakPtr<DocumentMarker> targetMarker;

    document.markers().forEachOfTypes({ DocumentMarker::Type::UnifiedTextReplacement }, [&replacementUUID, &targetNode, &targetMarker] (auto& node, auto& marker) mutable {
        auto data = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());
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

UnifiedTextReplacementController::UnifiedTextReplacementController(WebPage& webPage)
    : m_webPage(webPage)
{
}

void UnifiedTextReplacementController::willBeginTextReplacementSession(const WTF::UUID& uuid, CompletionHandler<void(const Vector<WebUnifiedTextReplacementContextData>&)>&& completionHandler)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s)", uuid.toString().utf8().data());

    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        completionHandler({ });
        return;
    }

    auto contextRange = m_webPage->autocorrectionContextRange();
    if (!contextRange) {
        RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::willBeginTextReplacementSession (%s) => no context range", uuid.toString().utf8().data());
        completionHandler({ });
        return;
    }

    auto liveRange = createLiveRange(*contextRange);

    ASSERT(!m_contextRanges.contains(uuid));
    m_contextRanges.set(uuid, liveRange);

    Ref frame = CheckedRef(corePage->focusController())->focusedOrMainFrame();

    auto selectedTextRange = frame->selection().selection().firstRange();

    auto attributedStringFromRange = attributedString(*contextRange);
    auto selectedTextCharacterRange = WebCore::characterRange(*contextRange, *selectedTextRange);

    completionHandler({ { WTF::UUID { 0 }, attributedStringFromRange, selectedTextCharacterRange } });
}

void UnifiedTextReplacementController::didBeginTextReplacementSession(const WTF::UUID& uuid, const Vector<WebKit::WebUnifiedTextReplacementContextData>& contexts)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didBeginTextReplacementSession (%s) [received contexts: %zu]", uuid.toString().utf8().data(), contexts.size());
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements(const WTF::UUID& uuid, const Vector<WebTextReplacementData>& replacements, const WebUnifiedTextReplacementContextData& context, bool finished)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveReplacements (%s) [received replacements: %zu, finished: %d]", uuid.toString().utf8().data(), replacements.size(), finished);

    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref frame = CheckedRef(corePage->focusController())->focusedOrMainFrame();

    ASSERT(m_contextRanges.contains(uuid));

    auto liveRange = m_contextRanges.get(uuid);
    auto sessionRange = makeSimpleRange(liveRange);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    frame->selection().clear();

    size_t additionalOffset = 0;

    for (const auto& replacementData : replacements) {
        auto locationWithOffset = replacementData.originalRange.location + additionalOffset;

        auto originalRangeWithOffset = CharacterRange { locationWithOffset, replacementData.originalRange.length };
        auto resolvedRange = resolveCharacterRange(*sessionRange, originalRangeWithOffset);

        replaceTextInRange(frame.get(), resolvedRange, replacementData.replacement);

        auto newRangeWithOffset = CharacterRange { locationWithOffset, replacementData.replacement.length() };
        auto newResolvedRange = resolveCharacterRange(*sessionRange, newRangeWithOffset);

        auto markerData = DocumentMarker::UnifiedTextReplacementData { replacementData.originalString.string, replacementData.uuid, DocumentMarker::UnifiedTextReplacementData::State::Pending };
        addMarker(resolvedRange, DocumentMarker::Type::UnifiedTextReplacement, markerData);

        additionalOffset += replacementData.replacement.length() - replacementData.originalRange.length;
    }
}

void UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement(const WTF::UUID& uuid, WebTextReplacementData::State state, const WebTextReplacementData& replacement, const WebUnifiedTextReplacementContextData& context)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidUpdateStateForReplacement (%s) [new state: %hhu, replacement: %s]", uuid.toString().utf8().data(), enumToUnderlyingType(state), replacement.uuid.toString().utf8().data());

    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref frame = CheckedRef(corePage->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (state != WebTextReplacementData::State::Committed && state != WebTextReplacementData::State::Reverted)
        return;

    auto nodeAndMarker = findReplacementMarkerByUUID(*document, replacement.uuid);
    if (!nodeAndMarker) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto& [node, marker] = *nodeAndMarker;

    auto rangeToReplace = makeSimpleRange(node, marker);

    auto oldData = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());

    auto offsetRange = OffsetRange { marker.startOffset(), marker.endOffset() };
    document->markers().removeMarkers(node, offsetRange, { DocumentMarker::Type::UnifiedTextReplacement });

    auto [newText, newState] = [&]() -> std::tuple<String, DocumentMarker::UnifiedTextReplacementData::State> {
        switch (state) {
        case WebTextReplacementData::State::Committed:
            return std::make_tuple(replacement.replacement, DocumentMarker::UnifiedTextReplacementData::State::Committed);

        case WebTextReplacementData::State::Reverted:
            return std::make_tuple(oldData.originalText, DocumentMarker::UnifiedTextReplacementData::State::Reverted);

        default:
            RELEASE_ASSERT_NOT_REACHED();
        }
    }();

    replaceTextInRange(frame.get(), rangeToReplace, newText);

    auto newData = DocumentMarker::UnifiedTextReplacementData { oldData.originalText, oldData.uuid, newState };
    document->markers().addMarker(node, DocumentMarker { DocumentMarker::Type::UnifiedTextReplacement, offsetRange, WTFMove(newData) });
}

void UnifiedTextReplacementController::didEndTextReplacementSession(const WTF::UUID& uuid, bool accepted)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::didEndTextReplacementSession (%s) [accepted: %d]", uuid.toString().utf8().data(), accepted);

    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref frame = CheckedRef(corePage->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    // FIXME: Associate the markers with a specific session, and only modify the markers which belong
    // to this session.

    if (!accepted) {
        document->markers().forEachOfTypes({ DocumentMarker::Type::UnifiedTextReplacement }, [&frame] (auto& node, auto& marker) mutable {
            auto data = std::get<DocumentMarker::UnifiedTextReplacementData>(marker.data());
            if (data.state == DocumentMarker::UnifiedTextReplacementData::State::Reverted)
                return false;

            auto rangeToReplace = makeSimpleRange(node, marker);
            replaceTextInRange(frame.get(), rangeToReplace, data.originalText);

            return false;
        });
    }

    document->markers().removeMarkers({ DocumentMarker::Type::UnifiedTextReplacement });

    m_contextRanges.remove(uuid);
    m_originalDocumentNodes.remove(uuid);
    m_replacements.remove(uuid);
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange(const WTF::UUID& uuid, const WebCore::AttributedString& attributedText, const WebCore::CharacterRange& range, const WebUnifiedTextReplacementContextData& context)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) [range: %llu, %llu]", uuid.toString().utf8().data(), range.location, range.length);

    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref frame = CheckedRef(corePage->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto liveRange = m_contextRanges.get(uuid);
    auto sessionRange = makeSimpleRange(liveRange);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    frame->selection().clear();

    auto resolvedRange = resolveCharacterRange(*sessionRange, range);

    if (!m_originalDocumentNodes.contains(uuid)) {
        auto contents = liveRange->cloneContents();
        if (contents.hasException()) {
            RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveTextWithReplacementRange (%s) => exception when cloning contents", uuid.toString().utf8().data());
            return;
        }
        m_originalDocumentNodes.set(uuid, contents.returnValue()); // Deep clone.
    }

    auto& replacements = m_replacements.ensure(uuid, [] {
        return Vector<Replacement> { };
    }).iterator->value;

    auto [newText, newRange] = [&] {
        if (replacements.isEmpty())
            return std::make_tuple(attributedText.string, range);

        auto lastReplacement = replacements.last();
        auto lastReplacementSize = lastReplacement.attributedText.string.length();

        auto newTextResult = attributedText.string.substring(lastReplacementSize);
        auto newRangeResult = CharacterRange { range.location + lastReplacementSize, range.length - lastReplacementSize };

        return std::make_tuple(newTextResult, newRangeResult);
    }();

    auto newResolvedRange = resolveCharacterRange(*sessionRange, newRange);

    replaceTextInRange(frame.get(), newResolvedRange, newText);

    auto updatedLiveRange = createLiveRange(*sessionRange);
    m_contextRanges.set(uuid, updatedLiveRange);

    replacements.append({ attributedText, range });
}

void UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction(const WTF::UUID& uuid, WebKit::WebTextReplacementData::EditAction action)
{
    RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) [action: %hhu]", uuid.toString().utf8().data(), enumToUnderlyingType(action));

    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    Ref frame = CheckedRef(corePage->focusController())->focusedOrMainFrame();
    RefPtr document = frame->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto liveRange = m_contextRanges.get(uuid);
    auto sessionRange = makeSimpleRange(liveRange);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    switch (action) {
    case WebKit::WebTextReplacementData::EditAction::Undo:
    case WebKit::WebTextReplacementData::EditAction::Redo: {
        if (m_replacements.isEmpty())
            return;

        auto firstReplacement = m_replacements.get(uuid).first();
        auto lastReplacement = m_replacements.get(uuid).last();

        auto originalFragment = m_originalDocumentNodes.take(uuid);

        auto contents = liveRange->cloneContents();
        if (contents.hasException()) {
            RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) => exception when cloning contents", uuid.toString().utf8().data());
            return;
        }

        m_originalDocumentNodes.set(uuid, contents.returnValue()); // Deep clone.

        replaceContentsInRange(frame, *sessionRange, *originalFragment);

        auto updatedLiveRange = createLiveRange(*sessionRange);
        m_contextRanges.set(uuid, updatedLiveRange);

        break;
    }

    case WebKit::WebTextReplacementData::EditAction::UndoAll:
        // FIXME: Implement this action.
        RELEASE_LOG(UnifiedTextReplacement, "UnifiedTextReplacementController::textReplacementSessionDidReceiveEditAction (%s) [action: %hhu] => not yet implemented", uuid.toString().utf8().data(), enumToUnderlyingType(action));
        RELEASE_ASSERT_NOT_REACHED();
        break;
    }
}

} // namespace WebKit

#endif
