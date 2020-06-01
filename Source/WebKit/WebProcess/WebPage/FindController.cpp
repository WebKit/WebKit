/*
 * Copyright (C) 2010, 2015 Apple Inc. All rights reserved.
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

#include "CallbackID.h"
#include "DrawingArea.h"
#include "PluginView.h"
#include "ShareableBitmap.h"
#include "WKPage.h"
#include "WebCoreArgumentCoders.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/DocumentMarkerController.h>
#include <WebCore/FloatQuad.h>
#include <WebCore/FocusController.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/FrameView.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PathUtilities.h>
#include <WebCore/PlatformMouseEvent.h>
#include <WebCore/PluginDocument.h>
#include <WebCore/SimpleRange.h>

#if PLATFORM(COCOA)
#include <WebCore/TextIndicatorWindow.h>
#endif

namespace WebKit {
using namespace WebCore;

WebCore::FindOptions core(FindOptions options)
{
    WebCore::FindOptions result;
    if (options & FindOptionsCaseInsensitive)
        result.add(WebCore::CaseInsensitive);
    if (options & FindOptionsAtWordStarts)
        result.add(WebCore::AtWordStarts);
    if (options & FindOptionsTreatMedialCapitalAsWordStart)
        result.add(WebCore::TreatMedialCapitalAsWordStart);
    if (options & FindOptionsBackwards)
        result.add(WebCore::Backwards);
    if (options & FindOptionsWrapAround)
        result.add(WebCore::WrapAround);
    return result;
}

FindController::FindController(WebPage* webPage)
    : m_webPage(webPage)
{
}

FindController::~FindController()
{
}

void FindController::countStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    if (maxMatchCount == std::numeric_limits<unsigned>::max())
        --maxMatchCount;
    
    auto* pluginView = WebPage::pluginViewForFrame(m_webPage->mainFrame());
    
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

uint32_t FindController::replaceMatches(const Vector<uint32_t>& matchIndices, const String& replacementText, bool selectionOnly)
{
    if (matchIndices.isEmpty())
        return m_webPage->corePage()->replaceSelectionWithText(replacementText);

    // FIXME: This is an arbitrary cap on the maximum number of matches to try and replace, to prevent the web process from
    // hanging while replacing an enormous amount of matches. In the future, we should handle replacement in batches, and
    // periodically update an NSProgress in the UI process when a batch of find-in-page matches are replaced.
    const uint32_t maximumNumberOfMatchesToReplace = 1000;

    Vector<Ref<Range>> rangesToReplace;
    rangesToReplace.reserveCapacity(std::min<uint32_t>(maximumNumberOfMatchesToReplace, matchIndices.size()));
    for (auto index : matchIndices) {
        if (index < m_findMatches.size())
            rangesToReplace.uncheckedAppend(*m_findMatches[index]);
        if (rangesToReplace.size() >= maximumNumberOfMatchesToReplace)
            break;
    }
    return m_webPage->corePage()->replaceRangesWithText(rangesToReplace, replacementText, selectionOnly);
}

static Frame* frameWithSelection(Page* page)
{
    for (Frame* frame = &page->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        if (frame->selection().isRange())
            return frame;
    }

    return 0;
}

void FindController::updateFindUIAfterPageScroll(bool found, const String& string, FindOptions options, unsigned maxMatchCount, DidWrap didWrap, FindUIOriginator originator)
{
    Frame* selectedFrame = frameWithSelection(m_webPage->corePage());
    
    auto* pluginView = WebPage::pluginViewForFrame(m_webPage->mainFrame());

    bool shouldShowOverlay = false;

    if (!found) {
        if (!pluginView)
            m_webPage->corePage()->unmarkAllTextMatches();

        if (selectedFrame)
            selectedFrame->selection().clear();

        hideFindIndicator();
        resetMatchIndex();
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
                m_foundStringMatchIndex += matchCount; // FIXME: Shouldn't this just be "="? Why is it correct to add to -1 here?
            if (m_foundStringMatchIndex >= (int) matchCount)
                m_foundStringMatchIndex -= matchCount;
        }

        // If updating UI after finding an individual match, update the current
        // match rects and inform the UI process that we succeeded.
        // If we're doing a multi-result search and just updating the indicator,
        // this would blow away the results for the other matches.
        // FIXME: This whole class needs a much clearer division between these two paths.
        if (originator == FindUIOriginator::FindString) {
            m_findMatches.clear();
            Vector<IntRect> matchRects;
            if (auto range = m_webPage->corePage()->selection().firstRange()) {
                matchRects = RenderObject::absoluteTextRects(*range);
                m_findMatches.append(createLiveRange(*range));
            }
            m_webPage->send(Messages::WebPageProxy::DidFindString(string, matchRects, matchCount, m_foundStringMatchIndex, didWrap == DidWrap::Yes));
        }
    }

    if (!shouldShowOverlay) {
        if (m_findPageOverlay)
            m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*m_findPageOverlay, PageOverlay::FadeMode::Fade);
    } else {
        if (!m_findPageOverlay) {
            auto findPageOverlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
            m_findPageOverlay = findPageOverlay.ptr();
            m_webPage->corePage()->pageOverlayController().installPageOverlay(WTFMove(findPageOverlay), PageOverlay::FadeMode::Fade);
        }
        m_findPageOverlay->setNeedsDisplay();
    }
    
    if (found && (!(options & FindOptionsShowFindIndicator) || !selectedFrame || !updateFindIndicator(*selectedFrame, shouldShowOverlay)))
        hideFindIndicator();
}

void FindController::findString(const String& string, FindOptions options, unsigned maxMatchCount, Optional<CallbackID> callbackID)
{
    auto* pluginView = WebPage::pluginViewForFrame(m_webPage->mainFrame());

    WebCore::FindOptions coreOptions = core(options);

    // iOS will reveal the selection through a different mechanism, and
    // we need to avoid sending the non-painted selection change to the UI process
    // so that it does not clear the selection out from under us.
#if PLATFORM(IOS_FAMILY)
    coreOptions.add(DoNotRevealSelection);
#endif

    willFindString();

    bool foundStringStartsAfterSelection = false;
    if (!pluginView) {
        if (Frame* selectedFrame = frameWithSelection(m_webPage->corePage())) {
            FrameSelection& fs = selectedFrame->selection();
            if (fs.selectionBounds().isEmpty()) {
                m_findMatches.clear();
                int indexForSelection;
                m_webPage->corePage()->findStringMatchingRanges(string, coreOptions, maxMatchCount, m_findMatches, indexForSelection);
                m_foundStringMatchIndex = indexForSelection;
                foundStringStartsAfterSelection = true;
            }
        }
    }

    m_findMatches.clear();

    bool found;
    DidWrap didWrap = DidWrap::No;
    if (pluginView)
        found = pluginView->findString(string, coreOptions, maxMatchCount);
    else
        found = m_webPage->corePage()->findString(string, coreOptions, &didWrap);

    if (found) {
        didFindString();

        if (!foundStringStartsAfterSelection) {
            if (options & FindOptionsBackwards)
                m_foundStringMatchIndex--;
            else if (!(options & FindOptionsNoIndexChange))
                m_foundStringMatchIndex++;
        }
    }

    RefPtr<WebPage> protectedWebPage = m_webPage;
    m_webPage->drawingArea()->dispatchAfterEnsuringUpdatedScrollPosition([protectedWebPage, found, string, options, maxMatchCount, didWrap] () {
        protectedWebPage->findController().updateFindUIAfterPageScroll(found, string, options, maxMatchCount, didWrap, FindUIOriginator::FindString);
    });

    if (callbackID)
        m_webPage->send(Messages::WebPageProxy::FindStringCallback(found, *callbackID));
}

void FindController::findStringMatches(const String& string, FindOptions options, unsigned maxMatchCount)
{
    m_findMatches.clear();
    int indexForSelection;

    m_webPage->corePage()->findStringMatchingRanges(string, core(options), maxMatchCount, m_findMatches, indexForSelection);

    Vector<Vector<IntRect>> matchRects;
    for (auto& range : m_findMatches)
        matchRects.append(RenderObject::absoluteTextRects(*range));

    m_webPage->send(Messages::WebPageProxy::DidFindStringMatches(string, matchRects, indexForSelection));

    if (!(options & FindOptionsShowOverlay || options & FindOptionsShowFindIndicator))
        return;

    bool found = !m_findMatches.isEmpty();
    m_webPage->drawingArea()->dispatchAfterEnsuringUpdatedScrollPosition([protectedWebPage = makeRefPtr(m_webPage), found, string, options, maxMatchCount] () {
        protectedWebPage->findController().updateFindUIAfterPageScroll(found, string, options, maxMatchCount, DidWrap::No, FindUIOriginator::FindStringMatches);
    });
}

void FindController::getImageForFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    Frame* frame = m_findMatches[matchIndex]->startContainer().document().frame();
    if (!frame)
        return;

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(VisibleSelection(*m_findMatches[matchIndex]));

    RefPtr<ShareableBitmap> selectionSnapshot = WebFrame::fromCoreFrame(*frame)->createSelectionSnapshot();

    frame->selection().setSelection(oldSelection);

    if (!selectionSnapshot)
        return;

    ShareableBitmap::Handle handle;
    selectionSnapshot->createHandle(handle);

    if (handle.isNull())
        return;

    m_webPage->send(Messages::WebPageProxy::DidGetImageForFindMatch(handle, matchIndex));
#if USE(DIRECT2D)
    // Don't destroy the shared handle in the WebContent process. It will be destroyed in the UIProcess.
    selectionSnapshot->leakSharedResource();
#endif
}

void FindController::selectFindMatch(uint32_t matchIndex)
{
    if (matchIndex >= m_findMatches.size())
        return;
    Frame* frame = m_findMatches[matchIndex]->startContainer().document().frame();
    if (!frame)
        return;
    frame->selection().setSelection(VisibleSelection(*m_findMatches[matchIndex]));
}

void FindController::indicateFindMatch(uint32_t matchIndex)
{
    selectFindMatch(matchIndex);

    Frame* selectedFrame = frameWithSelection(m_webPage->corePage());
    if (!selectedFrame)
        return;

    selectedFrame->selection().revealSelection();

    updateFindIndicator(*selectedFrame, !!m_findPageOverlay);
}

void FindController::hideFindUI()
{
    m_findMatches.clear();
    if (m_findPageOverlay)
        m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*m_findPageOverlay, PageOverlay::FadeMode::Fade);

    if (auto* pluginView = WebPage::pluginViewForFrame(m_webPage->mainFrame()))
        pluginView->findString(emptyString(), { }, 0);
    else
        m_webPage->corePage()->unmarkAllTextMatches();
    
    hideFindIndicator();
    resetMatchIndex();
}

#if !PLATFORM(IOS_FAMILY)

bool FindController::updateFindIndicator(Frame& selectedFrame, bool isShowingOverlay, bool shouldAnimate)
{
    auto indicator = TextIndicator::createWithSelectionInFrame(selectedFrame, { TextIndicatorOption::IncludeMarginIfRangeMatchesSelection }, shouldAnimate ? TextIndicatorPresentationTransition::Bounce : TextIndicatorPresentationTransition::None);
    if (!indicator)
        return false;

    m_findIndicatorRect = enclosingIntRect(indicator->selectionRectInRootViewCoordinates());
#if PLATFORM(COCOA)
    m_webPage->send(Messages::WebPageProxy::SetTextIndicator(indicator->data(), static_cast<uint64_t>(isShowingOverlay ? TextIndicatorWindowLifetime::Permanent : TextIndicatorWindowLifetime::Temporary)));
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
}

void FindController::didFailToFindString()
{
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

void FindController::redraw()
{
    if (!m_isShowingFindIndicator)
        return;

    Frame* selectedFrame = frameWithSelection(m_webPage->corePage());
    if (!selectedFrame)
        return;

    updateFindIndicator(*selectedFrame, isShowingOverlay(), false);
}

Vector<FloatRect> FindController::rectsForTextMatchesInRect(IntRect clipRect)
{
    Vector<FloatRect> rects;

    FrameView* mainFrameView = m_webPage->corePage()->mainFrame().view();

    for (Frame* frame = &m_webPage->corePage()->mainFrame(); frame; frame = frame->tree().traverseNext()) {
        Document* document = frame->document();
        if (!document)
            continue;

        for (FloatRect rect : document->markers().renderedRectsForMarkers(DocumentMarker::TextMatch)) {
            if (!frame->isMainFrame())
                rect = mainFrameView->windowToContents(frame->view()->contentsToWindow(enclosingIntRect(rect)));

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
    m_findPageOverlay = 0;
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

    auto overlayBackgroundColor = makeSimpleColorFromFloats(0.1f, 0.1f, 0.1f, 0.25f);
    auto shadowColor = makeSimpleColorFromFloats(0.0f, 0.0f, 0.0f, 0.5f);

    IntRect borderInflatedDirtyRect = dirtyRect;
    borderInflatedDirtyRect.inflate(borderWidth);
    Vector<FloatRect> rects = rectsForTextMatchesInRect(borderInflatedDirtyRect);

    // Draw the background.
    graphicsContext.fillRect(dirtyRect, overlayBackgroundColor);

    Vector<Path> whiteFramePaths = PathUtilities::pathsWithShrinkWrappedRects(rects, findIndicatorRadius());

    GraphicsContextStateSaver stateSaver(graphicsContext);

    // Draw white frames around the holes.
    // We double the thickness because half of the stroke will be erased when we clear the holes.
    graphicsContext.setShadow(FloatSize(shadowOffsetX, shadowOffsetY), shadowBlurRadius, shadowColor);
    graphicsContext.setStrokeColor(Color::white);
    graphicsContext.setStrokeThickness(borderWidth * 2);
    for (auto& path : whiteFramePaths)
        graphicsContext.strokePath(path);

    graphicsContext.clearShadow();

    // Clear out the holes.
    graphicsContext.setCompositeOperation(CompositeOperator::Clear);
    for (auto& path : whiteFramePaths)
        graphicsContext.fillPath(path);

    if (!m_isShowingFindIndicator)
        return;

    if (Frame* selectedFrame = frameWithSelection(m_webPage->corePage())) {
        IntRect findIndicatorRect = selectedFrame->view()->contentsToRootView(enclosingIntRect(selectedFrame->selection().selectionBounds(FrameSelection::ClipToVisibleContent::No)));

        if (findIndicatorRect != m_findIndicatorRect) {
            // We are underneath painting, so it's not safe to mutate the layer tree synchronously.
            callOnMainThread([weakWebPage = makeWeakPtr(m_webPage)] {
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
    else if (Frame *selectedFrame = frameWithSelection(m_webPage->corePage()))
        updateFindIndicator(*selectedFrame, true, false);
}

bool FindController::mouseEvent(PageOverlay&, const PlatformMouseEvent& mouseEvent)
{
    if (mouseEvent.type() == PlatformEvent::MousePressed)
        hideFindUI();

    return false;
}

void FindController::didInvalidateDocumentMarkerRects()
{
    if (m_findPageOverlay)
        m_findPageOverlay->setNeedsDisplay();
}

} // namespace WebKit
