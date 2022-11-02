/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebFoundTextRangeController.h"

#include "WebPage.h"
#include <WebCore/CharacterRange.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/Editor.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/FrameView.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageOverlay.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PathUtilities.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIterator.h>
#include <wtf/Scope.h>

namespace WebKit {
using namespace WebCore;

WebFoundTextRangeController::WebFoundTextRangeController(WebPage& webPage)
    : m_webPage(webPage)
{
}

void WebFoundTextRangeController::findTextRangesForStringMatches(const String& string, OptionSet<FindOptions> options, uint32_t maxMatchCount, CompletionHandler<void(Vector<WebFoundTextRange>&&)>&& completionHandler)
{
    auto result = m_webPage->corePage()->findTextMatches(string, core(options), maxMatchCount, false);
    Vector<WebCore::SimpleRange> findMatches = WTFMove(result.ranges);

    if (!findMatches.isEmpty())
        m_cachedFoundRanges.clear();

    AtomString frameName;
    uint64_t order = 0;
    Vector<WebFoundTextRange> foundTextRanges;
    for (auto& simpleRange : findMatches) {
        auto& document = simpleRange.startContainer().document();

        auto* element = document.documentElement();
        if (!element)
            continue;

        auto currentFrameName = document.frame()->tree().uniqueName();
        if (frameName != currentFrameName) {
            frameName = currentFrameName;
            order++;
        }

        // FIXME: We should get the character ranges at the same time as the SimpleRanges to avoid additional traversals.
        auto range = characterRange(makeBoundaryPointBeforeNodeContents(*element), simpleRange, WebCore::findIteratorOptions());
        auto foundTextRange = WebFoundTextRange { range.location, range.length, frameName.length() ? frameName : emptyAtom(), order };

        m_cachedFoundRanges.add(foundTextRange, simpleRange);
        foundTextRanges.append(foundTextRange);
    }

    completionHandler(WTFMove(foundTextRanges));
}

void WebFoundTextRangeController::replaceFoundTextRangeWithString(const WebFoundTextRange& range, const String& string)
{
    auto simpleRange = simpleRangeFromFoundTextRange(range);
    if (!simpleRange)
        return;

    RefPtr document = documentForFoundTextRange(range);
    if (!document)
        return;

    RefPtr frame = document->frame();
    if (!frame)
        return;

    WebCore::VisibleSelection visibleSelection(*simpleRange);
    OptionSet temporarySelectionOptions { WebCore::TemporarySelectionOption::DoNotSetFocus, WebCore::TemporarySelectionOption::IgnoreSelectionChanges };
    WebCore::TemporarySelectionChange selectionChange(*document, visibleSelection, temporarySelectionOptions);

    frame->editor().replaceSelectionWithText(string, WebCore::Editor::SelectReplacement::Yes, WebCore::Editor::SmartReplace::No, WebCore::EditAction::InsertReplacement);
}

void WebFoundTextRangeController::decorateTextRangeWithStyle(const WebFoundTextRange& range, FindDecorationStyle style)
{
    auto simpleRange = simpleRangeFromFoundTextRange(range);
    if (!simpleRange)
        return;

    auto currentStyleForRange = m_decoratedRanges.get(range);
    if (style == currentStyleForRange)
        return;

    m_decoratedRanges.set(range, style);

    if (currentStyleForRange == FindDecorationStyle::Highlighted && range == m_highlightedRange) {
        m_textIndicator = nullptr;
        m_highlightedRange = { };
    }

    if (style == FindDecorationStyle::Normal)
        simpleRange->start.document().markers().removeMarkers(*simpleRange, WebCore::DocumentMarker::TextMatch);
    else if (style == FindDecorationStyle::Found)
        simpleRange->start.document().markers().addMarker(*simpleRange, WebCore::DocumentMarker::TextMatch);
    else if (style == FindDecorationStyle::Highlighted) {
        m_highlightedRange = range;

        if (m_findPageOverlay)
            setTextIndicatorWithRange(*simpleRange);
        else
            flashTextIndicatorAndUpdateSelectionWithRange(*simpleRange);
    }

    if (m_findPageOverlay)
        m_findPageOverlay->setNeedsDisplay();
}

void WebFoundTextRangeController::scrollTextRangeToVisible(const WebFoundTextRange& range)
{
    auto simpleRange = simpleRangeFromFoundTextRange(range);
    if (!simpleRange)
        return;

    auto* document = documentForFoundTextRange(range);
    if (!document)
        return;

    WebCore::VisibleSelection visibleSelection(*simpleRange);
    OptionSet temporarySelectionOptions { WebCore::TemporarySelectionOption::DelegateMainFrameScroll, WebCore::TemporarySelectionOption::RevealSelectionBounds, WebCore::TemporarySelectionOption::DoNotSetFocus, WebCore::TemporarySelectionOption::UserTriggered };

    if (document->isTopDocument())
        temporarySelectionOptions.add(WebCore::TemporarySelectionOption::SmoothScroll);

    WebCore::TemporarySelectionChange selectionChange(*document, visibleSelection, temporarySelectionOptions);
}

void WebFoundTextRangeController::clearAllDecoratedFoundText()
{
    m_cachedFoundRanges.clear();
    m_decoratedRanges.clear();
    m_webPage->corePage()->unmarkAllTextMatches();

    m_highlightedRange = { };
    m_textIndicator = nullptr;

    if (m_findPageOverlay)
        m_findPageOverlay->setNeedsDisplay();
}

void WebFoundTextRangeController::didBeginTextSearchOperation()
{
    if (m_findPageOverlay)
        m_findPageOverlay->stopFadeOutAnimation();
    else {
        m_findPageOverlay = WebCore::PageOverlay::create(*this, WebCore::PageOverlay::OverlayType::Document);
        m_webPage->corePage()->pageOverlayController().installPageOverlay(*m_findPageOverlay, WebCore::PageOverlay::FadeMode::Fade);
    }

    m_findPageOverlay->setNeedsDisplay();
}

void WebFoundTextRangeController::didEndTextSearchOperation()
{
    if (m_findPageOverlay)
        m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*m_findPageOverlay, WebCore::PageOverlay::FadeMode::Fade);
}

void WebFoundTextRangeController::addLayerForFindOverlay(CompletionHandler<void(WebCore::GraphicsLayer::PlatformLayerID)>&& completionHandler)
{
    if (!m_findPageOverlay) {
        m_findPageOverlay = WebCore::PageOverlay::create(*this, WebCore::PageOverlay::OverlayType::Document, WebCore::PageOverlay::AlwaysTileOverlayLayer::Yes);
        m_webPage->corePage()->pageOverlayController().installPageOverlay(*m_findPageOverlay, WebCore::PageOverlay::FadeMode::DoNotFade);
        m_findPageOverlay->layer().setOpacity(0);
    }

    completionHandler(m_findPageOverlay->layer().primaryLayerID());

    m_findPageOverlay->setNeedsDisplay();
}

void WebFoundTextRangeController::removeLayerForFindOverlay()
{
    if (m_findPageOverlay)
        m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*m_findPageOverlay, WebCore::PageOverlay::FadeMode::DoNotFade);
}

void WebFoundTextRangeController::requestRectForFoundTextRange(const WebFoundTextRange& range, CompletionHandler<void(WebCore::FloatRect)>&& completionHandler)
{
    auto simpleRange = simpleRangeFromFoundTextRange(range);
    if (!simpleRange) {
        completionHandler({ });
        return;
    }

    auto* frameView = simpleRange->startContainer().document().frame()->view();
    completionHandler(frameView->contentsToRootView(unionRect(WebCore::RenderObject::absoluteTextRects(*simpleRange))));
}

void WebFoundTextRangeController::redraw()
{
    if (!m_findPageOverlay)
        return;

    auto setNeedsDisplay = makeScopeExit([findPageOverlay = RefPtr { m_findPageOverlay }] {
        findPageOverlay->setNeedsDisplay();
    });

    if (!m_highlightedRange.length)
        return;

    auto simpleRange = simpleRangeFromFoundTextRange(m_highlightedRange);
    if (!simpleRange)
        return;

    setTextIndicatorWithRange(*simpleRange);
}

void WebFoundTextRangeController::willMoveToPage(WebCore::PageOverlay&, WebCore::Page* page)
{
    if (page)
        return;

    ASSERT(m_findPageOverlay);
    m_findPageOverlay = nullptr;
}

void WebFoundTextRangeController::didMoveToPage(WebCore::PageOverlay&, WebCore::Page*)
{
}

bool WebFoundTextRangeController::mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&)
{
    return false;
}

void WebFoundTextRangeController::drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext& graphicsContext, const WebCore::IntRect& dirtyRect)
{
    constexpr int indicatorRadius = 3;
    constexpr int indicatorBorderWidth = 1;

    constexpr auto highlightColor = WebCore::SRGBA<uint8_t> { 255, 228, 56 };
    constexpr auto foundColor = WebCore::Color::white;
    constexpr auto overlayBackgroundColor = WebCore::SRGBA<uint8_t> { 26, 26, 26, 64 };
    constexpr auto shadowColor = WebCore::Color::black.colorWithAlphaByte(128);

    constexpr float shadowOffsetX = 0;
    constexpr float shadowOffsetY = 0;
    constexpr float shadowBlurRadius = 1;

    WebCore::IntRect borderInflatedDirtyRect = dirtyRect;
    borderInflatedDirtyRect.inflate(indicatorBorderWidth);
    Vector<WebCore::FloatRect> rects = rectsForTextMatchesInRect(borderInflatedDirtyRect);

    graphicsContext.fillRect(dirtyRect, overlayBackgroundColor);

    auto foundFramePaths = WebCore::PathUtilities::pathsWithShrinkWrappedRects(rects, indicatorRadius);

    WebCore::GraphicsContextStateSaver stateSaver(graphicsContext);

    graphicsContext.setShadow(WebCore::FloatSize(shadowOffsetX, shadowOffsetY), shadowBlurRadius, shadowColor);
    graphicsContext.setStrokeColor(foundColor);
    graphicsContext.setStrokeThickness(indicatorBorderWidth * 2);
    for (auto& path : foundFramePaths)
        graphicsContext.strokePath(path);

    graphicsContext.clearShadow();

    graphicsContext.setCompositeOperation(WebCore::CompositeOperator::Clear);
    for (auto& path : foundFramePaths)
        graphicsContext.fillPath(path);

    if (m_textIndicator) {
        graphicsContext.setCompositeOperation(WebCore::CompositeOperator::SourceOver);

        auto* indicatorImage = m_textIndicator->contentImage();
        if (!indicatorImage)
            return;

        auto textBoundingRectInRootViewCoordinates = m_textIndicator->textBoundingRectInRootViewCoordinates();
        auto textRectsInBoundingRectCoordinates = m_textIndicator->textRectsInBoundingRectCoordinates();

        auto textRectsInRootViewCoordinates = textRectsInBoundingRectCoordinates.map([&](auto rect) {
            rect.moveBy(textBoundingRectInRootViewCoordinates.location());
            return rect;
        });

        auto paths = WebCore::PathUtilities::pathsWithShrinkWrappedRects(textRectsInRootViewCoordinates, indicatorRadius);

        graphicsContext.setFillColor(highlightColor);
        for (const auto& path : paths)
            graphicsContext.fillPath(path);

        graphicsContext.drawImage(*indicatorImage, textBoundingRectInRootViewCoordinates);
    }
}

RefPtr<WebCore::TextIndicator> WebFoundTextRangeController::createTextIndicatorForRange(const WebCore::SimpleRange& range, WebCore::TextIndicatorPresentationTransition transition)
{
    constexpr int indicatorMargin = 1;

    OptionSet options { WebCore::TextIndicatorOption::IncludeMarginIfRangeMatchesSelection, WebCore::TextIndicatorOption::DoNotClipToVisibleRect };
    if (WebCore::ImageOverlay::isInsideOverlay(range))
        options.add({ WebCore::TextIndicatorOption::PaintAllContent, WebCore::TextIndicatorOption::PaintBackgrounds });

#if PLATFORM(IOS_FAMILY)
    Ref frame = CheckedRef(m_webPage->corePage()->focusController())->focusedOrMainFrame();
    frame->selection().setUpdateAppearanceEnabled(true);
    frame->selection().updateAppearance();
    frame->selection().setUpdateAppearanceEnabled(false);
#endif

    return WebCore::TextIndicator::createWithRange(range, options, transition, WebCore::FloatSize(indicatorMargin, indicatorMargin));
}

void WebFoundTextRangeController::setTextIndicatorWithRange(const WebCore::SimpleRange& range)
{
    m_textIndicator = createTextIndicatorForRange(range, WebCore::TextIndicatorPresentationTransition::None);
}

void WebFoundTextRangeController::flashTextIndicatorAndUpdateSelectionWithRange(const WebCore::SimpleRange& range)
{
    auto& document = range.startContainer().document();
    document.selection().setSelection(WebCore::VisibleSelection(range), WebCore::FrameSelection::defaultSetSelectionOptions(WebCore::UserTriggered));

    if (auto textIndicator = createTextIndicatorForRange(range, WebCore::TextIndicatorPresentationTransition::Bounce))
        m_webPage->setTextIndicator(textIndicator->data());
}

Vector<WebCore::FloatRect> WebFoundTextRangeController::rectsForTextMatchesInRect(WebCore::IntRect clipRect)
{
    Vector<WebCore::FloatRect> rects;

    RefPtr mainFrameView = m_webPage->corePage()->mainFrame().view();

    for (WebCore::AbstractFrame* frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<WebCore::LocalFrame>(frame);
        if (!localFrame)
            continue;
        RefPtr document = localFrame->document();
        if (!document)
            continue;

        for (auto rect : document->markers().renderedRectsForMarkers(WebCore::DocumentMarker::TextMatch)) {
            if (!localFrame->isMainFrame())
                rect = mainFrameView->windowToContents(localFrame->view()->contentsToWindow(enclosingIntRect(rect)));

            if (rect.isEmpty() || !rect.intersects(clipRect))
                continue;

            rects.append(rect);
        }
    }

    return rects;
}

WebCore::Document* WebFoundTextRangeController::documentForFoundTextRange(const WebFoundTextRange& range) const
{
    auto& mainFrame = m_webPage->corePage()->mainFrame();
    if (range.frameIdentifier.isEmpty())
        return mainFrame.document();

    if (auto* frame = dynamicDowncast<WebCore::LocalFrame>(mainFrame.tree().find(AtomString { range.frameIdentifier }, mainFrame)))
        return frame->document();

    return nullptr;
}

std::optional<WebCore::SimpleRange> WebFoundTextRangeController::simpleRangeFromFoundTextRange(WebFoundTextRange range)
{
    return m_cachedFoundRanges.ensure(range, [&] () -> std::optional<WebCore::SimpleRange> {
        auto* document = documentForFoundTextRange(range);
        if (!document)
            return std::nullopt;

        return resolveCharacterRange(makeRangeSelectingNodeContents(*document->documentElement()), { range.location, range.length }, WebCore::findIteratorOptions());
    }).iterator->value;
}

} // namespace WebKit
