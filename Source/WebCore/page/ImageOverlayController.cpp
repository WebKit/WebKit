/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "ImageOverlayController.h"

#include "Chrome.h"
#include "ChromeClient.h"
#include "Document.h"
#include "Editor.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "ImageOverlay.h"
#include "IntRect.h"
#include "LayoutRect.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "PlatformMouseEvent.h"
#include "RenderElement.h"
#include "RenderStyle.h"
#include "SimpleRange.h"
#include "VisiblePosition.h"

namespace WebCore {

class FloatQuad;

ImageOverlayController::ImageOverlayController(Page& page)
    : m_page(page)
{
}

void ImageOverlayController::selectionQuadsDidChange(Frame& frame, const Vector<FloatQuad>& quads)
{
    if (!m_page || !m_page->chrome().client().needsImageOverlayControllerForSelectionPainting())
        return;

    if (frame.editor().ignoreSelectionChanges() || frame.editor().isGettingDictionaryPopupInfo())
        return;

    m_hostElementForSelection = nullptr;
    m_selectionQuads.clear();
    m_selectionBackgroundColor = Color::transparentBlack;
    m_selectionClipRect = { };

    auto overlayHost = ([&] () -> RefPtr<HTMLElement> {
        auto selectedRange = frame.selection().selection().range();
        if (!selectedRange)
            return nullptr;

        if (!ImageOverlay::isInsideOverlay(*selectedRange))
            return nullptr;

        if (RefPtr host = selectedRange->startContainer().shadowHost(); is<HTMLElement>(host))
            return static_pointer_cast<HTMLElement>(WTFMove(host));

        return nullptr;
    })();

    if (!overlayHost) {
        uninstallPageOverlayIfNeeded();
        return;
    }

    auto overlayHostRenderer = overlayHost->renderer();
    if (!overlayHostRenderer) {
        uninstallPageOverlayIfNeeded();
        return;
    }

    if (!shouldUsePageOverlayToPaintSelection(*overlayHostRenderer)) {
        uninstallPageOverlayIfNeeded();
        return;
    }

    m_hostElementForSelection = *overlayHost;
    m_selectionQuads = quads;
    m_selectionBackgroundColor = overlayHostRenderer->selectionBackgroundColor();
    m_selectionClipRect = overlayHostRenderer->absoluteBoundingBoxRect();

    installPageOverlayIfNeeded().setNeedsDisplay();
}

bool ImageOverlayController::shouldUsePageOverlayToPaintSelection(const RenderElement& renderer)
{
    // If the selection is already painted (with nonzero opacity) in the overlay host's renderer,
    // then we don't need to fall back to a page overlay to paint the selection.
    return renderer.style().opacity() <= 0.01;
}

void ImageOverlayController::documentDetached(const Document& document)
{
    if (m_hostElementForSelection && &document == &m_hostElementForSelection->document())
        m_hostElementForSelection = nullptr;

#if PLATFORM(MAC)
    if (m_hostElementForDataDetectors && &document == &m_hostElementForDataDetectors->document())
        m_hostElementForDataDetectors = nullptr;
#endif

    uninstallPageOverlayIfNeeded();
}

PageOverlay& ImageOverlayController::installPageOverlayIfNeeded()
{
    if (m_overlay)
        return *m_overlay;

    m_overlay = PageOverlay::create(*this, PageOverlay::OverlayType::Document);
    m_page->pageOverlayController().installPageOverlay(*m_overlay, PageOverlay::FadeMode::DoNotFade);
    return *m_overlay;
}

void ImageOverlayController::uninstallPageOverlay()
{
    m_hostElementForSelection = nullptr;
    m_selectionQuads.clear();
    m_selectionBackgroundColor = Color::transparentBlack;
    m_selectionClipRect = { };

#if PLATFORM(MAC)
    clearDataDetectorHighlights();
#endif

    auto overlayToUninstall = std::exchange(m_overlay, nullptr);
    if (!m_page || !overlayToUninstall)
        return;

    m_page->pageOverlayController().uninstallPageOverlay(*overlayToUninstall, PageOverlay::FadeMode::DoNotFade);
}

void ImageOverlayController::uninstallPageOverlayIfNeeded()
{
    if (m_hostElementForSelection)
        return;

#if PLATFORM(MAC)
    if (m_hostElementForDataDetectors)
        return;
#endif

    uninstallPageOverlay();
}

void ImageOverlayController::willMoveToPage(PageOverlay&, Page* page)
{
    if (!page)
        uninstallPageOverlay();
}

void ImageOverlayController::drawRect(PageOverlay& pageOverlay, GraphicsContext& context, const IntRect& dirtyRect)
{
    if (&pageOverlay != m_overlay) {
        ASSERT_NOT_REACHED();
        return;
    }

    GraphicsContextStateSaver stateSaver(context);
    context.clearRect(dirtyRect);

    if (m_selectionQuads.isEmpty())
        return;

    Path coalescedSelectionPath;
    for (auto& quad : m_selectionQuads) {
        coalescedSelectionPath.moveTo(quad.p1());
        coalescedSelectionPath.addLineTo(quad.p2());
        coalescedSelectionPath.addLineTo(quad.p3());
        coalescedSelectionPath.addLineTo(quad.p4());
        coalescedSelectionPath.addLineTo(quad.p1());
        coalescedSelectionPath.closeSubpath();
    }

    context.setFillColor(m_selectionBackgroundColor);
    context.clip(m_selectionClipRect);
    context.fillPath(coalescedSelectionPath);
}

#if !PLATFORM(MAC)

bool ImageOverlayController::platformHandleMouseEvent(const PlatformMouseEvent&)
{
    return false;
}

void ImageOverlayController::elementUnderMouseDidChange(Frame&, Element*)
{
}

#if ENABLE(DATA_DETECTION)

void ImageOverlayController::textRecognitionResultsChanged(HTMLElement&)
{
}

bool ImageOverlayController::hasActiveDataDetectorHighlightForTesting() const
{
    return false;
}

#endif // ENABLE(DATA_DETECTION)

#endif // !PLATFORM(MAC)

} // namespace WebCore
