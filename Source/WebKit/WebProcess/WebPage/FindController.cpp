/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "FindIndicator.h"
#include "MessageSenderInlines.h"
#include "PluginView.h"
#include "WKPage.h"
#include "WebFrame.h"
#include "WebImage.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/BoundaryPointInlines.h>
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/DocumentMarkers.h>
#include <WebCore/DocumentView.h>
#include <WebCore/FindOptions.h>
#include <WebCore/FindRevealAlgorithms.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameDestructionObserverInlines.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/GeometryUtilities.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageAnalysisQueue.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/ImageOverlay.h>
#include <WebCore/LocalFrameInlines.h>
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
#include <wtf/Forward.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FindController);

FindController::FindController(WebPage* webPage)
    : m_webPage(webPage)
#if PLATFORM(IOS_FAMILY)
    , m_findIndicator(WTF::makeUnique<FindIndicatorIOS>(webPage))
#else
    , m_findIndicator(WTF::makeUnique<FindIndicator>(webPage))
#endif
{
}

FindController::~FindController() = default;

#if ENABLE(PDF_PLUGIN)

PluginView* FindController::mainFramePlugIn()
{
    return protect(m_webPage.get())->mainFramePlugIn();
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
        RefPtr webPage { m_webPage.get() };
        matchCount = protect(webPage->corePage())->countFindMatches(string, core(options), maxMatchCount + 1);
        protect(webPage->corePage())->unmarkAllTextMatches();
    }

    if (matchCount > maxMatchCount)
        matchCount = static_cast<unsigned>(kWKMoreThanMaximumMatchCount);

    completionHandler(matchCount);
}

uint32_t FindController::replaceMatches(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly)
{
    RefPtr webPage { m_webPage.get() };
    if (matchIndices.isEmpty())
        return protect(webPage->corePage())->replaceSelectionWithText(replacementText);

    // FIXME: This is an arbitrary cap on the maximum number of matches to try and replace, to prevent the web process from
    // hanging while replacing an enormous amount of matches. In the future, we should handle replacement in batches, and
    // periodically update an NSProgress in the UI process when a batch of find-in-page matches are replaced.
    constexpr uint32_t maximumNumberOfMatchesToReplace = 1000;

    Vector<SimpleRange> rangesToReplace;
    rangesToReplace.reserveInitialCapacity(std::min<uint32_t>(maximumNumberOfMatchesToReplace, matchIndices.size()));
    for (auto index : matchIndices) {
        if (index < m_findMatches.size())
            rangesToReplace.append(m_findMatches[index]);
        if (rangesToReplace.size() >= maximumNumberOfMatchesToReplace)
            break;
    }
    return protect(webPage->corePage())->replaceRangesWithText(rangesToReplace, replacementText, selectionOnly);
}

RefPtr<LocalFrame> FindController::frameWithSelection(Page* page)
{
    for (RefPtr<Frame> frame = page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;

        if (localFrame->selection().isCaretOrRange())
            return localFrame;
    }
    return nullptr;
}

void FindController::updateFindUIAfterIncrementalFind(bool found, const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, WebCore::DidWrap didWrap, std::optional<FrameIdentifier> idOfFrameContainingString, CompletionHandler<void(std::optional<WebCore::FrameIdentifier>, Vector<IntRect>&&, uint32_t, int32_t, bool)>&& completionHandler)
{
    RefPtr webPage { m_webPage.get() };
    RefPtr selectedFrame = frameWithSelection(protect(webPage->corePage()).get());

#if ENABLE(PDF_PLUGIN)
    RefPtr pluginView = mainFramePlugIn();
#endif

    bool shouldShowOverlay = found && options.contains(FindOptions::ShowOverlay);
    unsigned matchCount = 0;
    Vector<IntRect> matchRects;
    if (!found) {
        if (selectedFrame && !options.contains(FindOptions::DoNotSetSelection))
            protect(selectedFrame->selection())->clear();

        hideFindUI();

        completionHandler(idOfFrameContainingString, WTF::move(matchRects), matchCount, m_foundStringMatchIndex.value_or(-1), didWrap == WebCore::DidWrap::Yes);
        return;
    }

    bool needsCount = options.contains(FindOptions::DetermineMatchIndex);
    bool needsMarking = shouldShowOverlay || options.contains(FindOptions::ShowHighlight);
    matchCount = 1;

    // marking implicitly counts, so we try not to double count here
    if (needsMarking)
        matchCount = markMatches(string, options, maxMatchCount);
    else if (needsCount)
        matchCount = getMatchCount(string, options, maxMatchCount);

    if (matchCount > maxMatchCount)
        matchCount = static_cast<unsigned>(kWKMoreThanMaximumMatchCount);

    updateMatchIndex(matchCount, options);

    if (auto range = protect(webPage->corePage())->selection().firstRange())
        matchRects = RenderObject::absoluteTextRects(*range);

    updateFindPageOverlay(shouldShowOverlay);
    updateFindIndicatorIfNeeded(found, options, shouldShowOverlay);
    completionHandler(idOfFrameContainingString, WTF::move(matchRects), matchCount, m_foundStringMatchIndex.value_or(0), didWrap == WebCore::DidWrap::Yes);
}

unsigned FindController::markMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    maxMatchCount = std::min(std::numeric_limits<unsigned>::max() - 1, maxMatchCount);

#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        return pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
#endif

    RefPtr webPage { m_webPage.get() };
    bool shouldShowHighlight = options.contains(FindOptions::ShowHighlight);

    protect(webPage->corePage())->unmarkAllTextMatches();
    return protect(webPage->corePage())->markAllMatchesForText(string, core(options), shouldShowHighlight, maxMatchCount + 1);
}

unsigned FindController::getMatchCount(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn(); pluginView)
        return pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
#endif

    RefPtr webPage { m_webPage.get() };
    return protect(webPage->corePage())->countFindMatches(string, core(options), maxMatchCount + 1);
}

void FindController::updateMatchIndex(unsigned matchCount, OptionSet<FindOptions> options)
{
    if (!options.contains(FindOptions::DetermineMatchIndex))
        return;

    if (matchCount == static_cast<unsigned>(kWKMoreThanMaximumMatchCount)) {
        m_foundStringMatchIndex = std::nullopt;
        return;
    }

    if (!m_foundStringMatchIndex)
        m_foundStringMatchIndex = matchCount;
    else if (*m_foundStringMatchIndex >= matchCount)
        *m_foundStringMatchIndex -= matchCount;
}

void FindController::updateFindUIAfterFindingAllMatches(bool found, const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount)
{
    RefPtr webPage { m_webPage.get() };
    RefPtr selectedFrame = frameWithSelection(protect(webPage->corePage()).get());

#if ENABLE(PDF_PLUGIN)
    RefPtr pluginView = mainFramePlugIn();
#endif

    bool shouldShowOverlay = found && options.contains(FindOptions::ShowOverlay);
    if (!found) {
        if (selectedFrame && !options.contains(FindOptions::DoNotSetSelection))
            protect(selectedFrame->selection())->clear();
        hideFindUI();
        return;
    }

    bool shouldShowHighlight = options.contains(FindOptions::ShowHighlight);
    if (shouldShowOverlay || shouldShowHighlight) {
        protect(webPage->corePage())->unmarkAllTextMatches();
        protect(webPage->corePage())->markAllMatchesForText(string, core(options), shouldShowHighlight, maxMatchCount + 1);
    }

    updateFindPageOverlay(shouldShowOverlay);
    updateFindIndicatorIfNeeded(found, options, shouldShowOverlay);
}

void FindController::updateFindPageOverlay(bool shouldShowOverlay)
{
    if (!shouldShowOverlay) {
        if (RefPtr findPageOverlay = m_findPageOverlay.get())
            m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*findPageOverlay, PageOverlay::FadeMode::Fade);
        return;
    }

    RefPtr findPageOverlay = m_findPageOverlay.get();
    if (!findPageOverlay) {
        findPageOverlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
        m_findPageOverlay = findPageOverlay.get();
#if ENABLE(PDF_PLUGIN)
        if (RefPtr pluginView = mainFramePlugIn(); pluginView && !pluginView->drawsFindOverlay())
            findPageOverlay->setNeedsSynchronousScrolling(true);
#endif
        m_webPage->corePage()->pageOverlayController().installPageOverlay(*findPageOverlay, PageOverlay::FadeMode::Fade);
    }
    findPageOverlay->setNeedsDisplay();
}

void FindController::updateFindIndicatorIfNeeded(bool found, OptionSet<FindOptions> options, bool shouldShowOverlay)
{
    bool shouldSetSelection = !options.contains(FindOptions::DoNotSetSelection);
    bool wantsFindIndicator = found && options.contains(FindOptions::ShowFindIndicator);
    bool canShowFindIndicator = frameWithSelection(protect(protect(m_webPage.get())->corePage()).get());
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        canShowFindIndicator |= !pluginView->drawsFindOverlay();
#endif
    if (shouldSetSelection && (!wantsFindIndicator || !canShowFindIndicator || !m_findIndicator->update(frameWithSelection(protect(m_webPage->corePage()).get()), shouldShowOverlay)))
        m_findIndicator->hide();
}

#if ENABLE(IMAGE_ANALYSIS)
void FindController::findStringIncludingImages(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(std::optional<FrameIdentifier>, Vector<IntRect>&&, uint32_t, int32_t, bool)>&& completionHandler)
{
    protect(protect(m_webPage.get())->corePage())->analyzeImagesForFindInPage([weakPage = WeakPtr { m_webPage }, string, options, maxMatchCount, completionHandler = WTF::move(completionHandler)]() mutable {
        if (weakPage)
            weakPage->findController().findString(string, options, maxMatchCount, WTF::move(completionHandler));
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
    // and reveal the selection in FindIndicatorStrategy::didFindString.
    coreOptions.add(FindOption::DoNotRevealSelection);

    m_findIndicator->willFindString();

    bool foundStringStartsAfterSelection = false;
    RefPtr webPage { m_webPage.get() };
#if ENABLE(PDF_PLUGIN)
    if (!pluginView)
#endif
    {
        if (RefPtr selectedFrame = frameWithSelection(protect(webPage->corePage()).get())) {
            if (protect(selectedFrame->selection())->selectionBounds().isEmpty()) {
                auto result = protect(webPage->corePage())->findTextMatches(string, coreOptions, maxMatchCount);
                m_foundStringMatchIndex = result.indexForSelection;
                foundStringStartsAfterSelection = true;
            }
        }
    }

    bool found;
    std::optional<FrameIdentifier> idOfFrameContainingString;
    auto didWrap = WebCore::DidWrap::No;
#if ENABLE(PDF_PLUGIN)
    if (pluginView) {
        found = pluginView->findString(string, coreOptions, maxMatchCount);
        if (auto* frame = pluginView->frame(); frame && found)
            idOfFrameContainingString = frame->frameID();
    } else
#endif
    {
        auto [frameID, _] = protect(webPage->corePage())->findString(string, coreOptions, &didWrap);
        idOfFrameContainingString = frameID;
        found = idOfFrameContainingString.has_value();
    }

    if (found && !options.contains(FindOptions::DoNotSetSelection)) {
        m_findIndicator->didFindString(frameWithSelection(protect(m_webPage->corePage()).get()));

        if (!foundStringStartsAfterSelection && m_foundStringMatchIndex) {
            if (options.contains(FindOptions::Backwards))
                (*m_foundStringMatchIndex)--;
            else if (!options.contains(FindOptions::NoIndexChange))
                (*m_foundStringMatchIndex)++;
        }
    }

    protect(webPage->drawingArea())->dispatchAfterEnsuringUpdatedScrollPosition([webPage, found, string, options, maxMatchCount, didWrap, idOfFrameContainingString, completionHandler = WTF::move(completionHandler)]() mutable {
        webPage->findController().updateFindUIAfterIncrementalFind(found, string, options, maxMatchCount, didWrap, idOfFrameContainingString, WTF::move(completionHandler));
    });
}

void FindController::findStringMatches(const String& string, OptionSet<FindOptions> options, unsigned maxMatchCount, CompletionHandler<void(Vector<Vector<WebCore::IntRect>>, int32_t)>&& completionHandler)
{
    RefPtr webPage { m_webPage.get() };
    auto result = protect(webPage->corePage())->findTextMatches(string, core(options), maxMatchCount);
    m_findMatches = WTF::move(result.ranges);

    auto matchRects = m_findMatches.map([](auto& range) {
        return RenderObject::absoluteTextRects(range);
    });
    completionHandler(matchRects, result.indexForSelection.value_or(-1));

    if (!options.contains(FindOptions::ShowOverlay) && !options.contains(FindOptions::ShowFindIndicator))
        return;

    bool found = !m_findMatches.isEmpty();
    protect(webPage->drawingArea())->dispatchAfterEnsuringUpdatedScrollPosition([webPage, found, string, options, maxMatchCount]() {
        webPage->findController().updateFindUIAfterFindingAllMatches(found, string, options, maxMatchCount);
    });
}

void FindController::getImageForFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    RefPtr frame = m_findMatches[matchIndex].start.document().frame();
    if (!frame)
        return;

    CheckedRef frameSelection = frame->selection();
    auto oldSelection = frameSelection->selection();
    frameSelection->setSelection(m_findMatches[matchIndex]);

    auto selectionSnapshot = protect(WebFrame::fromCoreFrame(*frame))->createSelectionSnapshot();

    frameSelection->setSelection(oldSelection);

    if (!selectionSnapshot)
        return;

    auto handle = selectionSnapshot->createHandle();
    if (!handle || !selectionSnapshot->parameters())
        return;

    m_webPage->send(Messages::WebPageProxy::DidGetImageForFindMatch(*selectionSnapshot->parameters(), WTF::move(*handle), matchIndex));
}

void FindController::selectFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    RefPtr frame = m_findMatches[matchIndex].start.document().frame();
    if (!frame)
        return;
    protect(frame->selection())->setSelection(m_findMatches[matchIndex]);
}

void FindController::indicateFindMatch(uint32_t matchIndex)
{
    m_findIndicator->willFindString();

    selectFindMatch(matchIndex);
    RefPtr selectedFrame = frameWithSelection(protect(protect(m_webPage.get())->corePage()).get());
    if (!selectedFrame)
        return;

    m_findIndicator->didFindString(selectedFrame);
    m_findIndicator->update(selectedFrame, !!m_findPageOverlay);
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
        protect(protect(m_webPage.get())->corePage())->unmarkAllTextMatches();

    m_findIndicator->hide();
    resetMatchIndex();

#if ENABLE(IMAGE_ANALYSIS)
    if (RefPtr imageAnalysisQueue = m_webPage->corePage()->imageAnalysisQueueIfExists())
        imageAnalysisQueue->clearDidBecomeEmptyCallback();
#endif
}

void FindController::hideFindIndicator()
{
    m_findIndicator->hide();
}

void FindController::resetMatchIndex()
{
    m_foundStringMatchIndex = std::nullopt;
}

void FindController::showFindIndicatorInSelection()
{
    m_findIndicator->update(frameWithSelection(protect(m_webPage->corePage())), false);
}

void FindController::deviceScaleFactorDidChange()
{
    ASSERT(isShowingOverlay());

    m_findIndicator->update(frameWithSelection(protect(m_webPage->corePage())), true, false);
}

void FindController::redraw()
{
    if (!m_findIndicator->isShowing())
        return;

    m_findIndicator->update(frameWithSelection(protect(m_webPage->corePage())), isShowingOverlay(), false);
}

Vector<FloatRect> FindController::rectsForTextMatchesInRect(IntRect clipRect)
{
#if ENABLE(PDF_PLUGIN)
    if (RefPtr pluginView = mainFramePlugIn())
        return pluginView->rectsForTextMatchesInRect(clipRect);
#endif

    Vector<FloatRect> rects;
    RefPtr mainFrameView = protect(protect(protect(m_webPage.get())->corePage())->mainFrame())->virtualView();
    for (RefPtr frame = m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        auto* localFrame = dynamicDowncast<LocalFrame>(frame.get());
        if (!localFrame)
            continue;
        RefPtr document = localFrame->document();
        if (!document)
            continue;

        for (FloatRect rect : protect(document->markers())->renderedRectsForMarkers(DocumentMarkerType::TextMatch)) {
            if (!localFrame->isMainFrame())
                rect = mainFrameView->windowToContents(protect(localFrame->view())->contentsToWindow(enclosingIntRect(rect)));

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

constexpr float shadowOffsetX = 0;
constexpr float shadowOffsetY = 0;
constexpr float shadowBlurRadius = 1;
constexpr unsigned findIndicatorRadius = 3;
void FindController::drawRect(PageOverlay&, GraphicsContext& graphicsContext, const IntRect& dirtyRect)
{
    constexpr int borderWidth = 1;

    constexpr auto overlayBackgroundColor = SRGBA<uint8_t> { 26, 26, 26, 64 };
    constexpr auto shadowColor = Color::black.colorWithAlphaByte(128);

    IntRect borderInflatedDirtyRect = dirtyRect;
    borderInflatedDirtyRect.inflate(borderWidth);
    Vector<FloatRect> rects = rectsForTextMatchesInRect(borderInflatedDirtyRect);

    // Draw the background.
    graphicsContext.fillRect(dirtyRect, overlayBackgroundColor);

    Vector<Path> whiteFramePaths = PathUtilities::pathsWithShrinkWrappedRects(rects, findIndicatorRadius);

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

    if (!m_findIndicator->isShowing())
        return;

    if (RefPtr selectedFrame = frameWithSelection(protect(protect(m_webPage)->corePage()))) {
        auto findIndicatorRect = protect(selectedFrame->view())->contentsToRootView(enclosingIntRect(protect(selectedFrame->selection())->selectionBounds(FrameSelection::ClipToVisibleContent::No)));

        if (findIndicatorRect != m_findIndicator->rect()) {
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
    if (m_findIndicator->shouldHideOnScroll())
        m_findIndicator->hide();
    else
        m_findIndicator->update(frameWithSelection(protect(m_webPage->corePage())), true, false);
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
