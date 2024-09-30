/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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

#include "PluginView.h"
#include "WebPage.h"
#include <WebCore/CharacterRange.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/Editor.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/GraphicsLayer.h>
#include <WebCore/ImageOverlay.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PathUtilities.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/SimpleRange.h>
#include <WebCore/TextIterator.h>
#include <wtf/Scope.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebFoundTextRangeController);

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
        Ref document = simpleRange.startContainer().document();

        RefPtr element = document->documentElement();
        if (!element)
            continue;

        auto currentFrameName = document->frame()->tree().uniqueName();
        if (frameName != currentFrameName) {
            frameName = currentFrameName;
            order++;
        }

        // FIXME: We should get the character ranges at the same time as the SimpleRanges to avoid additional traversals.
        auto range = characterRange(makeBoundaryPointBeforeNodeContents(*element), simpleRange, WebCore::findIteratorOptions());
        auto domData = WebFoundTextRange::DOMData { range.location, range.length };
        auto foundTextRange = WebFoundTextRange { domData, frameName.length() ? frameName : emptyAtom(), order };

        m_cachedFoundRanges.add(foundTextRange, simpleRange);
        foundTextRanges.append(foundTextRange);
    }

#if ENABLE(PDF_PLUGIN)
    for (RefPtr frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;

        if (RefPtr pluginView = WebPage::pluginViewForFrame(localFrame.get())) {
            auto currentFrameName = frame->tree().uniqueName();
            order++;

            Vector<WebFoundTextRange::PDFData> pdfMatches = pluginView->findTextMatches(string, core(options));
            for (auto& pdfMatch : pdfMatches)
                foundTextRanges.append(WebFoundTextRange { pdfMatch, frameName.length() ? frameName : emptyAtom(), order });
        }
    }
#endif

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
    auto currentStyleForRange = m_decoratedRanges.get(range);
    if (style == currentStyleForRange)
        return;

    m_decoratedRanges.set(range, style);

    if (currentStyleForRange == FindDecorationStyle::Highlighted && range == m_highlightedRange) {
        m_textIndicator = nullptr;
        m_highlightedRange = { };
    }

    if (auto simpleRange = simpleRangeFromFoundTextRange(range)) {
        switch (style) {
        case FindDecorationStyle::Normal:
            simpleRange->start.document().markers().removeMarkers(*simpleRange, WebCore::DocumentMarker::Type::TextMatch);
            break;
        case FindDecorationStyle::Found:
            simpleRange->start.document().markers().addMarker(*simpleRange, WebCore::DocumentMarker::Type::TextMatch);
            break;
        case FindDecorationStyle::Highlighted: {
            m_highlightedRange = range;

            if (m_findPageOverlay)
                setTextIndicatorWithRange(*simpleRange);
            else
                flashTextIndicatorAndUpdateSelectionWithRange(*simpleRange);

            break;
        }
        }
    }

#if ENABLE(PDF_PLUGIN)
    if (style == FindDecorationStyle::Highlighted && std::holds_alternative<WebFoundTextRange::PDFData>(range.data)) {
        m_highlightedRange = range;
        if (m_findPageOverlay)
            setTextIndicatorWithPDFRange(m_highlightedRange);
        else
            flashTextIndicatorAndUpdateSelectionWithPDFRange(m_highlightedRange);
    }
#endif

    if (m_findPageOverlay)
        m_findPageOverlay->setNeedsDisplay();
}

void WebFoundTextRangeController::scrollTextRangeToVisible(const WebFoundTextRange& range)
{
    WTF::switchOn(range.data,
        [&] (const WebKit::WebFoundTextRange::DOMData&) {
            auto simpleRange = simpleRangeFromFoundTextRange(range);
            if (!simpleRange)
                return;

            RefPtr document = documentForFoundTextRange(range);
            if (!document)
                return;

            WebCore::VisibleSelection visibleSelection(*simpleRange);
            OptionSet temporarySelectionOptions { WebCore::TemporarySelectionOption::DelegateMainFrameScroll, WebCore::TemporarySelectionOption::RevealSelectionBounds, WebCore::TemporarySelectionOption::DoNotSetFocus, WebCore::TemporarySelectionOption::UserTriggered };

            if (document->isTopDocument())
                temporarySelectionOptions.add(WebCore::TemporarySelectionOption::SmoothScroll);

            WebCore::TemporarySelectionChange selectionChange(*document, visibleSelection, temporarySelectionOptions);
        },
        [&] (const WebKit::WebFoundTextRange::PDFData& pdfData) {
#if ENABLE(PDF_PLUGIN)
            RefPtr frame = frameForFoundTextRange(range);
            if (!frame)
                return;

            RefPtr pluginView = WebPage::pluginViewForFrame(frame.get());
            if (!pluginView)
                return;

            pluginView->scrollToRevealTextMatch(pdfData);
#else
            UNUSED_PARAM(pdfData);
#endif
        }
    );
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

void WebFoundTextRangeController::addLayerForFindOverlay(CompletionHandler<void(std::optional<WebCore::PlatformLayerIdentifier>)>&& completionHandler)
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

    RefPtr frameView = simpleRange->startContainer().document().frame()->view();
    completionHandler(frameView->contentsToRootView(unionRect(WebCore::RenderObject::absoluteTextRects(*simpleRange))));
}

void WebFoundTextRangeController::redraw()
{
    if (!m_findPageOverlay)
        return;

    auto setNeedsDisplay = makeScopeExit([findPageOverlay = RefPtr { m_findPageOverlay }] {
        findPageOverlay->setNeedsDisplay();
    });

    WTF::switchOn(m_highlightedRange.data,
        [&] (const WebKit::WebFoundTextRange::DOMData& domData) {
            if (!domData.length)
                return;

            if (auto simpleRange = simpleRangeFromFoundTextRange(m_highlightedRange))
                setTextIndicatorWithRange(*simpleRange);
        },
        [&] (const WebKit::WebFoundTextRange::PDFData&) {
            setTextIndicatorWithPDFRange(m_highlightedRange);
        }
    );
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

    graphicsContext.setDropShadow({ { shadowOffsetX, shadowOffsetY }, shadowBlurRadius, shadowColor, ShadowRadiusMode::Default });
    graphicsContext.setStrokeColor(foundColor);
    graphicsContext.setStrokeThickness(indicatorBorderWidth * 2);
    for (auto& path : foundFramePaths)
        graphicsContext.strokePath(path);

    graphicsContext.clearDropShadow();

    graphicsContext.setCompositeOperation(WebCore::CompositeOperator::Clear);
    for (auto& path : foundFramePaths)
        graphicsContext.fillPath(path);

    if (m_textIndicator && !m_textIndicator->selectionRectInRootViewCoordinates().isEmpty()) {
        RefPtr indicatorImage = m_textIndicator->contentImage();
        if (!indicatorImage)
            return;

        auto textBoundingRectInRootViewCoordinates = m_textIndicator->textBoundingRectInRootViewCoordinates();
        auto textRectsInBoundingRectCoordinates = m_textIndicator->textRectsInBoundingRectCoordinates();

        auto textRectsInRootViewCoordinates = textRectsInBoundingRectCoordinates.map([&](auto rect) {
            rect.moveBy(textBoundingRectInRootViewCoordinates.location());
            return rect;
        });

        auto paths = WebCore::PathUtilities::pathsWithShrinkWrappedRects(textRectsInRootViewCoordinates, indicatorRadius);

        graphicsContext.setCompositeOperation(WebCore::CompositeOperator::SourceOver);
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
    if (RefPtr frame = m_webPage->corePage()->checkedFocusController()->focusedOrMainFrame()) {
        frame->selection().setUpdateAppearanceEnabled(true);
        frame->selection().updateAppearance();
        frame->selection().setUpdateAppearanceEnabled(false);
    }
#endif

    return WebCore::TextIndicator::createWithRange(range, options, transition, WebCore::FloatSize(indicatorMargin, indicatorMargin));
}

void WebFoundTextRangeController::setTextIndicatorWithRange(const WebCore::SimpleRange& range)
{
    m_textIndicator = createTextIndicatorForRange(range, WebCore::TextIndicatorPresentationTransition::None);
}

void WebFoundTextRangeController::flashTextIndicatorAndUpdateSelectionWithRange(const WebCore::SimpleRange& range)
{
    Ref document = range.startContainer().document();
    document->selection().setSelection(WebCore::VisibleSelection(range), WebCore::FrameSelection::defaultSetSelectionOptions(WebCore::UserTriggered::Yes));

    if (auto textIndicator = createTextIndicatorForRange(range, WebCore::TextIndicatorPresentationTransition::Bounce))
        m_webPage->setTextIndicator(textIndicator->data());
}

RefPtr<WebCore::TextIndicator> WebFoundTextRangeController::createTextIndicatorForPDFRange(const WebFoundTextRange& range, WebCore::TextIndicatorPresentationTransition transition)
{
#if ENABLE(PDF_PLUGIN)
    RefPtr frame = frameForFoundTextRange(range);
    if (!frame)
        return { };

    RefPtr pluginView = WebPage::pluginViewForFrame(frame.get());
    if (!pluginView)
        return { };

    if (auto* pdfData = std::get_if<WebFoundTextRange::PDFData>(&range.data))
        return pluginView->textIndicatorForTextMatch(*pdfData, transition);
#endif

    return { };
}

void WebFoundTextRangeController::setTextIndicatorWithPDFRange(const WebFoundTextRange& range)
{
    m_textIndicator = createTextIndicatorForPDFRange(range, WebCore::TextIndicatorPresentationTransition::None);
}

void WebFoundTextRangeController::flashTextIndicatorAndUpdateSelectionWithPDFRange(const WebFoundTextRange& range)
{
    if (auto textIndicator = createTextIndicatorForPDFRange(range, WebCore::TextIndicatorPresentationTransition::Bounce))
        m_webPage->setTextIndicator(textIndicator->data());
}

Vector<WebCore::FloatRect> WebFoundTextRangeController::rectsForTextMatchesInRect(WebCore::IntRect clipRect)
{
    Vector<WebCore::FloatRect> rects;
    Ref mainFrame = m_webPage->corePage()->mainFrame();
    RefPtr mainFrameView = mainFrame->virtualView();
    for (RefPtr<Frame> frame = mainFrame.ptr(); frame; frame = frame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<WebCore::LocalFrame>(*frame);
        if (!localFrame)
            continue;

#if ENABLE(PDF_PLUGIN)
        if (RefPtr pluginView = WebPage::pluginViewForFrame(localFrame.get())) {
            auto frameName = frame->tree().uniqueName();
            if (!frameName.length())
                frameName = emptyAtom();

            for (auto& [range, style] : m_decoratedRanges) {
                if (style != FindDecorationStyle::Found)
                    continue;

                if (range.frameIdentifier != frameName)
                    continue;

                if (auto* pdfData = std::get_if<WebFoundTextRange::PDFData>(&range.data)) {
                    for (auto& rect : pluginView->rectsForTextMatch(*pdfData))
                        rects.append(rect);
                }
            }

            continue;
        }
#endif

        RefPtr document = localFrame->document();
        if (!document)
            continue;

        for (auto rect : document->markers().renderedRectsForMarkers(WebCore::DocumentMarker::Type::TextMatch)) {
            if (!localFrame->isMainFrame())
                rect = mainFrameView->windowToContents(localFrame->view()->contentsToWindow(enclosingIntRect(rect)));

            if (rect.isEmpty() || !rect.intersects(clipRect))
                continue;

            rects.append(rect);
        }
    }

    return rects;
}

WebCore::LocalFrame* WebFoundTextRangeController::frameForFoundTextRange(const WebFoundTextRange& range) const
{
    RefPtr mainFrame = dynamicDowncast<LocalFrame>(m_webPage->corePage()->mainFrame());
    if (!mainFrame)
        return nullptr;

    if (range.frameIdentifier.isEmpty())
        return mainFrame.get();

    return dynamicDowncast<WebCore::LocalFrame>(mainFrame->tree().findByUniqueName(AtomString { range.frameIdentifier }, *mainFrame));
}

WebCore::Document* WebFoundTextRangeController::documentForFoundTextRange(const WebFoundTextRange& range) const
{
    if (RefPtr frame = frameForFoundTextRange(range))
        return frame->document();

    return nullptr;
}

std::optional<WebCore::SimpleRange> WebFoundTextRangeController::simpleRangeFromFoundTextRange(WebFoundTextRange range)
{
    return m_cachedFoundRanges.ensure(range, [&] () -> std::optional<WebCore::SimpleRange> {
        if (!std::holds_alternative<WebFoundTextRange::DOMData>(range.data))
            return std::nullopt;

        RefPtr document = documentForFoundTextRange(range);
        if (!document)
            return std::nullopt;

        Ref documentElement = *document->documentElement();
        auto domData = std::get<WebFoundTextRange::DOMData>(range.data);
        return resolveCharacterRange(makeRangeSelectingNodeContents(documentElement), { domData.location, domData.length }, WebCore::findIteratorOptions());
    }).iterator->value;
}

} // namespace WebKit
