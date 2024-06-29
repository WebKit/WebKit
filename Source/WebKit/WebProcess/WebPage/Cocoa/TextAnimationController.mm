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

#if ENABLE(WRITING_TOOLS_UI)

#include "config.h"
#include "TextAnimationController.h"

#include "TextAnimationType.h"
#include "WebPage.h"
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/FocusController.h>
#include <WebCore/Range.h>
#include <WebCore/RenderedDocumentMarker.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TextIterator.h>
#include <WebCore/WritingToolsTypes.h>

namespace WebKit {

// This should mirror what is in WritingToolsController, and eventually not be copied.
static constexpr auto defaultTextAnimationControllerTextIteratorBehaviors = WebCore::TextIteratorBehaviors {
    WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForImages,
#if ENABLE(ATTACHMENT_ELEMENT)
    WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForAttachments
#endif
};

TextAnimationController::TextAnimationController(WebPage& webPage)
    : m_webPage(webPage)
{
}

RefPtr<WebCore::Document> TextAnimationController::document() const
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

// FIXME: This is a layering violation.
std::optional<WebCore::SimpleRange> TextAnimationController::contextRangeForSessionWithID(const WebCore::WritingTools::Session::ID& sessionID) const
{
    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    return corePage->contextRangeForSessionWithID(sessionID);
}

std::optional<WebCore::SimpleRange> TextAnimationController::contextRangeForTextAnimationID(const WTF::UUID& animationUUID) const
{
    if (auto iterator = m_manuallyEnabledAnimationRanges.find(animationUUID); iterator != m_manuallyEnabledAnimationRanges.end())
        return WebCore::makeSimpleRange(iterator->value.get());

    for (auto sessionUUID : m_initialAnimations.keys()) {
        if (m_initialAnimations.get(sessionUUID) == animationUUID)
            return contextRangeForSessionWithID(sessionUUID);
    }

    for (auto sessionUUID : m_activeTextAnimations.keys()) {
        for (auto animationState : m_activeTextAnimations.get(sessionUUID)) {
            if (animationState.styleID == animationUUID) {
                if (auto fullSessionRange = contextRangeForSessionWithID(sessionUUID))
                    return WebCore::resolveCharacterRange(*fullSessionRange, animationState.range, defaultTextAnimationControllerTextIteratorBehaviors);
            }
        }
    }
    return std::nullopt;
}

void TextAnimationController::removeTransparentMarkersForSessionID(const WTF::UUID& sessionUUID)
{
    if (auto iterator = m_initialAnimations.find(sessionUUID); iterator != m_initialAnimations.end())
        removeTransparentMarkersForTextAnimationID(iterator->value);

    auto animationStates = m_activeTextAnimations.take(sessionUUID);
    if (animationStates.isEmpty())
        return;
    for (auto animationState : animationStates)
        removeTransparentMarkersForTextAnimationID(animationState.styleID);

    if (auto rangeData = m_unstyledRanges.get(sessionUUID))
        removeTransparentMarkersForTextAnimationID(rangeData->styleID);
}

void TextAnimationController::removeTransparentMarkersForTextAnimationID(const WTF::UUID& uuid)
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

void TextAnimationController::removeInitialTextAnimation(const WTF::UUID& sessionUUID)
{
    if (auto animationID = m_initialAnimations.take(sessionUUID))
        m_webPage->removeTextAnimationForAnimationID(animationID);
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

void TextAnimationController::addInitialTextAnimation(const WTF::UUID& sessionUUID)
{
    auto initialAnimationUUID = WTF::UUID::createVersion4();
    auto sessionRange = contextRangeForSessionWithID(sessionUUID);

    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto textIndicatorData = createTextIndicatorForRange(*sessionRange);
    m_webPage->addTextAnimationForAnimationID(initialAnimationUUID, { TextAnimationType::Initial, WTF::UUID(WTF::UUID::emptyValue) }, *textIndicatorData);

    m_initialAnimations.add(sessionUUID, initialAnimationUUID);
}


void TextAnimationController::addSourceTextAnimation(const WTF::UUID& sessionUUID, const WebCore::CharacterRange& currentReplacedRange)
{
#if PLATFORM(MAC)
    auto currentlyStyledRange = m_alreadyReplacedRanges.getOptional(sessionUUID);
    if (!currentlyStyledRange)
        return;

    auto replaceCharacterRange = newlyReplacedCharacterRange(currentReplacedRange, *currentlyStyledRange);
    auto sessionRange = contextRangeForSessionWithID(sessionUUID);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto replacedRange = WebCore::resolveCharacterRange(*sessionRange, *currentlyStyledRange, defaultTextAnimationControllerTextIteratorBehaviors);

    auto sourceTextIndicatorUUID = WTF::UUID::createVersion4();
    auto textIndicatorData = createTextIndicatorForRange(replacedRange);
    if (!textIndicatorData)
        return;

    m_webPage->addTextAnimationForAnimationID(sourceTextIndicatorUUID, { WebKit::TextAnimationType::Source, WTF::UUID(WTF::UUID::emptyValue)  }, *textIndicatorData);

    TextAnimationState animationState = { sourceTextIndicatorUUID, replaceCharacterRange };
    auto& animationStates = m_activeTextAnimations.ensure(sessionUUID, [&] {
        return Vector<TextAnimationState> { };
    }).iterator->value;

    animationStates.append(animationState);

    m_alreadyReplacedRanges.set(sessionUUID, currentReplacedRange);
#endif
}

void TextAnimationController::addDestinationTextAnimation(const WTF::UUID& sessionUUID, const WebCore::CharacterRange& characterRangeAfterReplace)
{
    auto finalTextIndicatorUUID = WTF::UUID::createVersion4();
    auto sessionRange = contextRangeForSessionWithID(sessionUUID);
    if (!sessionRange) {
        ASSERT_NOT_REACHED();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    auto replacedRangeAfterReplace = WebCore::resolveCharacterRange(*sessionRange, characterRangeAfterReplace, defaultTextAnimationControllerTextIteratorBehaviors);

    auto unstyledRange = WebCore::makeRangeSelectingNodeContents(*document);
    unstyledRange.start.container = replacedRangeAfterReplace.endContainer();
    unstyledRange.start.offset = replacedRangeAfterReplace.endOffset();

    auto unstyledRangeUUID = WTF::UUID::createVersion4();
    TextAnimationUnstyledRangeData unstyledRangeData = { unstyledRangeUUID, unstyledRange };
    m_unstyledRanges.add(sessionUUID, unstyledRangeData);

    auto textIndicatorData = createTextIndicatorForRange(replacedRangeAfterReplace);
    if (!textIndicatorData)
        return;

    m_webPage->addTextAnimationForAnimationID(finalTextIndicatorUUID, { TextAnimationType::Final, unstyledRangeUUID }, *textIndicatorData);
    TextAnimationState animationState = { finalTextIndicatorUUID, characterRangeAfterReplace };
    auto& animationStates = m_activeTextAnimations.ensure(sessionUUID, [&] {
        return Vector<TextAnimationState> { };
    }).iterator->value;

    animationStates.append(animationState);
}

void TextAnimationController::updateUnderlyingTextVisibilityForTextAnimationID(const WTF::UUID& uuid, bool visible, CompletionHandler<void()>&& completionHandler)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        completionHandler();
        return;
    }

    if (visible)
        removeTransparentMarkersForTextAnimationID(uuid);
    else {
        auto sessionRange = contextRangeForTextAnimationID(uuid);

        if (!sessionRange) {
            completionHandler();
            return;
        }
        document->markers().addTransparentContentMarker(*sessionRange, uuid);
    }
    completionHandler();
}

std::optional<WebCore::TextIndicatorData> TextAnimationController::createTextIndicatorForRange(const WebCore::SimpleRange& range)
{
    if (!m_webPage) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

    RefPtr corePage = m_webPage->corePage();
    if (!corePage) {
        ASSERT_NOT_REACHED();
        return std::nullopt;
    }

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

    return textIndicatorData;
}

// FIXME: This shouldn't be called anymore, make sure that that is true, and remove.
void TextAnimationController::createTextIndicatorForTextAnimationID(const WTF::UUID& uuid, CompletionHandler<void(std::optional<WebCore::TextIndicatorData>&&)>&& completionHandler)
{
    auto sessionRange = contextRangeForTextAnimationID(uuid);

    if (!sessionRange) {
        completionHandler(std::nullopt);
        return;
    }
    completionHandler(createTextIndicatorForRange(*sessionRange));
}

void TextAnimationController::enableSourceTextAnimationAfterElementWithID(const String& elementID, const WTF::UUID& uuid)
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

    m_manuallyEnabledAnimationRanges.add(uuid, createLiveRange(*simpleRange));
}

void TextAnimationController::enableTextAnimationTypeForElementWithID(const String& elementID, const WTF::UUID& uuid)
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

    m_manuallyEnabledAnimationRanges.add(uuid, createLiveRange(elementRange));
}

} // namespace WebKit

#endif // ENABLE(WRITING_TOOLS)
