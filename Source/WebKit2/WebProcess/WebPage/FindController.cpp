/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "PluginView.h"
#include "ShareableBitmap.h"
#include "TextIndicator.h"
#include "WKPage.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/FocusController.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/MainFrame.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PluginDocument.h>

using namespace WebCore;

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101000
#define ENABLE_LEGACY_FIND_INDICATOR_STYLE 1
#else
#define ENABLE_LEGACY_FIND_INDICATOR_STYLE 0
#endif

namespace WebKit {

static WebCore::FindOptions core(FindOptions options)
{
    return (options & FindOptionsCaseInsensitive ? CaseInsensitive : 0)
        | (options & FindOptionsAtWordStarts ? AtWordStarts : 0)
        | (options & FindOptionsTreatMedialCapitalAsWordStart ? TreatMedialCapitalAsWordStart : 0)
        | (options & FindOptionsBackwards ? Backwards : 0)
        | (options & FindOptionsWrapAround ? WrapAround : 0);
}

FindController::FindController(WebPage* webPage)
    : m_webPage(webPage)
    , m_findPageOverlay(nullptr)
    , m_isShowingFindIndicator(false)
    , m_foundStringMatchIndex(-1)
{
}

FindController::~FindController()
{
}

static PluginView* pluginViewForFrame(Frame* frame)
{
    if (!frame->document()->isPluginDocument())
        return 0;

    PluginDocument* pluginDocument = static_cast<PluginDocument*>(frame->document());
    return static_cast<PluginView*>(pluginDocument->pluginWidget());
}

void FindController::countStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    if (maxMatchCount == std::numeric_limits<unsigned>::max())
        --maxMatchCount;
    
    PluginView* pluginView = pluginViewForFrame(m_webPage->mainFrame());
    
    unsigned matchCount;

    if (pluginView)
        matchCount = pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
    else {
        matchCount = m_webPage->corePage()->countFindMatches(string, core(options), maxMatchCount + 1);
        m_webPage->corePage()->unmarkAllTextMatches();
    }

    if (matchCount > maxMatchCount)
        matchCount = static_cast<unsigned>(kWKMoreThanMaximumMatchCount);
    
    m_webPage->send(Messages::WebPageProxy::DidCountStringMatches(string, matchCount));
}

static Frame* frameWithSelection(Page* page)
{
    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->selection().isRange())
            return frame;
    }

    return 0;
}

void FindController::updateFindUIAfterPageScroll(bool found, const String& string, FindOptions options, unsigned maxMatchCount)
{
    Frame* selectedFrame = frameWithSelection(m_webPage->corePage());
    
    PluginView* pluginView = pluginViewForFrame(m_webPage->mainFrame());

    bool shouldShowOverlay = false;

    if (!found) {
        if (!pluginView)
            m_webPage->corePage()->unmarkAllTextMatches();

        if (selectedFrame)
            selectedFrame->selection().clear();

        hideFindIndicator();
        didFailToFindString();

        m_webPage->send(Messages::WebPageProxy::DidFailToFindString(string));
    } else {
        shouldShowOverlay = options & FindOptionsShowOverlay;
        bool shouldShowHighlight = options & FindOptionsShowHighlight;
        bool shouldDetermineMatchIndex = options & FindOptionsDetermineMatchIndex;
        unsigned matchCount = 1;

        if (shouldDetermineMatchIndex) {
            if (pluginView)
                matchCount = pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
            else
                matchCount = m_webPage->corePage()->countFindMatches(string, core(options), maxMatchCount + 1);
        }

        if (shouldShowOverlay || shouldShowHighlight) {
            if (maxMatchCount == std::numeric_limits<unsigned>::max())
                --maxMatchCount;

            if (pluginView) {
                if (!shouldDetermineMatchIndex)
                    matchCount = pluginView->countFindMatches(string, core(options), maxMatchCount + 1);
                shouldShowOverlay = false;
            } else {
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
                m_foundStringMatchIndex += matchCount;
            if (m_foundStringMatchIndex >= (int) matchCount)
                m_foundStringMatchIndex -= matchCount;
        }

        m_webPage->send(Messages::WebPageProxy::DidFindString(string, matchCount, m_foundStringMatchIndex));

        if (!(options & FindOptionsShowFindIndicator) || !selectedFrame || !updateFindIndicator(*selectedFrame, shouldShowOverlay))
            hideFindIndicator();
    }

    if (!shouldShowOverlay) {
        if (m_findPageOverlay)
            m_webPage->mainFrame()->pageOverlayController().uninstallPageOverlay(m_findPageOverlay, PageOverlay::FadeMode::Fade);
    } else {
        if (!m_findPageOverlay) {
            RefPtr<PageOverlay> findPageOverlay = PageOverlay::create(*this);
            m_findPageOverlay = findPageOverlay.get();
            m_webPage->mainFrame()->pageOverlayController().installPageOverlay(findPageOverlay.release(), PageOverlay::FadeMode::Fade);
        }
        m_findPageOverlay->setNeedsDisplay();
    }
}

void FindController::findString(const String& string, FindOptions options, unsigned maxMatchCount)
{
    PluginView* pluginView = pluginViewForFrame(m_webPage->mainFrame());

    WebCore::FindOptions coreOptions = core(options);

    // iOS will reveal the selection through a different mechanism, and
    // we need to avoid sending the non-painted selection change to the UI process
    // so that it does not clear the selection out from under us.
#if PLATFORM(IOS)
    coreOptions = static_cast<FindOptions>(coreOptions | DoNotRevealSelection);
#endif

    willFindString();

    bool foundStringStartsAfterSelection = false;
    if (!pluginView) {
        if (Frame* selectedFrame = frameWithSelection(m_webPage->corePage())) {
            FrameSelection& fs = selectedFrame->selection();
            if (fs.selectionBounds().isEmpty()) {
                m_findMatches.clear();
                int indexForSelection;
                m_webPage->corePage()->findStringMatchingRanges(string, core(options), maxMatchCount, m_findMatches, indexForSelection);
                m_foundStringMatchIndex = indexForSelection;
                foundStringStartsAfterSelection = true;
            }
        }
    }

    bool found;
    if (pluginView)
        found = pluginView->findString(string, coreOptions, maxMatchCount);
    else
        found = m_webPage->corePage()->findString(string, coreOptions);

    if (found && !foundStringStartsAfterSelection) {
        if (options & FindOptionsBackwards)
            m_foundStringMatchIndex--;
        else
            m_foundStringMatchIndex++;
    }

    m_webPage->drawingArea()->dispatchAfterEnsuringUpdatedScrollPosition(WTF::bind(&FindController::updateFindUIAfterPageScroll, this, found, string, options, maxMatchCount));
}

void FindController::findStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    m_findMatches.clear();
    int indexForSelection;

    m_webPage->corePage()->findStringMatchingRanges(string, core(options), maxMatchCount, m_findMatches, indexForSelection);

    Vector<Vector<IntRect>> matchRects;
    for (size_t i = 0; i < m_findMatches.size(); ++i) {
        Vector<IntRect> rects;
        m_findMatches[i]->textRects(rects);
        matchRects.append(WTF::move(rects));
    }

    m_webPage->send(Messages::WebPageProxy::DidFindStringMatches(string, matchRects, indexForSelection));
}

void FindController::getImageForFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    Frame* frame = m_findMatches[matchIndex]->startContainer()->document().frame();
    if (!frame)
        return;

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(VisibleSelection(m_findMatches[matchIndex].get()));

    RefPtr<ShareableBitmap> selectionSnapshot = WebFrame::fromCoreFrame(*frame)->createSelectionSnapshot();

    frame->selection().setSelection(oldSelection);

    if (!selectionSnapshot)
        return;

    ShareableBitmap::Handle handle;
    selectionSnapshot->createHandle(handle);

    if (handle.isNull())
        return;

    m_webPage->send(Messages::WebPageProxy::DidGetImageForFindMatch(handle, matchIndex));
}

void FindController::selectFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    Frame* frame = m_findMatches[matchIndex]->startContainer()->document().frame();
    if (!frame)
        return;
    frame->selection().setSelection(VisibleSelection(m_findMatches[matchIndex].get()));
}

void FindController::hideFindUI()
{
    m_findMatches.clear();
    if (m_findPageOverlay)
        m_webPage->mainFrame()->pageOverlayController().uninstallPageOverlay(m_findPageOverlay, PageOverlay::FadeMode::Fade);

    PluginView* pluginView = pluginViewForFrame(m_webPage->mainFrame());
    
    if (pluginView)
        pluginView->findString(emptyString(), 0, 0);
    else
        m_webPage->corePage()->unmarkAllTextMatches();
    
    hideFindIndicator();
}

#if !PLATFORM(IOS)
bool FindController::updateFindIndicator(Frame& selectedFrame, bool isShowingOverlay, bool shouldAnimate)
{
    RefPtr<TextIndicator> indicator = TextIndicator::createWithSelectionInFrame(*WebFrame::fromCoreFrame(selectedFrame), shouldAnimate ? TextIndicator::PresentationTransition::Bounce : TextIndicator::PresentationTransition::None);
    if (!indicator)
        return false;

    m_findIndicatorRect = enclosingIntRect(indicator->selectionRectInWindowCoordinates());
    m_webPage->send(Messages::WebPageProxy::SetTextIndicator(indicator->data(), !isShowingOverlay));
    m_isShowingFindIndicator = true;

    return true;
}

void FindController::hideFindIndicator()
{
    if (!m_isShowingFindIndicator)
        return;

    m_webPage->send(Messages::WebPageProxy::ClearTextIndicator());
    m_isShowingFindIndicator = false;
    m_foundStringMatchIndex = -1;
    didHideFindIndicator();
}

void FindController::willFindString()
{
}

void FindController::didFailToFindString()
{
}

void FindController::didHideFindIndicator()
{
}
#endif

void FindController::showFindIndicatorInSelection()
{
    Frame& selectedFrame = m_webPage->corePage()->focusController().focusedOrMainFrame();
    updateFindIndicator(selectedFrame, false);
}

void FindController::deviceScaleFactorDidChange()
{
    ASSERT(isShowingOverlay());

    Frame* selectedFrame = frameWithSelection(m_webPage->corePage());
    if (!selectedFrame)
        return;

    updateFindIndicator(*selectedFrame, true, false);
}

Vector<IntRect> FindController::rectsForTextMatches()
{
    Vector<IntRect> rects;

    for (Frame* frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document)
            continue;

        IntRect visibleRect = frame->view()->visibleContentRect();
        Vector<IntRect> frameRects = document->markers().renderedRectsForMarkers(DocumentMarker::TextMatch);
        IntPoint frameOffset(-frame->view()->documentScrollOffsetRelativeToViewOrigin().width(), -frame->view()->documentScrollOffsetRelativeToViewOrigin().height());
        frameOffset = frame->view()->convertToContainingWindow(frameOffset);

        for (Vector<IntRect>::iterator it = frameRects.begin(), end = frameRects.end(); it != end; ++it) {
            it->intersect(visibleRect);
            it->move(frameOffset.x(), frameOffset.y());
            rects.append(*it);
        }
    }

    return rects;
}

void FindController::pageOverlayDestroyed(PageOverlay&)
{
}

void FindController::willMoveToPage(PageOverlay&, Page* page)
{
    if (page)
        return;

    ASSERT(m_findPageOverlay);
    m_findPageOverlay = 0;
}
    
void FindController::didMoveToPage(PageOverlay&, Page*)
{
}

#if ENABLE(LEGACY_FIND_INDICATOR_STYLE)
const float shadowOffsetX = 0;
const float shadowOffsetY = 1;
const float shadowBlurRadius = 2;
const float shadowColorAlpha = 1;
#else
const float shadowOffsetX = 0;
const float shadowOffsetY = 0;
const float shadowBlurRadius = 1;
const float shadowColorAlpha = 0.5;
#endif

void FindController::drawRect(PageOverlay&, GraphicsContext& graphicsContext, const IntRect& dirtyRect)
{
    Color overlayBackgroundColor(0.1f, 0.1f, 0.1f, 0.25f);

    Vector<IntRect> rects = rectsForTextMatches();

    // Draw the background.
    graphicsContext.fillRect(dirtyRect, overlayBackgroundColor, ColorSpaceSRGB);

    {
        GraphicsContextStateSaver stateSaver(graphicsContext);

        graphicsContext.setShadow(FloatSize(shadowOffsetX, shadowOffsetY), shadowBlurRadius, Color(0.0f, 0.0f, 0.0f, shadowColorAlpha), ColorSpaceSRGB);
        graphicsContext.setFillColor(Color::white, ColorSpaceSRGB);

        // Draw white frames around the holes.
        for (auto& rect : rects) {
            IntRect whiteFrameRect = rect;
            whiteFrameRect.inflate(1);
            graphicsContext.fillRect(whiteFrameRect);
        }
    }

    // Clear out the holes.
    for (auto& rect : rects)
        graphicsContext.clearRect(rect);

    if (!m_isShowingFindIndicator)
        return;

    if (Frame* selectedFrame = frameWithSelection(m_webPage->corePage())) {
        IntRect findIndicatorRect = selectedFrame->view()->contentsToWindow(enclosingIntRect(selectedFrame->selection().selectionBounds()));

        if (findIndicatorRect != m_findIndicatorRect)
            hideFindIndicator();
    }
}

bool FindController::mouseEvent(PageOverlay&, const PlatformMouseEvent& mouseEvent)
{
    if (mouseEvent.type() == PlatformEvent::MousePressed)
        hideFindUI();

    return false;
}

} // namespace WebKit
