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

#include "config.h"
#include "TextAnimationController.h"

#include "WebPage.h"
#include <WebCore/Chrome.h>
#include <WebCore/ChromeClient.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/Editing.h>
#include <WebCore/FocusController.h>
#include <WebCore/Range.h>
#include <WebCore/RenderedDocumentMarker.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextAnimationTypes.h>
#include <WebCore/TextIndicator.h>
#include <WebCore/TextIterator.h>
#include <WebCore/WritingToolsTypes.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

// This should mirror what is in WritingToolsController, and eventually not be copied.
static constexpr auto defaultTextAnimationControllerTextIteratorBehaviors = WebCore::TextIteratorBehaviors {
    WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForImages,
#if ENABLE(ATTACHMENT_ELEMENT)
    WebCore::TextIteratorBehavior::EmitsObjectReplacementCharactersForAttachments
#endif
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(TextAnimationController);

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

std::optional<WebCore::SimpleRange> TextAnimationController::unreplacedRangeForActiveWritingToolsSession() const
{
    auto sessionRange = contextRangeForActiveWritingToolsSession();
    if (!sessionRange)
        return std::nullopt;

    auto previouslyReplacedRange = m_alreadyReplacedRange;
    if (!previouslyReplacedRange)
        return sessionRange;

    auto replacedRange = WebCore::resolveCharacterRange(*sessionRange, previouslyReplacedRange->range, defaultTextAnimationControllerTextIteratorBehaviors);

    auto remainingRange = *sessionRange;
    remainingRange.start = replacedRange.end;

    return remainingRange;
}

// FIXME: This is a layering violation.
std::optional<WebCore::SimpleRange> TextAnimationController::contextRangeForActiveWritingToolsSession() const
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

    return corePage->contextRangeForActiveWritingToolsSession();
}

std::optional<WebCore::SimpleRange> TextAnimationController::contextRangeForTextAnimationID(const WTF::UUID& animationUUID) const
{
    if (RefPtr manuallyEnabledAnimationRange = m_manuallyEnabledAnimationRange; manuallyEnabledAnimationRange)
        return WebCore::makeSimpleRange(*manuallyEnabledAnimationRange);

    if (m_initialAnimationID == animationUUID)
        return unreplacedRangeForActiveWritingToolsSession();

    for (auto animationRange : m_textAnimationRanges) {
        if (animationRange.animationUUID == animationUUID) {
            if (auto fullSessionRange = contextRangeForActiveWritingToolsSession())
                return WebCore::resolveCharacterRange(*fullSessionRange, animationRange.range, defaultTextAnimationControllerTextIteratorBehaviors);
        }
    }

    if (m_unanimatedRangeData && m_unanimatedRangeData->animationUUID == animationUUID)
        return m_unanimatedRangeData->range;

    return std::nullopt;
}

void TextAnimationController::removeTransparentMarkersForActiveWritingToolsSession()
{
    if (auto initialAnimationID = m_initialAnimationID)
        removeTransparentMarkersForTextAnimationID(*initialAnimationID);

    auto textAnimationRanges = std::exchange(m_textAnimationRanges, { });

    if (textAnimationRanges.isEmpty())
        return;

    for (auto textAnimationRange : textAnimationRanges)
        removeTransparentMarkersForTextAnimationID(textAnimationRange.animationUUID);

    if (auto rangeData = m_unanimatedRangeData)
        removeTransparentMarkersForTextAnimationID(rangeData->animationUUID);
}

void TextAnimationController::removeTransparentMarkersForTextAnimationID(const WTF::UUID& uuid)
{
    RefPtr document = this->document();
    if (!document) {
        ASSERT_NOT_REACHED();
        return;
    }

    document->markers().removeMarkers({ WebCore::DocumentMarkerType::TransparentContent }, [&](const WebCore::DocumentMarker& marker) {
        return std::get<WebCore::DocumentMarker::TransparentContentData>(marker.data()).uuid == uuid ? WebCore::FilterMarkerResult::Remove : WebCore::FilterMarkerResult::Keep;
    });
}

void TextAnimationController::removeInitialTextAnimationForActiveWritingToolsSession()
{
    if (auto initialAnimationID = std::exchange(m_initialAnimationID, std::nullopt))
        m_webPage->removeTextAnimationForAnimationID(*initialAnimationID);
}

static WebCore::CharacterRange remainingCharacterRange(WebCore::CharacterRange totalRange, WebCore::CharacterRange previousRange)
{
    if (totalRange.length < previousRange.length)
        return totalRange;

    auto location = previousRange.location + previousRange.length;
    auto length = totalRange.length - previousRange.length;

    return WebCore::CharacterRange { location, length };
};

void TextAnimationController::addInitialTextAnimationForActiveWritingToolsSession()
{
    auto initialAnimationID = WTF::UUID::createVersion4();
    auto animatingRange = unreplacedRangeForActiveWritingToolsSession();

    if (!animatingRange || animatingRange->collapsed())
        return;

    auto textIndicatorData = createTextIndicatorForRange(*animatingRange);
    if (!textIndicatorData)
        return;

    removeInitialTextAnimationForActiveWritingToolsSession();

    m_webPage->addTextAnimationForAnimationID(initialAnimationID, { WebCore::TextAnimationType::Initial, WebCore::TextAnimationRunMode::RunAnimation }, *textIndicatorData);

    m_initialAnimationID = initialAnimationID;
}

void TextAnimationController::addSourceTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, bool finished, const WebCore::CharacterRange& replacingRange, const String& string, CompletionHandler<void(WebCore::TextAnimationRunMode)>&& completionHandler)
{
    auto previouslyReplacedRange = m_alreadyReplacedRange;
    auto replaceCharacterRange = replacingRange;

    WebCore::TextAnimationRunMode runMode = WebCore::TextAnimationRunMode::RunAnimation;

    if (finished)
        runMode = WebCore::TextAnimationRunMode::OnlyReplaceText;
    else if (previouslyReplacedRange)
        replaceCharacterRange = remainingCharacterRange(replacingRange, previouslyReplacedRange->range);

    auto sessionRange = contextRangeForActiveWritingToolsSession();
    if (!sessionRange) {
        completionHandler(WebCore::TextAnimationRunMode::RunAnimation);
        return;
    }

    auto replacedRange = WebCore::resolveCharacterRange(*sessionRange, replaceCharacterRange, defaultTextAnimationControllerTextIteratorBehaviors);

    auto textIndicatorData = createTextIndicatorForRange(replacedRange);
    if (!textIndicatorData) {
        completionHandler(WebCore::TextAnimationRunMode::RunAnimation);
        return;
    }

    if (runMode == WebCore::TextAnimationRunMode::OnlyReplaceText) {
        if (m_activeAnimation) {
            if (auto finalReplaceHander = std::exchange(m_finalReplaceHandler, std::nullopt))
                (*finalReplaceHander)(WebCore::TextAnimationRunMode::OnlyReplaceText);
            m_finalReplaceHandler = WTFMove(completionHandler);
        } else
            completionHandler(WebCore::TextAnimationRunMode::OnlyReplaceText);
        return;
    }

    // On iOS this will search for the completion handler to pass the text indicator to so that the animation can continue.
    m_webPage->addTextAnimationForAnimationID(sourceAnimationUUID, { WebCore::TextAnimationType::Source, runMode, WTF::UUID(WTF::UUID::emptyValue), sourceAnimationUUID, destinationAnimationUUID }, *textIndicatorData, WTFMove(completionHandler));
    m_activeAnimation = sourceAnimationUUID;
    m_textAnimationRanges.append({ sourceAnimationUUID, replaceCharacterRange });
}

void TextAnimationController::addDestinationTextAnimationForActiveWritingToolsSession(const WTF::UUID& sourceAnimationUUID, const WTF::UUID& destinationAnimationUUID, const std::optional<WebCore::CharacterRange>& characterRangeAfterReplace, const String& string)
{
    if (!characterRangeAfterReplace) {
        m_webPage->didEndPartialIntelligenceTextAnimation();
        return;
    }

    auto sessionRange = contextRangeForActiveWritingToolsSession();
    if (!sessionRange) {
        m_webPage->didEndPartialIntelligenceTextAnimation();
        return;
    }

    RefPtr document = this->document();
    if (!document) {
        m_webPage->didEndPartialIntelligenceTextAnimation();
        ASSERT_NOT_REACHED();
        return;
    }

    auto previouslyReplacedRange = m_alreadyReplacedRange;

    auto replacedCharacterRange = *characterRangeAfterReplace;
    if (previouslyReplacedRange)
        replacedCharacterRange = remainingCharacterRange(*characterRangeAfterReplace, previouslyReplacedRange->range);

    auto replacedRangeAfterReplace = WebCore::resolveCharacterRange(*sessionRange, replacedCharacterRange, defaultTextAnimationControllerTextIteratorBehaviors);

    auto endPosition = makeContainerOffsetPosition(sessionRange->start);
    auto endBoundaryPoint = makeBoundaryPoint(endOfEditableContent(endPosition));

    WTF::UUID unanimatedRangeUUID { WTF::UUID::emptyValue };
    if (endBoundaryPoint) {
        auto unanimatedRange = makeSimpleRangeHelper(replacedRangeAfterReplace.end, *endBoundaryPoint);

        if (unanimatedRange) {
            unanimatedRangeUUID = WTF::UUID::createVersion4();
            m_unanimatedRangeData = { unanimatedRangeUUID, *unanimatedRange };
        }
    }

    auto textIndicatorData = createTextIndicatorForRange(replacedRangeAfterReplace);

    if (!textIndicatorData) {
        m_webPage->didEndPartialIntelligenceTextAnimation();
        return;
    }

    m_webPage->addTextAnimationForAnimationID(destinationAnimationUUID, { WebCore::TextAnimationType::Final, WebCore::TextAnimationRunMode::RunAnimation, unanimatedRangeUUID, sourceAnimationUUID }, *textIndicatorData, [weakWebPage = WeakPtr { *m_webPage }](WebCore::TextAnimationRunMode runMode) mutable {
        if (runMode == WebCore::TextAnimationRunMode::DoNotRun)
            return;

        if (!weakWebPage)
            return;

        weakWebPage->addInitialTextAnimationForActiveWritingToolsSession();
    });

    if (m_activeAnimation == sourceAnimationUUID) {
        m_activeAnimation = std::nullopt;
        if (auto finalReplaceHander = std::exchange(m_finalReplaceHandler, std::nullopt))
            (*finalReplaceHander)(WebCore::TextAnimationRunMode::OnlyReplaceText);
    }

    m_textAnimationRanges.append({ destinationAnimationUUID, replacedCharacterRange });

    m_alreadyReplacedRange = { *characterRangeAfterReplace, string };

#if PLATFORM(IOS_FAMILY)
    // Need to manually turn on/off the text for the animation since the structure of animations is very different on iOS.
    updateUnderlyingTextVisibilityForTextAnimationID(sourceAnimationUUID, true);
    updateUnderlyingTextVisibilityForTextAnimationID(destinationAnimationUUID, false);
#endif
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
        auto animationRange = contextRangeForTextAnimationID(uuid);
        if (!animationRange) {
            if (completionHandler)
                completionHandler();
            return;
        }
        document->markers().addTransparentContentMarker(*animationRange, uuid);
    }

    if (completionHandler)
        completionHandler();
}

void TextAnimationController::saveSnapshotOfTextPlaceholderForAnimation(const WebCore::SimpleRange& placeholderRange)
{
    constexpr OptionSet textIndicatorOptions {
        WebCore::TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        WebCore::TextIndicatorOption::ExpandClipBeyondVisibleRect,
        WebCore::TextIndicatorOption::SkipReplacedContent,
        WebCore::TextIndicatorOption::RespectTextColor
    };

    auto textIndicator = WebCore::TextIndicator::createWithRange(placeholderRange, textIndicatorOptions, WebCore::TextIndicatorPresentationTransition::None, { });
    if (textIndicator)
        m_placeholderTextIndicatorData = textIndicator->data();
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

    // For smart replies, we need to use a blank snapshot
    if (range.collapsed() && (m_placeholderTextIndicatorData))
        return *m_placeholderTextIndicatorData;

    std::optional<WebCore::TextIndicatorData> textIndicatorData;
    constexpr OptionSet textIndicatorOptions {
        WebCore::TextIndicatorOption::IncludeSnapshotOfAllVisibleContentWithoutSelection,
        WebCore::TextIndicatorOption::ExpandClipBeyondVisibleRect,
        WebCore::TextIndicatorOption::SkipReplacedContent,
        WebCore::TextIndicatorOption::RespectTextColor,
#if PLATFORM(VISION)
        WebCore::TextIndicatorOption::SnapshotContentAt3xBaseScale,
#endif
    };

    if (auto textIndicator = WebCore::TextIndicator::createWithRange(range, textIndicatorOptions, WebCore::TextIndicatorPresentationTransition::None, { }))
        textIndicatorData = textIndicator->data();

    return textIndicatorData;
}

void TextAnimationController::clearAnimationsForActiveWritingToolsSession()
{
    removeTransparentMarkersForActiveWritingToolsSession();
    removeInitialTextAnimationForActiveWritingToolsSession();

    auto textAnimationRanges = std::exchange(m_textAnimationRanges, { });

    for (auto textAnimationRange : textAnimationRanges)
        m_webPage->removeTextAnimationForAnimationID(textAnimationRange.animationUUID);

    if (auto finalReplaceHander = std::exchange(m_finalReplaceHandler, std::nullopt))
        (*finalReplaceHander)(WebCore::TextAnimationRunMode::DoNotRun);

    m_alreadyReplacedRange = std::nullopt;
    m_unanimatedRangeData = std::nullopt;
    m_activeAnimation = std::nullopt;
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

void TextAnimationController::enableSourceTextAnimationAfterElementWithID(const String& elementID)
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

    m_manuallyEnabledAnimationRange = createLiveRange(*simpleRange);
}

void TextAnimationController::enableTextAnimationTypeForElementWithID(const String& elementID)
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

    m_manuallyEnabledAnimationRange = createLiveRange(elementRange);
}

} // namespace WebKit

#endif // ENABLE(WRITING_TOOLS)
