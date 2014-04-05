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

#ifndef TelephoneNumberOverlayController_h

#if ENABLE(TELEPHONE_NUMBER_DETECTION)

#include "PageOverlay.h"
#include <WebCore/IntRect.h>
#include <wtf/RefCounted.h>

namespace WebCore {
class Range;
}

namespace WebKit {
    
class WebPage;
    
#if PLATFORM(MAC)
typedef void* DDHighlightRef;
#endif

class TelephoneNumberOverlayController : public RefCounted<TelephoneNumberOverlayController>, private PageOverlay::Client {
public:

    static PassRefPtr<TelephoneNumberOverlayController> create(WebPage* webPage)
    {
        return adoptRef(new TelephoneNumberOverlayController(webPage));
    }

    void selectedTelephoneNumberRangesChanged(const Vector<RefPtr<WebCore::Range>>&);
    
private:
    TelephoneNumberOverlayController(WebPage*);

    void createOverlayIfNeeded();
    void destroyOverlay();
    
    void clearHighlights();
    void clearMouseDownInformation();
    
    void handleTelephoneClick();
    
    Vector<WebCore::IntRect> rectsForDrawing() const;

    virtual void pageOverlayDestroyed(PageOverlay*) override;
    virtual void willMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void didMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    virtual bool mouseEvent(PageOverlay*, const WebMouseEvent&) override;

    RefPtr<WebPage> m_webPage;
    PageOverlay* m_telephoneNumberOverlay;
    Vector<RefPtr<WebCore::Range>> m_currentSelectionRanges;
    
    
#if PLATFORM(MAC)
    Vector<RetainPtr<DDHighlightRef>> m_highlights;
    RetainPtr<DDHighlightRef> m_currentMouseDownHighlight;
#endif
    
    WebCore::IntPoint m_mouseDownPosition;
};

} // namespace WebKit

#endif // ENABLE(TELEPHONE_NUMBER_DETECTION)
#endif // TelephoneNumberOverlayController_h
