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

#ifndef ServicesOverlayController_h

#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)

#include "PageOverlay.h"

typedef void* DDHighlightRef;

namespace WebCore {
class LayoutRect;
class Range;
}

namespace WebKit {

class WebPage;

typedef void* DDHighlightRef;

class ServicesOverlayController : private PageOverlay::Client {
public:
    ServicesOverlayController(WebPage&);
    ~ServicesOverlayController();

    void selectedTelephoneNumberRangesChanged(const Vector<RefPtr<WebCore::Range>>&);
    void selectionRectsDidChange(const Vector<WebCore::LayoutRect>&);

private:
    void createOverlayIfNeeded();
    void handleClick(const WebCore::IntPoint&);
    void clearHighlightState();
    
    virtual void pageOverlayDestroyed(PageOverlay*) override;
    virtual void willMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void didMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    virtual bool mouseEvent(PageOverlay*, const WebMouseEvent&) override;

    void drawTelephoneNumberHighlight(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect);
    void drawSelectionHighlight(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect);
    void drawCurrentHighlight(WebCore::GraphicsContext&);

    WebPage* m_webPage;
    PageOverlay* m_servicesOverlay;
    
    Vector<WebCore::LayoutRect> m_currentSelectionRects;
    Vector<RefPtr<WebCore::Range>> m_currentTelephoneNumberRanges;

    WebCore::IntPoint m_mousePosition;
    bool m_mouseIsDownOnButton;
    bool m_mouseIsOverHighlight;
    bool m_drawingTelephoneNumberHighlight;

    RetainPtr<DDHighlightRef> m_currentHighlight;
    bool m_currentHighlightIsDirty;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS) && ENABLE(TELEPHONE_NUMBER_DETECTION)
#endif // ServicesOverlayController_h
