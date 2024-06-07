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
#include "TextIndicatorStyleController.h"

#include "TextIndicatorStyle.h"
#include "UnifiedTextReplacementController.h"
#include "WebPage.h"
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/FocusController.h>
#include <WebCore/RenderedDocumentMarker.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TextIterator.h>

namespace WebKit {

// This should mirror what is in UnifiedTextIndicatorController, and eventually not be copied.
static constexpr auto defaultTextIndicatorStyleControllerTextIteratorBehaviors = WebCore::TextIteratorBehaviors {
    WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForImages,
#if ENABLE(ATTACHMENT_ELEMENT)
    WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForAttachments
#endif
};

TextIndicatorStyleController::TextIndicatorStyleController(WebPage& webPage)
    : m_webPage(webPage)
{
}

RefPtr<WebCore::Document> TextIndicatorStyleController::document() const
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

std::optional<WebCore::SimpleRange> TextIndicatorStyleController::contextRangeForTextIndicatorStyle(const WTF::UUID& uuid) const
{
    if (auto sessionRange = m_webPage->unifiedTextReplacementController().contextRangeForSessionWithUUID(uuid))
        return sessionRange;

    if (m_textIndicatorStyleEnablementRanges.contains(uuid))
        return makeSimpleRange(m_textIndicatorStyleEnablementRanges.find(uuid)->value.get());

    for (auto sessionUUID : m_activeTextIndicatorStyles.keys()) {
        for (auto styleState : m_activeTextIndicatorStyles.get(sessionUUID)) {
            if (styleState.styleID == uuid) {
                if (auto fullSessionRange = m_webPage->unifiedTextReplacementController().contextRangeForSessionWithUUID(sessionUUID))
                    return WebCore::resolveCharacterRange(*fullSessionRange, styleState.range, defaultTextIndicatorStyleControllerTextIteratorBehaviors);
            }
        }
    }
    return std::nullopt;
}

void TextIndicatorStyleController::cleanUpTextStylesForSessionID(const WTF::UUID& sessionUUID)
{
    auto styleStates = m_activeTextIndicatorStyles.take(sessionUUID);
    if (styleStates.isEmpty())
        return;
    for (auto styleState : styleStates)
        removeTransparentMarkersForUUID(styleState.styleID);

    auto unstyledRange = m_unstyledRanges.find(sessionUUID);
    if (unstyledRange->value)
        removeTransparentMarkersForUUID(unstyledRange->value->styleID);
}

void TextIndicatorStyleController::removeTransparentMarkersForUUID(const WTF::UUID& uuid)
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

#if PLATFORM(MAC)
static WebCore::CharacterRange newlyReplacedCharacterRange(WebCore::CharacterRange superRange, WebCore::CharacterRange previousRange)
{
    auto location = previousRange.location + previousRange.length;
    auto length = superRange.length - previousRange.length;
    if (superRange.length < previousRange.length) {
        ASSERT_NOT_REACHED();
        return WebCore::CharacterRange { 0, 0 };
    }
    return WebCore::CharacterRange { location, length };
};
#endif

void TextIndicatorStyleController::addSourceTextIndicatorStyle(const WTF::UUID& sessionUUID, const WebCore::CharacterRange& currentReplacedRange, const WebCore::SimpleRange& resolvedRange)
{
    auto replacedRange = resolvedRange;
    auto currentlyStyledRange = m_currentlyStyledRange.getOptional(sessionUUID);
    if (!currentlyStyledRange)
        return;
#if PLATFORM(MAC)
    auto replaceCharacterRange = newlyReplacedCharacterRange(currentReplacedRange, *currentlyStyledRange);
    auto sessionRange = m_webPage->unifiedTextReplacementController().contextRangeForSessionWithUUID(sessionUUID);
    replacedRange = WebCore::resolveCharacterRange(*sessionRange, *currentlyStyledRange, defaultTextIndicatorStyleControllerTextIteratorBehaviors);

    auto sourceTextIndicatorUUID = WTF::UUID::createVersion4();
    createTextIndicatorForRange(replacedRange, [sourceTextIndicatorUUID, weakWebPage = WeakPtr { m_webPage }](std::optional<WebCore::TextIndicatorData>&& textIndicatorData) {
        if (!weakWebPage)
            return;
        RefPtr protectedWebPage = weakWebPage.get();
        if (textIndicatorData)
            protectedWebPage->addTextIndicatorStyleForID(sourceTextIndicatorUUID, { WebKit::TextIndicatorStyle::Source, WTF::UUID(WTF::UUID::emptyValue)  }, *textIndicatorData);
    });
    TextIndicatorStyleState styleState = { sourceTextIndicatorUUID, replaceCharacterRange };
    auto& styleStates = m_activeTextIndicatorStyles.ensure(sessionUUID, [&] {
        return Vector<TextIndicatorStyleState> { };
    }).iterator->value;

    styleStates.append(styleState);

    m_currentlyStyledRange.set(sessionUUID, currentReplacedRange);
#endif
}
void TextIndicatorStyleController::addDestinationTextIndicatorStyle(const WTF::UUID& sessionUUID, const WebCore::CharacterRange& characterRangeAfterReplace, const WebCore::SimpleRange& resolvedRange)
{
    auto finalTextIndicatorUUID = WTF::UUID::createVersion4();
    auto sessionRange = m_webPage->unifiedTextReplacementController().contextRangeForSessionWithUUID(sessionUUID);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto replacedRangeAfterReplace = WebCore::resolveCharacterRange(*sessionRange, characterRangeAfterReplace, defaultTextIndicatorStyleControllerTextIteratorBehaviors);

    auto unstyledRange = WebCore::makeRangeSelectingNodeContents(*document);
    unstyledRange.start.container = replacedRangeAfterReplace.endContainer();
    unstyledRange.start.offset = replacedRangeAfterReplace.endOffset();

    auto unstyledRangeUUID = WTF::UUID::createVersion4();
    TextIndicatorStyleUnstyledRangeData unstyledRangeData = { unstyledRangeUUID, unstyledRange };
    m_unstyledRanges.add(sessionUUID, unstyledRangeData);

    createTextIndicatorForRange(replacedRangeAfterReplace, [finalTextIndicatorUUID, unstyledRangeUUID, weakWebPage = WeakPtr { m_webPage }](std::optional<WebCore::TextIndicatorData>&& textIndicatorData) {
        if (!weakWebPage)
            return;
        RefPtr protectedWebPage = weakWebPage.get();
        if (textIndicatorData)
            protectedWebPage->addTextIndicatorStyleForID(finalTextIndicatorUUID, { TextIndicatorStyle::Final, unstyledRangeUUID }, *textIndicatorData);
    });
    TextIndicatorStyleState styleState = { finalTextIndicatorUUID, characterRangeAfterReplace };
    auto& styleStates = m_activeTextIndicatorStyles.ensure(sessionUUID, [&] {
        return Vector<TextIndicatorStyleState> { };
    }).iterator->value;

    styleStates.append(styleState);
}

void TextIndicatorStyleController::updateTextIndicatorStyleVisibilityForID(const WTF::UUID& uuid, bool visible, CompletionHandler<void()>&& completionHandler)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler();
        return;
    }

    if (visible)
        removeTransparentMarkersForUUID(uuid);
    else {
        auto sessionRange = contextRangeForTextIndicatorStyle(uuid);

        if (!sessionRange) {
            completionHandler();
            return;
        }
        document->markers().addTransparentContentMarker(*sessionRange, uuid);
    }
    completionHandler();
}


void TextIndicatorStyleController::createTextIndicatorForRange(const WebCore::SimpleRange& range, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&& completionHandler)
{
    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (RefPtr localMainFrame = dynamicDowncast<WebCore::LocalFrame>(corePage->mainFrame())) {
        std::optional<WebCore::TextIndicatorData> textIndicatorData;
        constexpr OptionSet textIndicatorOptions {
            WebCore::TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
            WebCore::TextIndicatorOption::ExpandClipBeyondVisibleRect,
            WebCore::TextIndicatorOption::UseSelectionRectForSizing,
            WebCore::TextIndicatorOption::SkipReplacedContent,
            WebCore::TextIndicatorOption::RespectTextColor
        };
        if (auto textIndicator = WebCore::TextIndicator::createWithRange(range, textIndicatorOptions, WebCore::TextIndicatorPresentationTransition::None, { }))
            textIndicatorData = textIndicator->data();
        completionHandler(WTFMove(textIndicatorData));
        return;
    }
    completionHandler(std::nullopt);
}

void TextIndicatorStyleController::createTextIndicatorForID(const WTF::UUID& uuid, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&& completionHandler)
{
    auto sessionRange = contextRangeForTextIndicatorStyle(uuid);

    if (!sessionRange) {
        completionHandler(std::nullopt);
        return;
    }
    createTextIndicatorForRange(*sessionRange, WTFMove(completionHandler));
}

void TextIndicatorStyleController::enableTextIndicatorStyleAfterElementWithID(const String& elementID, const WTF::UUID& uuid)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr root = document->documentElement();
    if (!root) {
        ASSERT_NOT_REACHED();
        return;
    }

    WebCore::VisibleSelection fullDocumentSelection(WebCore::VisibleSelection::selectionFromContentsOfNode(root.get()));
    auto simpleRange = fullDocumentSelection.range();
    if (!simpleRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    if (RefPtr element = document->getElementById(elementID)) {
        auto elementRange = WebCore::makeRangeSelectingNodeContents(*element);
        if (!elementRange.collapsed())
            simpleRange->start = elementRange.end;
    }

    m_textIndicatorStyleEnablementRanges.add(uuid, createLiveRange(*simpleRange));
}

void TextIndicatorStyleController::enableTextIndicatorStyleForElementWithID(const String& elementID, const WTF::UUID& uuid)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr element = document->getElementById(elementID);
    if (!element) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto elementRange = WebCore::makeRangeSelectingNodeContents(*element);
    if (elementRange.collapsed()) {
        ASSERT_NOT_REACHED();
        return;
    }

    m_textIndicatorStyleEnablementRanges.add(uuid, createLiveRange(elementRange));
}

} // namespace WebKit

#endif // ENABLE(UNIFIED_TEXT_REPLACEMENT)
