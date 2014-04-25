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

#include "config.h"
#include "SelectionOverlayController.h"

#if ENABLE(SERVICE_CONTROLS)

#include "WebPage.h"
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {
    
SelectionOverlayController::SelectionOverlayController(WebPage* webPage)
    : m_webPage(webPage)
    , m_selectionOverlay(nullptr)
    , m_mouseIsDownOnButton(false)
    , m_mouseIsOverHighlight(false)
{
}

void SelectionOverlayController::createOverlayIfNeeded()
{
    if (m_selectionOverlay) {
        m_selectionOverlay->setNeedsDisplay();
        return;
    }
    
    RefPtr<PageOverlay> overlay = PageOverlay::create(this, PageOverlay::OverlayType::Document);
    m_selectionOverlay = overlay.get();
    m_webPage->installPageOverlay(overlay.release(), PageOverlay::FadeMode::Fade);
    m_selectionOverlay->setNeedsDisplay();
}

void SelectionOverlayController::destroyOverlay()
{
    if (!m_selectionOverlay)
        return;

    m_webPage->uninstallPageOverlay(m_selectionOverlay, PageOverlay::FadeMode::DoNotFade);
}

void SelectionOverlayController::pageOverlayDestroyed(PageOverlay*)
{
    // Before the overlay is destroyed, it should have moved out of the WebPage,
    // at which point we already cleared our back pointer.
    ASSERT(!m_selectionOverlay);
}

void SelectionOverlayController::willMoveToWebPage(PageOverlay*, WebPage* webPage)
{
    if (webPage)
        return;

    ASSERT(m_selectionOverlay);
    m_selectionOverlay = nullptr;
}

void SelectionOverlayController::didMoveToWebPage(PageOverlay*, WebPage*)
{
}

void SelectionOverlayController::selectionRectsDidChange(const Vector<LayoutRect>& rects)
{
#if __MAC_OS_X_VERSION_MIN_REQUIRED > 1090
    clearHighlight();

    m_currentSelectionRects = rects;

    if (m_currentSelectionRects.isEmpty())
        destroyOverlay();
    else
        createOverlayIfNeeded();
#else
    UNUSED_PARAM(rects);
#endif
}

#if !PLATFORM(MAC)
void SelectionOverlayController::drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect&)
{
    notImplemented();
}

bool SelectionOverlayController::mouseEvent(PageOverlay*, const WebMouseEvent&)
{
    notImplemented();
    return false;
}
#endif

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS)
