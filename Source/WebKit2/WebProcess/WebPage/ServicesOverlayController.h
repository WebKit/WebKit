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

#include "WebFrame.h"
#include <WebCore/GraphicsLayerClient.h>
#include <WebCore/PageOverlay.h>
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

class ServicesOverlayController : private WebCore::PageOverlay::Client {
public:
    ServicesOverlayController(WebPage&);
    ~ServicesOverlayController();

    void selectedTelephoneNumberRangesChanged();
    void selectionRectsDidChange(const Vector<WebCore::LayoutRect>&, const Vector<WebCore::GapRects>&, bool isTextOnly);

private:
    class Highlight : public RefCounted<Highlight>, private WebCore::GraphicsLayerClient {
        WTF_MAKE_NONCOPYABLE(Highlight);
    public:
        static PassRefPtr<Highlight> createForSelection(ServicesOverlayController&, RetainPtr<DDHighlightRef>, PassRefPtr<WebCore::Range>);
        static PassRefPtr<Highlight> createForTelephoneNumber(ServicesOverlayController&, RetainPtr<DDHighlightRef>, PassRefPtr<WebCore::Range>);
        ~Highlight();

        void invalidate();

        DDHighlightRef ddHighlight() const { return m_ddHighlight.get(); }
        WebCore::Range* range() const { return m_range.get(); }
        WebCore::GraphicsLayer* layer() const { return m_graphicsLayer.get(); }

        enum class Type {
            TelephoneNumber,
            Selection
        };
        Type type() const { return m_type; }

        void fadeIn();
        void fadeOut();

        void setDDHighlight(DDHighlightRef);

    private:
        explicit Highlight(ServicesOverlayController&, Type, RetainPtr<DDHighlightRef>, PassRefPtr<WebCore::Range>);

        // GraphicsLayerClient
        virtual void notifyAnimationStarted(const WebCore::GraphicsLayer*, double time) override { }
        virtual void notifyFlushRequired(const WebCore::GraphicsLayer*) override;
        virtual void paintContents(const WebCore::GraphicsLayer*, WebCore::GraphicsContext&, WebCore::GraphicsLayerPaintingPhase, const WebCore::FloatRect& inClip) override;
        virtual float deviceScaleFactor() const override;

        void didFinishFadeOutAnimation();

        RetainPtr<DDHighlightRef> m_ddHighlight;
        RefPtr<WebCore::Range> m_range;
        std::unique_ptr<WebCore::GraphicsLayer> m_graphicsLayer;
        Type m_type;
        ServicesOverlayController* m_controller;
    };

    // PageOverlay::Client
    virtual void pageOverlayDestroyed(WebCore::PageOverlay&) override;
    virtual void willMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    virtual void didMoveToPage(WebCore::PageOverlay&, WebCore::Page*) override;
    virtual void drawRect(WebCore::PageOverlay&, WebCore::GraphicsContext&, const WebCore::IntRect& dirtyRect) override;
    virtual bool mouseEvent(WebCore::PageOverlay&, const WebCore::PlatformMouseEvent&) override;
    virtual void didScrollFrame(WebCore::PageOverlay&, WebCore::Frame&) override;

    void createOverlayIfNeeded();
    void handleClick(const WebCore::IntPoint&, Highlight&);

    void drawHighlight(Highlight&, WebCore::GraphicsContext&);

    void replaceHighlightsOfTypePreservingEquivalentHighlights(HashSet<RefPtr<Highlight>>&, Highlight::Type);
    void removeAllPotentialHighlightsOfType(Highlight::Type);
    void buildPhoneNumberHighlights();
    void buildSelectionHighlight();
    void didRebuildPotentialHighlights();

    void determineActiveHighlight(bool& mouseIsOverButton);
    void clearActiveHighlight();
    Highlight* activeHighlight() const { return m_activeHighlight.get(); }

    Highlight* findTelephoneNumberHighlightContainingSelectionHighlight(Highlight&);

    bool hasRelevantSelectionServices();

    bool mouseIsOverHighlight(Highlight&, bool& mouseIsOverButton) const;
    std::chrono::milliseconds remainingTimeUntilHighlightShouldBeShown(Highlight*) const;
    void determineActiveHighlightTimerFired(WebCore::Timer<ServicesOverlayController>&);

    static bool highlightsAreEquivalent(const Highlight* a, const Highlight* b);

    Vector<RefPtr<WebCore::Range>> telephoneNumberRangesForFocusedFrame();

    void didCreateHighlight(Highlight*);
    void willDestroyHighlight(Highlight*);
    void didFinishFadingOutHighlight(Highlight*);

    WebPage& webPage() const { return m_webPage; }

    WebPage& m_webPage;
    WebCore::PageOverlay* m_servicesOverlay;

    RefPtr<Highlight> m_activeHighlight;
    RefPtr<Highlight> m_nextActiveHighlight;
    HashSet<RefPtr<Highlight>> m_potentialHighlights;
    HashSet<RefPtr<Highlight>> m_animatingHighlights;

    HashSet<Highlight*> m_highlights;

    // FIXME: These should move onto Highlight.
    Vector<WebCore::LayoutRect> m_currentSelectionRects;
    bool m_isTextOnly;

    std::chrono::steady_clock::time_point m_lastSelectionChangeTime;
    std::chrono::steady_clock::time_point m_nextActiveHighlightChangeTime;
    std::chrono::steady_clock::time_point m_lastMouseUpTime;

    RefPtr<Highlight> m_currentMouseDownOnButtonHighlight;
    WebCore::IntPoint m_mousePosition;

    WebCore::Timer<ServicesOverlayController> m_determineActiveHighlightTimer;
};

} // namespace WebKit

#endif // ENABLE(SERVICE_CONTROLS) && ENABLE(TELEPHONE_NUMBER_DETECTION)
#endif // ServicesOverlayController_h
