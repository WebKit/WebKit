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
#include <WebCore/Timer.h>
#include <wtf/RefCounted.h>

typedef void* DDHighlightRef;

namespace WebCore {
class LayoutRect;

struct GapRects;
}

namespace WebKit {

class WebPage;

class ServicesOverlayController : private PageOverlay::Client {
public:
    ServicesOverlayController(WebPage&);
    ~ServicesOverlayController();

    void selectedTelephoneNumberRangesChanged(const Vector<RefPtr<WebCore::Range>>&);
    void selectionRectsDidChange(const Vector<WebCore::LayoutRect>&, const Vector<WebCore::GapRects>&, bool isTextOnly);

private:
    class Highlight : public RefCounted<Highlight> {
        WTF_MAKE_NONCOPYABLE(Highlight);
    public:
        static PassRefPtr<Highlight> createForSelection(RetainPtr<DDHighlightRef>);
        static PassRefPtr<Highlight> createForTelephoneNumber(RetainPtr<DDHighlightRef>, PassRefPtr<WebCore::Range>);

        DDHighlightRef ddHighlight() const { return m_ddHighlight.get(); }
        WebCore::Range* range() const { return m_range.get(); }

        enum class Type {
            TelephoneNumber,
            Selection
        };
        Type type() const { return m_type; }

    private:
        explicit Highlight(Type type, RetainPtr<DDHighlightRef> ddHighlight, PassRefPtr<WebCore::Range> range)
            : m_ddHighlight(ddHighlight)
            , m_range(range)
            , m_type(type)
        {
            ASSERT(m_ddHighlight);
            ASSERT(type != Type::TelephoneNumber || m_range);
        }

        RetainPtr<DDHighlightRef> m_ddHighlight;
        RefPtr<WebCore::Range> m_range;
        Type m_type;
    };

    // PageOverlay::Client
    virtual void pageOverlayDestroyed(PageOverlay*) override;
    virtual void willMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void didMoveToWebPage(PageOverlay*, WebPage*) override;
    virtual void drawRect(PageOverlay*, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    virtual bool mouseEvent(PageOverlay*, const WebMouseEvent&) override;

    void createOverlayIfNeeded();
    void handleClick(const WebCore::IntPoint&, Highlight&);

    void drawHighlight(Highlight&, WebCore::GraphicsContext&);

    void removeAllPotentialHighlightsOfType(Highlight::Type);
    void buildPhoneNumberHighlights();
    void buildSelectionHighlight();
    void didRebuildPotentialHighlights();

    void determineActiveHighlight(bool& mouseIsOverButton);
    void clearActiveHighlight();

    bool hasRelevantSelectionServices();

    bool mouseIsOverHighlight(Highlight&, bool& mouseIsOverButton) const;
    std::chrono::milliseconds remainingTimeUntilHighlightShouldBeShown() const;
    void repaintHighlightTimerFired(WebCore::Timer<ServicesOverlayController>&);

    static bool highlightsAreEquivalent(const Highlight* a, const Highlight* b);

    WebPage& m_webPage;
    PageOverlay* m_servicesOverlay;

    RefPtr<Highlight> m_activeHighlight;
    HashSet<RefPtr<Highlight>> m_potentialHighlights;

    Vector<RefPtr<WebCore::Range>> m_currentTelephoneNumberRanges;
    Vector<WebCore::LayoutRect> m_currentSelectionRects;
    bool m_isTextOnly;

    std::chrono::steady_clock::time_point m_lastSelectionChangeTime;
    std::chrono::steady_clock::time_point m_lastActiveHighlightChangeTime;

    RefPtr<Highlight> m_currentMouseDownOnButtonHighlight;
    WebCore::IntPoint m_mousePosition;

    WebCore::Timer<ServicesOverlayController> m_repaintHighlightTimer;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS) && ENABLE(TELEPHONE_NUMBER_DETECTION)
#endif // ServicesOverlayController_h
