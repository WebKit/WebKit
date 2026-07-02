/*
 * Copyright (C) 2014-2026 Apple Inc. All rights reserved.
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
#include "FindIndicator.h"

#if PLATFORM(IOS_FAMILY)

#include "MessageSenderInlines.h"
#include "SmartMagnificationControllerMessages.h"
#include "WebPage.h"
#include "WebPageProxyMessages.h"
#include <WebCore/DocumentPage.h>
#include <WebCore/Editor.h>
#include <WebCore/FindRevealAlgorithms.h>
#include <WebCore/FocusController.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/ImageOverlay.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/Page.h>
#include <WebCore/PageOverlayController.h>
#include <WebCore/PathUtilities.h>
#include <WebCore/Settings.h>
#include <WebCore/TextIndicator.h>

namespace WebKit {
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FindIndicatorIOS);

constexpr int totalHorizontalMargin = 1;
constexpr int totalVerticalMargin = 1;
constexpr unsigned findIndicatorRadius = 3;

static OptionSet<TextIndicatorOption> findTextIndicatorOptions(const LocalFrame& frame)
{
    OptionSet<TextIndicatorOption> options { TextIndicatorOption::IncludeMarginIfRangeMatchesSelection, TextIndicatorOption::DoNotClipToVisibleRect };
    if (auto selectedRange = frame.selection().selection().range(); selectedRange && ImageOverlay::isInsideOverlay(*selectedRange))
        options.add({ TextIndicatorOption::PaintAllContent, TextIndicatorOption::PaintBackgrounds });
    return options;
};

static constexpr auto highlightColor = SRGBA<uint8_t> { 255, 228, 56 };
void FindIndicatorOverlayClientIOS::drawRect(PageOverlay& overlay, GraphicsContext& context, const IntRect& dirtyRect)
{
    Ref frame = m_frame.get();
    float scaleFactor = frame->page()->deviceScaleFactor();

    if (frame->page()->delegatesScaling())
        scaleFactor *= frame->page()->pageScaleFactor();

    // If the page scale changed, we need to paint a new TextIndicator.
    if (m_textIndicator->contentImageScaleFactor() != scaleFactor)
        m_textIndicator = TextIndicator::createWithSelectionInFrame(frame, findTextIndicatorOptions(frame), TextIndicatorPresentationTransition::None, FloatSize(totalHorizontalMargin, totalVerticalMargin));

    if (!m_textIndicator)
        return;

    RefPtr indicatorImage = m_textIndicator->contentImage();
    if (!indicatorImage)
        return;

    Vector<FloatRect> textRectsInBoundingRectCoordinates = m_textIndicator->textRectsInBoundingRectCoordinates();
    Vector<Path> paths = PathUtilities::pathsWithShrinkWrappedRects(textRectsInBoundingRectCoordinates, findIndicatorRadius);

    context.setFillColor(highlightColor);
    for (const auto& path : paths)
        context.fillPath(path);

    context.drawImage(*indicatorImage, overlay.bounds());
}

bool FindIndicatorIOS::update(WebCore::LocalFrame* selectedFrame, bool isShowingOverlay, bool shouldAnimate)
{
    if (!selectedFrame)
        return false;

    if (m_findIndicatorOverlay) {
        protect(*m_webPage)->corePage()->pageOverlayController().uninstallPageOverlay(*protect(m_findIndicatorOverlay), PageOverlay::FadeMode::DoNotFade);
        m_findIndicatorOverlay = nullptr;
        m_isShowingFindIndicator = false;
    }

    auto textIndicator = TextIndicator::createWithSelectionInFrame(*selectedFrame, findTextIndicatorOptions(*selectedFrame), TextIndicatorPresentationTransition::None, FloatSize(totalHorizontalMargin, totalVerticalMargin));
    if (!textIndicator)
        return false;

    m_findIndicatorOverlayClient = makeUnique<FindIndicatorOverlayClientIOS>(*selectedFrame, textIndicator.get());
    m_findIndicatorRect = enclosingIntRect(textIndicator->selectionRectInRootViewCoordinates());
    m_findIndicatorOverlay = PageOverlay::create(*m_findIndicatorOverlayClient, PageOverlay::OverlayType::Document);
    Ref findIndicatorOverlay = *m_findIndicatorOverlay;
    protect(*m_webPage)->corePage()->pageOverlayController().installPageOverlay(findIndicatorOverlay, PageOverlay::FadeMode::DoNotFade);

    findIndicatorOverlay->setFrame(enclosingIntRect(textIndicator->textBoundingRectInRootViewCoordinates()));
    findIndicatorOverlay->setNeedsDisplay();

    if (shouldAnimate) {
        bool isReplaced;
        const VisibleSelection& visibleSelection = protect(selectedFrame)->selection().selection();
        FloatRect renderRect = protect(visibleSelection.start().containerNode())->absoluteBoundingRect(&isReplaced);
        IntRect startRect = visibleSelection.visibleStart().absoluteCaretBounds();

        m_webPage->send(Messages::SmartMagnificationController::ScrollToRect(startRect.center(), renderRect));
    }

    m_isShowingFindIndicator = true;

    return true;
}

static void setSelectionChangeUpdatesEnabledInAllFrames(WebPage& page, bool enabled)
{
    for (RefPtr coreFrame = page.mainFrame(); coreFrame; coreFrame = coreFrame->tree().traverseNext()) {
        RefPtr localFrame = dynamicDowncast<LocalFrame>(coreFrame.get());
        if (!localFrame)
            continue;
        protect(protect(localFrame)->editor())->setIgnoreSelectionChanges(enabled);
    }
}

void FindIndicatorIOS::hide()
{
    if (!m_isShowingFindIndicator)
        return;

    RefPtr webPage = { m_webPage.get() };
    protect(webPage->corePage())->pageOverlayController().uninstallPageOverlay(*protect(m_findIndicatorOverlay), PageOverlay::FadeMode::DoNotFade);
    m_findIndicatorOverlay = nullptr;
    m_isShowingFindIndicator = false;
    setSelectionChangeUpdatesEnabledInAllFrames(*webPage, false);
}

void FindIndicatorIOS::didFindString(WebCore::LocalFrame* selectedFrame)
{
    // If the selection before we enabled appearance updates is equal to the
    // range that we just found, setSelection will bail and fail to actually call
    // updateAppearance, so the selection won't have been pushed to the render tree.
    // Therefore, we need to force an update no matter what.
    if (!selectedFrame)
        return;

    CheckedRef selection = selectedFrame->selection();
    selection->setUpdateAppearanceEnabled(true);
    selection->updateAppearance();
    selection->setUpdateAppearanceEnabled(false);

    // Scrolling the main frame is handled by the SmartMagnificationController class but we still
    // need to consider overflow nodes and subframes here.
    // Many sites have overlay headers or footers that may overlap with the highlighted
    // text, so we reveal the text at the center of the viewport.
    // FIXME: Find a better way to estimate the obscured area (https://webkit.org/b/183889).
    selection->revealSelection({ SelectionRevealMode::RevealUpToMainFrame, ScrollAlignment::alignCenterAlways, WebCore::RevealExtentOption::DoNotRevealExtent });
    if (RefPtr anchorNode = selection->selection().start().anchorNode())
        revealClosedDetailsAndHiddenUntilFoundAncestors(*anchorNode);
}

} // namespace WebKit

#endif
