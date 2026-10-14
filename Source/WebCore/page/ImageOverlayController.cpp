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
#include "ContainerNodeInlines.h"
#include "Document.h"
#include "DocumentPage.h"
#include "DocumentView.h"
#include "Editor.h"
#include "FloatQuad.h"
#include "FrameDestructionObserverInlines.h"
#include "FrameSelection.h"
#include "GraphicsContext.h"
#include "HTMLElement.h"
#include "ImageOverlay.h"
#include "IntRect.h"
#include "LayoutRect.h"
#include "LocalFrameInlines.h"
#include "LocalFrame.h"
#include "NodeDocument.h"
#include "Page.h"
#include "PageOverlayController.h"
#include "PlatformMouseEvent.h"
#include "RenderElement.h"
#include "SimpleRange.h"
#include "StyleComputedStyle+GettersInlines.h"
#include "VisiblePosition.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(ImageOverlayController);

class FloatQuad;

ImageOverlayController::ImageOverlayController(Page& page)
    : m_page(page)
{
}

void ImageOverlayController::selectionQuadsDidChange(LocalFrame& frame, const Vector<FloatQuad>& quads)
{
    if (!m_page->chrome().client().needsImageOverlayControllerForSelectionPainting())
        return;

    if (frame.editor().ignoreSelectionChanges() || frame.editor().isGettingDictionaryPopupInfo())
        return;

    if (RefPtr overlay { overlayForRootFrame(rootFrameForHost(m_hostElementForSelection).get()) })
        overlay->setNeedsDisplay();
    clearSelectionState();

    auto overlayHost = ([&] () -> RefPtr<HTMLElement> {
        auto selectedRange = frame.selection().selection().range();
        if (!selectedRange)
            return nullptr;

        if (!ImageOverlay::isInsideOverlay(*selectedRange))
            return nullptr;

        return dynamicDowncast<HTMLElement>(selectedRange->startContainer().shadowHost());
    })();

    if (!overlayHost) {
        updateOverlays();
        return;
    }

    CheckedPtr overlayHostRenderer = overlayHost->renderer();
    if (!overlayHostRenderer) {
        updateOverlays();
        return;
    }

    if (!shouldUsePageOverlayToPaintSelection(*overlayHostRenderer)) {
        updateOverlays();
        return;
    }

    m_hostElementForSelection = *overlayHost;
    m_selectionQuads = quads;
    m_selectionBackgroundColor = overlayHostRenderer->selectionBackgroundColor();
    m_selectionClipRect = overlayHostRenderer->absoluteBoundingBoxRect();

    updateOverlays();
    if (RefPtr overlay { overlayForRootFrame(rootFrameForHost(*overlayHost).get()) })
        overlay->setNeedsDisplay();
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
        clearSelectionState();

#if PLATFORM(MAC)
    if (m_hostElementForDataDetectors && &document == &m_hostElementForDataDetectors->document())
        clearDataDetectorHighlights();
#endif

    updateOverlays();
}

RefPtr<LocalFrame> ImageOverlayController::rootFrameForHost(const HTMLElement& host)
{
    // FIXME: https://bugs.webkit.org/show_bug.cgi?id=325777 A frame nested under a remote frame that this process removed
    // stays alive for a while out of the frame tree, and its document is not detached. Treat its images as gone.
    RefPtr frame { host.document().frame() };
    RefPtr page { frame ? frame->page() : nullptr };
    if (!page || !frame->tree().isDescendantOf(&page->mainFrame()))
        return nullptr;
    return &frame->rootFrame();
}

RefPtr<LocalFrame> ImageOverlayController::rootFrameForHost(const WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>& host)
{
    RefPtr protectedHost { host.get() };
    return protectedHost ? rootFrameForHost(*protectedHost) : nullptr;
}

void ImageOverlayController::updateOverlays()
{
    Vector<Ref<LocalFrame>, 2> rootFramesNeedingOverlay;
    auto addRootFrameForHost = [&](const WeakPtr<HTMLElement, WeakPtrImplWithEventTargetData>& host) {
        RefPtr rootFrame { rootFrameForHost(host) };
        if (!rootFrame || !rootFrame->view())
            return;
        if (!rootFramesNeedingOverlay.containsIf([&](auto& frame) { return frame.ptr() == rootFrame; }))
            rootFramesNeedingOverlay.append(rootFrame.releaseNonNull());
    };
    addRootFrameForHost(m_hostElementForSelection);
#if PLATFORM(MAC)
    addRootFrameForHost(m_hostElementForDataDetectors);
#endif

    // Uninstalling re-enters willMoveToPage(), so take the overlays out first.
    Vector<Ref<PageOverlay>> overlaysToUninstall;
    m_overlays.removeAllMatching([&](auto& overlay) {
        if (rootFramesNeedingOverlay.containsIf([&](auto& frame) { return frame.ptr() == overlay->associatedFrame(); }))
            return false;
        overlaysToUninstall.append(overlay.copyRef());
        return true;
    });
    for (auto& overlay : overlaysToUninstall)
        m_page->pageOverlayController().uninstallPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);

    for (auto& rootFrame : rootFramesNeedingOverlay) {
        if (overlayForRootFrame(rootFrame.ptr()))
            continue;
        Ref overlay { PageOverlay::create(*this, PageOverlay::OverlayType::Document) };
        overlay->setAssociatedFrame(rootFrame.ptr());
        m_overlays.append(overlay.copyRef());
        m_page->pageOverlayController().installPageOverlay(overlay, PageOverlay::FadeMode::DoNotFade);
    }

    if (!overlayForRootFrame(rootFrameForHost(m_hostElementForSelection).get()))
        clearSelectionState();
#if PLATFORM(MAC)
    if (!overlayForRootFrame(rootFrameForHost(m_hostElementForDataDetectors).get()))
        clearDataDetectorHighlights();
#endif
}

RefPtr<PageOverlay> ImageOverlayController::overlayForRootFrame(const LocalFrame* rootFrame) const
{
    if (!rootFrame)
        return nullptr;

    for (auto& overlay : m_overlays) {
        if (overlay->associatedFrame() == rootFrame)
            return overlay.ptr();
    }
    return nullptr;
}

void ImageOverlayController::clearSelectionState()
{
    m_hostElementForSelection = nullptr;
    m_selectionQuads.clear();
    m_selectionBackgroundColor = Color::transparentBlack;
    m_selectionClipRect = { };
}

void ImageOverlayController::ref() const
{
    m_page->ref();
}

void ImageOverlayController::deref() const
{
    m_page->deref();
}

void ImageOverlayController::willMoveToPage(PageOverlay& overlay, Page* page)
{
    if (page)
        return;

    m_overlays.removeFirstMatching([&](auto& installedOverlay) {
        return installedOverlay.ptr() == &overlay;
    });
}

void ImageOverlayController::drawRect(PageOverlay& pageOverlay, GraphicsContext& context, const IntRect& dirtyRect)
{
    if (!m_overlays.containsIf([&](auto& overlay) { return overlay.ptr() == &pageOverlay; })) {
        ASSERT_NOT_REACHED();
        return;
    }

    GraphicsContextStateSaver stateSaver(context);
    context.clearRect(dirtyRect);

    RefPtr rootFrame { pageOverlay.associatedFrame() };
    if (m_selectionQuads.isEmpty() || !rootFrame || rootFrame != rootFrameForHost(m_hostElementForSelection))
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

bool ImageOverlayController::platformHandleMouseEvent(PageOverlay&, const PlatformMouseEvent&)
{
    return false;
}

void ImageOverlayController::elementUnderMouseDidChange(LocalFrame&, Element*)
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
