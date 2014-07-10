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
#define ServicesOverlayController_h

#if ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)

#include "PageOverlay.h"
#include <WebCore/Range.h>

typedef void* DDHighlightRef;

namespace WebCore {
class LayoutRect;

struct GapRects;
}

namespace WebKit {

class WebPage;

typedef void* DDHighlightRef;

struct TelephoneNumberData {
    TelephoneNumberData(RetainPtr<DDHighlightRef> highlight, PassRefPtr<WebCore::Range> range)
        : highlight(highlight)
        , range(range)
    {
    }

    RetainPtr<DDHighlightRef> highlight;
    RefPtr<WebCore::Range> range;
};

class ServicesOverlayController : private PageOverlay::Client {
public:
    ServicesOverlayController(WebPage&);
    ~ServicesOverlayController();

    void selectedTelephoneNumberRangesChanged(const Vector<RefPtr<WebCore::Range>>&);
    void selectionRectsDidChange(const Vector<WebCore::LayoutRect>&, const Vector<WebCore::GapRects>&);

private:
    void createOverlayIfNeeded();
    void handleClick(const WebCore::IntPoint&, DDHighlightRef);
    void clearHighlightState();

    virtual void pageOverlayDestroyed(PageOverlay*) override;
    virtual void willMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void didMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    virtual bool mouseEvent(PageOverlay*, const WebMouseEvent&) override;

    bool drawTelephoneNumberHighlightIfVisible(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect);
    void drawSelectionHighlight(WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect);
    void drawHighlight(DDHighlightRef, WebCore::GraphicsContext&);

    void establishHoveredTelephoneHighlight(Boolean& onButton);
    void maybeCreateSelectionHighlight();

    void clearSelectionHighlight();
    void clearHoveredTelephoneNumberHighlight();

    WebPage* m_webPage;
    PageOverlay* m_servicesOverlay;
    
    Vector<WebCore::LayoutRect> m_currentSelectionRects;
    RetainPtr<DDHighlightRef> m_selectionHighlight;

    Vector<RefPtr<WebCore::Range>> m_currentTelephoneNumberRanges;
    Vector<RetainPtr<DDHighlightRef>> m_telephoneNumberHighlights;
    std::unique_ptr<TelephoneNumberData> m_hoveredTelephoneNumberData;

    RetainPtr<DDHighlightRef> m_currentHoveredHighlight;
    RetainPtr<DDHighlightRef> m_currentMouseDownOnButtonHighlight;

    WebCore::IntPoint m_mousePosition;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS) && ENABLE(TELEPHONE_NUMBER_DETECTION)
#endif // ServicesOverlayController_h
