/*
 * Copyright (C) 2010-2022 Apple Inc. All rights reserved.
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
#include "FindController.h"

#include "DrawingArea.h"
#include "MessageSenderInlines.h"
#include "PluginView.h"
#include "WKPage.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebImage.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/DocumentInlines.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageAnalysisQueue.h>
#include <WebCore/ImageOverlay.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PathUtilities.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/Range.h>
#include <WebCore/RenderObject.h>
#include <WebCore/ShareableBitmap.h>
#include <WebCore/SimpleRange.h>
#include <wtf/TZoneMallocInlines.h>

#if PLATFORM(COCOA)
#include <WebCore/TextIndicatorWindow.h>
#endif

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FindController);

FindController::FindController(WebPage* webPage)
    : m_webPage(webPage)
{
}

FindController::~FindController()
{
}

#if ENABLE(PDF_PLUGIN)

PluginView* FindController::mainFramePlugIn()
{
    return m_webPage->mainFramePlugIn();
}

#endif

void FindController::countStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(uint32_t)>&& completionHandler)
{
    if (maxMatchCount == std::numeric_limits<unsigned>::max())
        --maxMatchCount;

    unsigned matchCount;
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        matchCount = pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
    else
#endif
    {
        matchCount = m_webPage->corePage()->countFindMatches(string, core(options), maxMatchCount + 1);
        m_webPage->corePage()->unmarkAllTextMatches();
    }

    if (matchCount > maxMatchCount)
        matchCount = static_cast<unsigned>(kWKMoreThanMaximumMatchCount);

    completionHandler(matchCount);
}

uint32_t FindController::replaceMatches(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly)
{
    if (matchIndices.isEmpty())
        return m_webPage->corePage()->replaceSelectionWithText(replacementText);

    // FIXME: This is an arbitrary cap on the maximum number of matches to try and replace, to prevent the web process from
    // hanging while replacing an enormous amount of matches. In the future, we should handle replacement in batches, and
    // periodically update an NSProgress in the UI process when a batch of find-in-page matches are replaced.
    const uint32_t maximumNumberOfMatchesToReplace = 1000;

    Vector<SimpleRange> rangesToReplace;
    rangesToReplace.reserveInitialCapacity(std::min<uint32_t>(maximumNumberOfMatchesToReplace, matchIndices.size()));
    for (auto index : matchIndices) {
        if (index < m_findMatches.size())
            rangesToReplace.append(m_findMatches[index]);
        if (rangesToReplace.size() >= maximumNumberOfMatchesToReplace)
            break;
    }
    return m_webPage->corePage()->replaceRangesWithText(rangesToReplace, replacementText, selectionOnly);
}

RefPtr<LocalFrame> FindController::frameWithSelection(Page* page)
{
    for (RefPtr<Frame> frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        if (localFrame->selection().isRange())
            return localFrame;
    }
    return nullptr;
}

void FindController::updateFindUIAfterPageScroll(bool found, const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, DidWrap didWrap, std::optional<FrameIdentifier> idOfFrameContainingString, CompletionHandler<void(std::optional<WebCore::FrameIdentifier>, Vector<IntRect>&&, uint32_t, int32_t, bool)>&& completionHandler)
{
    RefPtr selectedFrame = frameWithSelection(m_webPage->corePage());

#if ENABLE(PDF_PLUGIN)
    RefPtr pluginView = mainFramePlugIn();
#endif

    bool shouldShowOverlay = false;
    bool shouldSetSelection = !options.contains(FindOptions::DoNotSetSelection);
    unsigned matchCount = 0;
    Vector<IntRect> matchRects;
    if (!found) {
#if ENABLE(PDF_PLUGIN)
        if (!pluginView)
#endif
            m_webPage->corePage()->unmarkAllTextMatches();

        if (selectedFrame && shouldSetSelection)
            selectedFrame->selection().clear();

        hideFindIndicator();
        resetMatchIndex();
    } else {
        shouldShowOverlay = options.contains(FindOptions::ShowOverlay);
        bool shouldShowHighlight = options.contains(FindOptions::ShowHighlight);
        bool shouldDetermineMatchIndex = options.contains(FindOptions::DetermineMatchIndex);
        matchCount = 1;

        if (shouldDetermineMatchIndex) {
#if ENABLE(PDF_PLUGIN)
            if (pluginView)
                matchCount = pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
            else
#endif
                matchCount = m_webPage->corePage()->countFindMatches(string, core(options), maxMatchCount + 1);
        }

        if (shouldShowOverlay || shouldShowHighlight) {
            if (maxMatchCount == std::numeric_limits<unsigned>::max())
                --maxMatchCount;

#if ENABLE(PDF_PLUGIN)
            if (pluginView) {
                if (!shouldDetermineMatchIndex)
                    matchCount = pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
                shouldShowOverlay = !pluginView->drawsFindOverlay();
            } else
#endif
            {
                m_webPage->corePage()->unmarkAllTextMatches();
                matchCount = m_webPage->corePage()->markAllMatchesForText(string, core(options), shouldShowHighlight, maxMatchCount + 1);
            }

            // If we have a large number of matches, we don't want to take the time to paint the overlay.
            if (matchCount > maxMatchCount) {
                shouldShowOverlay = false;
                matchCount = static_cast<unsigned>(kWKMoreThanMaximumMatchCount);
            }
        }
        if (matchCount == static_cast<unsigned>(kWKMoreThanMaximumMatchCount))
            m_foundStringMatchIndex = -1;
        else {
            if (m_foundStringMatchIndex < 0)
                m_foundStringMatchIndex += matchCount; // FIXME: Shouldn't this just be "="? Why is it correct to add to -1 here?
            if (m_foundStringMatchIndex >= (int) matchCount)
                m_foundStringMatchIndex -= matchCount;
        }

        // If updating UI after finding an individual match, update the current
        // match rects and inform the UI process that we succeeded.
        // If we're doing a multi-result search and just updating the indicator,
        // this would blow away the results for the other matches.
        // FIXME: This whole class needs a much clearer division between these two paths.
        if (idOfFrameContainingString) {
            m_findMatches.clear();
            if (auto range = m_webPage->corePage()->selection().firstRange()) {
                matchRects = RenderObject::absoluteTextRects(*range);
                m_findMatches.append(*range);
            }
        }
    }

    if (!shouldShowOverlay) {
        if (RefPtr findPageOverlay = m_findPageOverlay.get())
            m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*findPageOverlay, PageOverlay::FadeMode::Fade);
    } else {
        RefPtr findPageOverlay = m_findPageOverlay.get();
        if (!findPageOverlay) {
            findPageOverlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
            m_findPageOverlay = findPageOverlay.get();
#if ENABLE(PDF_PLUGIN)
            // FIXME: Remove this once UnifiedPDFPlugin makes the overlay scroll along with the contents.
            if (pluginView && !pluginView->drawsFindOverlay())
                findPageOverlay->setNeedsSynchronousScrolling(true);
#endif
            m_webPage->corePage()->pageOverlayController().installPageOverlay(*findPageOverlay, PageOverlay::FadeMode::Fade);
        }
        findPageOverlay->setNeedsDisplay();
    }

    bool wantsFindIndicator = found && options.contains(FindOptions::ShowFindIndicator);
    bool canShowFindIndicator = selectedFrame;
#if ENABLE(PDF_PLUGIN)
    canShowFindIndicator |= pluginView && !pluginView->drawsFindOverlay();
#endif
    if (shouldSetSelection && (!wantsFindIndicator || !canShowFindIndicator || !updateFindIndicator(shouldShowOverlay)))
        hideFindIndicator();

    completionHandler(idOfFrameContainingString, WTFMove(matchRects), matchCount, m_foundStringMatchIndex, didWrap == DidWrap::Yes);
}

#if ENABLE(IMAGE_ANALYSIS)
void FindController::findStringIncludingImages(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(std::optional<FrameIdentifier>, Vector<IntRect>&&, uint32_t, int32_t, bool)>&& completionHandler)
{
    m_webPage->corePage()->analyzeImagesForFindInPage([weakPage = WeakPtr { m_webPage }, string, options, maxMatchCount, completionHandler = WTFMove(completionHandler)]() mutable {
        if (weakPage)
            weakPage->findController().findString(string, options, maxMatchCount, WTFMove(completionHandler));
        else
            completionHandler({ }, { }, { }, { }, { });
    });
}
#endif

void FindController::findString(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(std::optional<FrameIdentifier>, Vector<IntRect>&&, uint32_t, int32_t, bool)>&& completionHandler)
{
#if ENABLE(PDF_PLUGIN)
    RefPtr pluginView = mainFramePlugIn();
#endif

    WebCore::FindOptions coreOptions = core(options);

    // iOS will reveal the selection through a different mechanism, and
    // we need to avoid sending the non-painted selection change to the UI process
    // so that it does not clear the selection out from under us.
    //
    // To share logic between platforms, prevent Editor from revealing the selection
    // and reveal the selection in FindController::didFindString.
    coreOptions.add(FindOption::DoNotRevealSelection);

    willFindString();

    bool foundStringStartsAfterSelection = false;
#if ENABLE(PDF_PLUGIN)
    if (!pluginView)
#endif
    {
        if (RefPtr selectedFrame = frameWithSelection(m_webPage->corePage())) {
            if (selectedFrame->selection().selectionBounds().isEmpty()) {
                auto result = m_webPage->corePage()->findTextMatches(string, coreOptions, maxMatchCount);
                m_findMatches = WTFMove(result.ranges);
                m_foundStringMatchIndex = result.indexForSelection;
                foundStringStartsAfterSelection = true;
            }
        }
    }

    m_findMatches.clear();

    bool found;
    std::optional<FrameIdentifier> idOfFrameContainingString;
    DidWrap didWrap = DidWrap::No;
#if ENABLE(PDF_PLUGIN)
    if (pluginView) {
        found = pluginView->findString(string, coreOptions, maxMatchCount);
        if (auto* frame = pluginView->frame(); frame && found)
            idOfFrameContainingString = frame->frameID();
    } else
#endif
    {
        idOfFrameContainingString = m_webPage->corePage()->findString(string, coreOptions, &didWrap);
        found = idOfFrameContainingString.has_value();
    }

    if (found && !options.contains(FindOptions::DoNotSetSelection)) {
        didFindString();

        if (!foundStringStartsAfterSelection) {
            if (options.contains(FindOptions::Backwards))
                m_foundStringMatchIndex--;
            else if (!options.contains(FindOptions::NoIndexChange))
                m_foundStringMatchIndex++;
        }
    }

    RefPtr<WebPage> protectedWebPage { m_webPage.get() };
    m_webPage->drawingArea()->dispatchAfterEnsuringUpdatedScrollPosition([protectedWebPage, found, string, options, maxMatchCount, didWrap, idOfFrameContainingString, completionHandler = WTFMove(completionHandler)]() mutable {
        protectedWebPage->findController().updateFindUIAfterPageScroll(found, string, options, maxMatchCount, didWrap, idOfFrameContainingString, WTFMove(completionHandler));
    });
}

void FindController::findStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(Vector<Vector<WebCore::IntRect>>, int32_t)>&& completionHandler)
{
    auto result = m_webPage->corePage()->findTextMatches(string, core(options), maxMatchCount);
    m_findMatches = WTFMove(result.ranges);

    auto matchRects = m_findMatches.map([](auto& range) {
        return RenderObject::absoluteTextRects(range);
    });
    completionHandler(matchRects, result.indexForSelection);

    if (!options.contains(FindOptions::ShowOverlay) && !options.contains(FindOptions::ShowFindIndicator))
        return;

    bool found = !m_findMatches.isEmpty();
    m_webPage->drawingArea()->dispatchAfterEnsuringUpdatedScrollPosition([protectedWebPage = RefPtr { m_webPage.get() }, found, string, options, maxMatchCount]() {
        protectedWebPage->findController().updateFindUIAfterPageScroll(found, string, options, maxMatchCount, DidWrap::No, std::nullopt);
    });
}

void FindController::findRectsForStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(Vector<FloatRect>&&)>&& completionHandler)
{
    auto result = m_webPage->corePage()->findTextMatches(string, core(options), maxMatchCount);
    m_findMatches = WTFMove(result.ranges);

    auto rects = m_findMatches.map([&] (auto& range) {
        FloatRect rect = unionRect(RenderObject::absoluteTextRects(range));
        return range.startContainer().document().frame()->view()->contentsToRootView(rect);
    });

    completionHandler(WTFMove(rects));

    if (!options.contains(FindOptions::ShowOverlay) && !options.contains(FindOptions::ShowFindIndicator))
        return;

    bool found = !m_findMatches.isEmpty();
    m_webPage->drawingArea()->dispatchAfterEnsuringUpdatedScrollPosition([protectedWebPage = RefPtr { m_webPage.get() }, found, string, options, maxMatchCount] () {
        protectedWebPage->findController().updateFindUIAfterPageScroll(found, string, options, maxMatchCount, DidWrap::No, std::nullopt);
    });
}

void FindController::getImageForFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    RefPtr frame = m_findMatches[matchIndex].start.document().frame();
    if (!frame)
        return;

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(m_findMatches[matchIndex]);

    auto selectionSnapshot = WebFrame::fromCoreFrame(*frame)->createSelectionSnapshot();

    frame->selection().setSelection(oldSelection);

    if (!selectionSnapshot)
        return;

    auto handle = selectionSnapshot->createHandle();
    if (!handle || !selectionSnapshot->parameters())
        return;

    m_webPage->send(Messages::WebPageProxy::DidGetImageForFindMatch(*selectionSnapshot->parameters(), WTFMove(*handle), matchIndex));
}

void FindController::selectFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    RefPtr frame = m_findMatches[matchIndex].start.document().frame();
    if (!frame)
        return;
    frame->selection().setSelection(m_findMatches[matchIndex]);
}

void FindController::indicateFindMatch(uint32_t matchIndex)
{
    willFindString();

    selectFindMatch(matchIndex);

    if (!frameWithSelection(m_webPage->corePage()))
        return;

    didFindString();

    updateFindIndicator(!!m_findPageOverlay);
}

void FindController::hideFindUI()
{
    m_findMatches.clear();
    if (RefPtr findPageOverlay = m_findPageOverlay.get())
        m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*findPageOverlay, PageOverlay::FadeMode::Fade);

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        pluginView->findString(emptyString(), { }, 0);
    else
#endif
        m_webPage->corePage()->unmarkAllTextMatches();
    
    hideFindIndicator();
    resetMatchIndex();

#if ENABLE(IMAGE_ANALYSIS)
    if (auto imageAnalysisQueue = m_webPage->corePage()->imageAnalysisQueueIfExists())
        imageAnalysisQueue->clearDidBecomeEmptyCallback();
#endif
}

#if !PLATFORM(IOS_FAMILY)

bool FindController::updateFindIndicator(bool isShowingOverlay, bool shouldAnimate)
{
    OptionSet<TextIndicatorOption> textIndicatorOptions { TextIndicatorOption::IncludeMarginIfRangeMatchesSelection };
    auto presentationTransition = shouldAnimate ? TextIndicatorPresentationTransition::Bounce : TextIndicatorPresentationTransition::None;

    auto [frame, indicator] = [&]() -> std::tuple<RefPtr<Frame>, RefPtr<TextIndicator>> {
#if ENABLE(PDF_PLUGIN)
        if (RefPtr pluginView = mainFramePlugIn())
            return { m_webPage->mainFrame(), pluginView->textIndicatorForCurrentSelection(textIndicatorOptions, presentationTransition) };
#endif
        if (RefPtr selectedFrame = frameWithSelection(m_webPage->corePage())) {
            if (auto selectedRange = selectedFrame->selection().selection().range(); selectedRange && ImageOverlay::isInsideOverlay(*selectedRange))
                textIndicatorOptions.add({ TextIndicatorOption::PaintAllContent, TextIndicatorOption::PaintBackgrounds });

            return { selectedFrame, TextIndicator::createWithSelectionInFrame(*selectedFrame, textIndicatorOptions, presentationTransition) };
        }

        return { };
    }();

    if (!indicator)
        return false;

    m_findIndicatorRect = enclosingIntRect(indicator->selectionRectInRootViewCoordinates());
#if PLATFORM(COCOA)
    m_webPage->send(Messages::WebPageProxy::SetTextIndicatorFromFrame(frame->frameID(), indicator->data(), static_cast<uint64_t>(isShowingOverlay ? WebCore::TextIndicatorLifetime::Permanent : WebCore::TextIndicatorLifetime::Temporary)));
#endif
    m_isShowingFindIndicator = true;

    return true;
}

void FindController::hideFindIndicator()
{
    if (!m_isShowingFindIndicator)
        return;

    m_webPage->send(Messages::WebPageProxy::ClearTextIndicator());
    m_isShowingFindIndicator = false;
    didHideFindIndicator();
}

void FindController::resetMatchIndex()
{
    m_foundStringMatchIndex = -1;
}

void FindController::willFindString()
{
}

void FindController::didFindString()
{
    RefPtr selectedFrame = frameWithSelection(m_webPage->corePage());
    if (!selectedFrame)
        return;

    selectedFrame->selection().revealSelection();
}

void FindController::didHideFindIndicator()
{
}
    
unsigned FindController::findIndicatorRadius() const
{
    return 0;
}
    
bool FindController::shouldHideFindIndicatorOnScroll() const
{
    return true;
}

#endif

void FindController::showFindIndicatorInSelection()
{
    updateFindIndicator(false);
}

void FindController::deviceScaleFactorDidChange()
{
    ASSERT(isShowingOverlay());

    updateFindIndicator(true, false);
}

void FindController::redraw()
{
    if (!m_isShowingFindIndicator)
        return;

    updateFindIndicator(isShowingOverlay(), false);
}

Vector<FloatRect> FindController::rectsForTextMatchesInRect(IntRect clipRect)
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        return pluginView->rectsForTextMatchesInRect(clipRect);
#endif

    Vector<FloatRect> rects;
    RefPtr mainFrameView = m_webPage->corePage()->mainFrame().virtualView();
    for (RefPtr frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        RefPtr document = localFrame->document();
        if (!document)
            continue;

        for (FloatRect rect : document->markers().renderedRectsForMarkers(DocumentMarker::Type::TextMatch)) {
            if (!localFrame->isMainFrame())
                rect = mainFrameView->windowToContents(localFrame->view()->contentsToWindow(enclosingIntRect(rect)));

            if (rect.isEmpty() || !rect.intersects(clipRect))
                continue;

            rects.append(rect);
        }
    }

    return rects;
}

void FindController::willMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        return;

    ASSERT(m_findPageOverlay);
    m_findPageOverlay = nullptr;
}
    
void FindController::didMoveToPage(PageOverlay&, Page*)
{
}

const float shadowOffsetX = 0;
const float shadowOffsetY = 0;
const float shadowBlurRadius = 1;

void FindController::drawRect(PageOverlay&, GraphicsContext& graphicsContext, const IntRect& dirtyRect)
{
    const int borderWidth = 1;

    constexpr auto overlayBackgroundColor = SRGBA<uint8_t> { 26, 26, 26, 64 };
    constexpr auto shadowColor = Color::black.colorWithAlphaByte(128);

    IntRect borderInflatedDirtyRect = dirtyRect;
    borderInflatedDirtyRect.inflate(borderWidth);
    Vector<FloatRect> rects = rectsForTextMatchesInRect(borderInflatedDirtyRect);

    // Draw the background.
    graphicsContext.fillRect(dirtyRect, overlayBackgroundColor);

    Vector<Path> whiteFramePaths = PathUtilities::pathsWithShrinkWrappedRects(rects, findIndicatorRadius());

    GraphicsContextStateSaver stateSaver(graphicsContext);

    // Draw white frames around the holes.
    // We double the thickness because half of the stroke will be erased when we clear the holes.
    graphicsContext.setDropShadow({ { shadowOffsetX, shadowOffsetY }, shadowBlurRadius, shadowColor, ShadowRadiusMode::Default });
    graphicsContext.setStrokeColor(Color::white);
    graphicsContext.setStrokeThickness(borderWidth * 2);
    for (auto& path : whiteFramePaths)
        graphicsContext.strokePath(path);

    graphicsContext.clearDropShadow();

    // Clear out the holes.
    graphicsContext.setCompositeOperation(CompositeOperator::Clear);
    for (auto& path : whiteFramePaths)
        graphicsContext.fillPath(path);

    if (!m_isShowingFindIndicator)
        return;

    if (RefPtr selectedFrame = frameWithSelection(m_webPage->corePage())) {
        IntRect findIndicatorRect = selectedFrame->view()->contentsToRootView(enclosingIntRect(selectedFrame->selection().selectionBounds(FrameSelection::ClipToVisibleContent::No)));

        if (findIndicatorRect != m_findIndicatorRect) {
            // We are underneath painting, so it's not safe to mutate the layer tree synchronously.
            callOnMainRunLoop([weakWebPage = WeakPtr { m_webPage }] {
                if (!weakWebPage)
                    return;
                weakWebPage->findController().didScrollAffectingFindIndicatorPosition();
            });
        }
    }
}

void FindController::didScrollAffectingFindIndicatorPosition()
{
    if (shouldHideFindIndicatorOnScroll())
        hideFindIndicator();
    else
        updateFindIndicator(true, false);
}

bool FindController::mouseEvent(PageOverlay&, const PlatformMouseEvent& mouseEvent)
{
    if (mouseEvent.type() == PlatformEvent::Type::MousePressed)
        hideFindUI();

    return false;
}

void FindController::didInvalidateFindRects()
{
    if (RefPtr findPageOverlay = m_findPageOverlay.get())
        findPageOverlay->setNeedsDisplay();
}

} // namespace WebKit
