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

#ifndef SelectionOverlayController_h

#if ENABLE(SERVICE_CONTROLS)

#include "PageOverlay.h"
#include "WebPage.h"
#include <wtf/RefCounted.h>
#include <wtf/RunLoop.h>

typedef void* DDHighlightRef;

namespace WebCore {
class LayoutRect;
}

namespace WebKit {
    
class WebPage;

class SelectionOverlayController : public RefCounted<SelectionOverlayController>, private PageOverlay::Client {
public:

    static PassRefPtr<SelectionOverlayController> create(WebPage* webPage)
    {
        return adoptRef(new SelectionOverlayController(webPage));
    }

    void selectionRectsDidChange(const Vector<WebCore::LayoutRect>&);
    void handleClick(const WebCore::IntPoint&);
    
private:
    SelectionOverlayController(WebPage*);

    void createOverlayIfNeeded();
    void destroyOverlay();
    
    void handleSelectionOverlayClick(const WebCore::IntPoint&);

    virtual void pageOverlayDestroyed(PageOverlay*) override;
    virtual void willMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void didMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    virtual bool mouseEvent(PageOverlay*, const WebMouseEvent&) override;

    void mouseHoverStateChanged();
    void hoverTimerFired();

    RefPtr<WebPage> m_webPage;
    PageOverlay* m_selectionOverlay;
    Vector<WebCore::LayoutRect> m_currentSelectionRects;

    WebCore::IntPoint m_mousePosition;
    bool m_mouseIsDownOnButton;
    bool m_mouseIsOverHighlight;
    bool m_visible;

    RunLoop::Timer<SelectionOverlayController> m_hoverTimer;

    RetainPtr<DDHighlightRef> m_currentHighlight;
    bool m_currentHighlightIsDirty;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS)
#endif // SelectionOverlayController_h
