/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import <config.h>

#if PLATFORM(IOS_FAMILY)

#import "FindController.h"
#import "FindIndicatorOverlayClientIOS.h"
#import "SmartMagnificationControllerMessages.h"
#import "WebCoreArgumentCoders.h"
#import "WebPage.h"
#import "WebPageProxyMessages.h"
#import <WebCore/Editor.h>
#import <WebCore/FocusController.h>
#import <WebCore/Frame.h>
#import <WebCore/FrameView.h>
#import <WebCore/GraphicsContext.h>
#import <WebCore/Page.h>
#import <WebCore/PageOverlayController.h>
#import <WebCore/PathUtilities.h>
#import <WebCore/Settings.h>
#import <WebCore/TextIndicator.h>

namespace WebKit {
using namespace WebCore;

const int cornerRadius = 3;
const int totalHorizontalMargin = 1;
const int totalVerticalMargin = 1;

const TextIndicatorOptions findTextIndicatorOptions = TextIndicatorOptionIncludeMarginIfRangeMatchesSelection | TextIndicatorOptionDoNotClipToVisibleRect;

static Color highlightColor()
{
    return Color(255, 228, 56, 255);
}

void FindIndicatorOverlayClientIOS::drawRect(PageOverlay& overlay, GraphicsContext& context, const IntRect& dirtyRect)
{
    float scaleFactor = m_frame.page()->deviceScaleFactor();

    if (m_frame.settings().delegatesPageScaling())
        scaleFactor *= m_frame.page()->pageScaleFactor();

    // If the page scale changed, we need to paint a new TextIndicator.
    if (m_textIndicator->contentImageScaleFactor() != scaleFactor)
        m_textIndicator = TextIndicator::createWithSelectionInFrame(m_frame, findTextIndicatorOptions, TextIndicatorPresentationTransition::None, FloatSize(totalHorizontalMargin, totalVerticalMargin));

    if (!m_textIndicator)
        return;

    Image* indicatorImage = m_textIndicator->contentImage();
    if (!indicatorImage)
        return;

    Vector<FloatRect> textRectsInBoundingRectCoordinates = m_textIndicator->textRectsInBoundingRectCoordinates();
    Vector<Path> paths = PathUtilities::pathsWithShrinkWrappedRects(textRectsInBoundingRectCoordinates, cornerRadius);

    context.setFillColor(highlightColor());
    for (const auto& path : paths)
        context.fillPath(path);

    context.drawImage(*indicatorImage, overlay.bounds());
}

bool FindController::updateFindIndicator(Frame& selectedFrame, bool isShowingOverlay, bool shouldAnimate)
{
    if (m_findIndicatorOverlay) {
        m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*m_findIndicatorOverlay, PageOverlay::FadeMode::DoNotFade);
        m_findIndicatorOverlay = nullptr;
        m_isShowingFindIndicator = false;
    }

    auto textIndicator = TextIndicator::createWithSelectionInFrame(selectedFrame, findTextIndicatorOptions, TextIndicatorPresentationTransition::None, FloatSize(totalHorizontalMargin, totalVerticalMargin));
    if (!textIndicator)
        return false;

    m_findIndicatorOverlayClient = makeUnique<FindIndicatorOverlayClientIOS>(selectedFrame, textIndicator.get());
    m_findIndicatorRect = enclosingIntRect(textIndicator->selectionRectInRootViewCoordinates());
    m_findIndicatorOverlay = PageOverlay::create(*m_findIndicatorOverlayClient, PageOverlay::OverlayType::Document);
    m_webPage->corePage()->pageOverlayController().installPageOverlay(*m_findIndicatorOverlay, PageOverlay::FadeMode::DoNotFade);

    m_findIndicatorOverlay->setFrame(enclosingIntRect(textIndicator->textBoundingRectInRootViewCoordinates()));
    m_findIndicatorOverlay->setNeedsDisplay();

    if (shouldAnimate) {
        bool isReplaced;
        const VisibleSelection& visibleSelection = selectedFrame.selection().selection();
        FloatRect renderRect = visibleSelection.start().containerNode()->renderRect(&isReplaced);
        IntRect startRect = visibleSelection.visibleStart().absoluteCaretBounds();

        m_webPage->send(Messages::SmartMagnificationController::ScrollToRect(startRect.center(), renderRect));
    }

    m_isShowingFindIndicator = true;
    
    return true;
}

void FindController::hideFindIndicator()
{
    if (!m_isShowingFindIndicator)
        return;

    m_webPage->corePage()->pageOverlayController().uninstallPageOverlay(*m_findIndicatorOverlay, PageOverlay::FadeMode::DoNotFade);
    m_findIndicatorOverlay = nullptr;
    m_isShowingFindIndicator = false;
    m_foundStringMatchIndex = -1;
    didHideFindIndicator();
}

static void setSelectionChangeUpdatesEnabledInAllFrames(WebPage& page, bool enabled)
{
    for (Frame* coreFrame = page.mainFrame(); coreFrame; coreFrame = coreFrame->tree().traverseNext())
        coreFrame->editor().setIgnoreSelectionChanges(enabled);
}

void FindController::willFindString()
{
    setSelectionChangeUpdatesEnabledInAllFrames(*m_webPage, true);
}

void FindController::didFindString()
{
    // If the selection before we enabled appearance updates is equal to the
    // range that we just found, setSelection will bail and fail to actually call
    // updateAppearance, so the selection won't have been pushed to the render tree.
    // Therefore, we need to force an update no matter what.

    Frame& frame = m_webPage->corePage()->focusController().focusedOrMainFrame();
    frame.selection().setUpdateAppearanceEnabled(true);
    frame.selection().updateAppearance();
    frame.selection().setUpdateAppearanceEnabled(false);

    // Scrolling the main frame is handled by the SmartMagnificationController class but we still
    // need to consider overflow nodes and subframes here.
    // Many sites have overlay headers or footers that may overlap with the highlighted
    // text, so we reveal the text at the center of the viewport.
    // FIXME: Find a better way to estimate the obscured area (https://webkit.org/b/183889).
    frame.selection().revealSelection(SelectionRevealMode::RevealUpToMainFrame, ScrollAlignment::alignCenterAlways, WebCore::DoNotRevealExtent);
}

void FindController::didFailToFindString()
{
    setSelectionChangeUpdatesEnabledInAllFrames(*m_webPage, false);
}

void FindController::didHideFindIndicator()
{
    setSelectionChangeUpdatesEnabledInAllFrames(*m_webPage, false);
}
    
unsigned FindController::findIndicatorRadius() const
{
    return cornerRadius;
}
    
bool FindController::shouldHideFindIndicatorOnScroll() const
{
    return false;
}

} // namespace WebKit

#endif
