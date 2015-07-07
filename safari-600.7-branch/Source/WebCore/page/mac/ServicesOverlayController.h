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

#if (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)) && PLATFORM(MAC)

#include "GraphicsLayerClient.h"
#include "PageOverlay.h"
#include "Range.h"
#include "Timer.h"
#include <wtf/RefCounted.h>

typedef struct __DDHighlight DDHighlight, *DDHighlightRef;

namespace WebCore {
class LayoutRect;
class MainFrame;
struct GapRects;
}

namespace WebCore {

class ServicesOverlayController : private PageOverlay::Client {
public:
    explicit ServicesOverlayController(MainFrame&);
    ~ServicesOverlayController();

    void selectedTelephoneNumberRangesChanged();
    void selectionRectsDidChange(const Vector<LayoutRect>&, const Vector<GapRects>&, bool isTextOnly);

private:
    class Highlight : public RefCounted<Highlight>, private GraphicsLayerClient {
        WTF_MAKE_NONCOPYABLE(Highlight);
    public:
        static PassRefPtr<Highlight> createForSelection(ServicesOverlayController&, RetainPtr<DDHighlightRef>, PassRefPtr<Range>);
        static PassRefPtr<Highlight> createForTelephoneNumber(ServicesOverlayController&, RetainPtr<DDHighlightRef>, PassRefPtr<Range>);
        ~Highlight();

        void invalidate();

        DDHighlightRef ddHighlight() const { return m_ddHighlight.get(); }
        Range* range() const { return m_range.get(); }
        GraphicsLayer* layer() const { return m_graphicsLayer.get(); }

        enum class Type {
            TelephoneNumber,
            Selection
        };
        Type type() const { return m_type; }

        void fadeIn();
        void fadeOut();

        void setDDHighlight(DDHighlightRef);

    private:
        explicit Highlight(ServicesOverlayController&, Type, RetainPtr<DDHighlightRef>, PassRefPtr<Range>);

        // GraphicsLayerClient
        virtual void notifyFlushRequired(const GraphicsLayer*) override;
        virtual void paintContents(const GraphicsLayer*, GraphicsContext&, GraphicsLayerPaintingPhase, const FloatRect& inClip) override;
        virtual float deviceScaleFactor() const override;

        void didFinishFadeOutAnimation();

        RetainPtr<DDHighlightRef> m_ddHighlight;
        RefPtr<Range> m_range;
        std::unique_ptr<GraphicsLayer> m_graphicsLayer;
        Type m_type;
        ServicesOverlayController* m_controller;
    };

    // PageOverlay::Client
    virtual void pageOverlayDestroyed(PageOverlay&) override;
    virtual void willMoveToPage(PageOverlay&, Page*) override;
    virtual void didMoveToPage(PageOverlay&, Page*) override;
    virtual void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) override;
    virtual bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) override;
    virtual void didScrollFrame(PageOverlay&, Frame&) override;

    void createOverlayIfNeeded();
    void handleClick(const IntPoint&, Highlight&);

    void drawHighlight(Highlight&, GraphicsContext&);

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
    void determineActiveHighlightTimerFired(Timer&);

    static bool highlightsAreEquivalent(const Highlight* a, const Highlight* b);

    Vector<RefPtr<Range>> telephoneNumberRangesForFocusedFrame();

    void didCreateHighlight(Highlight*);
    void willDestroyHighlight(Highlight*);
    void didFinishFadingOutHighlight(Highlight*);

    MainFrame& mainFrame() const { return m_mainFrame; }

    MainFrame& m_mainFrame;
    PageOverlay* m_servicesOverlay;

    RefPtr<Highlight> m_activeHighlight;
    RefPtr<Highlight> m_nextActiveHighlight;
    HashSet<RefPtr<Highlight>> m_potentialHighlights;
    HashSet<RefPtr<Highlight>> m_animatingHighlights;

    HashSet<Highlight*> m_highlights;

    // FIXME: These should move onto Highlight.
    Vector<LayoutRect> m_currentSelectionRects;
    bool m_isTextOnly;

    std::chrono::steady_clock::time_point m_lastSelectionChangeTime;
    std::chrono::steady_clock::time_point m_nextActiveHighlightChangeTime;
    std::chrono::steady_clock::time_point m_lastMouseUpTime;

    RefPtr<Highlight> m_currentMouseDownOnButtonHighlight;
    IntPoint m_mousePosition;

    Timer m_determineActiveHighlightTimer;
};

} // namespace WebKit

#endif // (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)) && PLATFORM(MAC)
#endif // ServicesOverlayController_h
