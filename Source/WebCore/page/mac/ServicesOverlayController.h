/*
 * Copyright (C) 2014-2017 Apple Inc. All rights reserved.
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

#pragma once

#if (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)) && PLATFORM(MAC)

#include "GraphicsLayerClient.h"
#include "PageOverlay.h"
#include "Range.h"
#include "Timer.h"
#include <wtf/MonotonicTime.h>

typedef struct __DDHighlight *DDHighlightRef;

namespace WebCore {
    
class LayoutRect;
class Page;

struct GapRects;

class ServicesOverlayController : private PageOverlay::Client {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit ServicesOverlayController(Page&);
    ~ServicesOverlayController();

    void selectedTelephoneNumberRangesChanged();
    void selectionRectsDidChange(const Vector<LayoutRect>&, const Vector<GapRects>&, bool isTextOnly);

private:
    class Highlight : public RefCounted<Highlight>, private GraphicsLayerClient {
        WTF_MAKE_NONCOPYABLE(Highlight);
    public:
        static Ref<Highlight> createForSelection(ServicesOverlayController&, RetainPtr<DDHighlightRef>, Ref<Range>&&);
        static Ref<Highlight> createForTelephoneNumber(ServicesOverlayController&, RetainPtr<DDHighlightRef>, Ref<Range>&&);
        ~Highlight();

        void invalidate();

        DDHighlightRef ddHighlight() const { return m_ddHighlight.get(); }
        Range& range() const { return m_range.get(); }
        GraphicsLayer& layer() const { return m_graphicsLayer.get(); }

        enum {
            TelephoneNumberType = 1 << 0,
            SelectionType = 1 << 1,
        };
        typedef uint8_t Type;
        Type type() const { return m_type; }

        void fadeIn();
        void fadeOut();

        void setDDHighlight(DDHighlightRef);

    private:
        Highlight(ServicesOverlayController&, Type, RetainPtr<DDHighlightRef>, Ref<Range>&&);

        // GraphicsLayerClient
        void notifyFlushRequired(const GraphicsLayer*) override;
        void paintContents(const GraphicsLayer*, GraphicsContext&, OptionSet<GraphicsLayerPaintingPhase>, const FloatRect& inClip, GraphicsLayerPaintBehavior) override;
        float deviceScaleFactor() const override;

        void didFinishFadeOutAnimation();

        ServicesOverlayController* m_controller;
        RetainPtr<DDHighlightRef> m_ddHighlight;
        Ref<Range> m_range;
        Ref<GraphicsLayer> m_graphicsLayer;
        Type m_type;
    };

    // PageOverlay::Client
    void willMoveToPage(PageOverlay&, Page*) override;
    void didMoveToPage(PageOverlay&, Page*) override;
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) override;
    bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) override;
    void didScrollFrame(PageOverlay&, Frame&) override;

    void createOverlayIfNeeded();
    void handleClick(const IntPoint&, Highlight&);

    void drawHighlight(Highlight&, GraphicsContext&);

    void invalidateHighlightsOfType(Highlight::Type);
    void buildPotentialHighlightsIfNeeded();

    void replaceHighlightsOfTypePreservingEquivalentHighlights(HashSet<RefPtr<Highlight>>&, Highlight::Type);
    void removeAllPotentialHighlightsOfType(Highlight::Type);
    void buildPhoneNumberHighlights();
    void buildSelectionHighlight();

    void determineActiveHighlight(bool& mouseIsOverButton);
    void clearActiveHighlight();
    Highlight* activeHighlight() const { return m_activeHighlight.get(); }

    Highlight* findTelephoneNumberHighlightContainingSelectionHighlight(Highlight&);

    bool hasRelevantSelectionServices();

    bool mouseIsOverHighlight(Highlight&, bool& mouseIsOverButton) const;
    Seconds remainingTimeUntilHighlightShouldBeShown(Highlight*) const;
    void determineActiveHighlightTimerFired();

    static bool highlightsAreEquivalent(const Highlight* a, const Highlight* b);

    Vector<RefPtr<Range>> telephoneNumberRangesForFocusedFrame();

    void didCreateHighlight(Highlight*);
    void willDestroyHighlight(Highlight*);
    void didFinishFadingOutHighlight(Highlight*);

    Frame& mainFrame() const;
    Page& page() const { return m_page; }

    Page& m_page;
    PageOverlay* m_servicesOverlay { nullptr };

    RefPtr<Highlight> m_activeHighlight;
    RefPtr<Highlight> m_nextActiveHighlight;
    HashSet<RefPtr<Highlight>> m_potentialHighlights;
    HashSet<RefPtr<Highlight>> m_animatingHighlights;

    HashSet<Highlight*> m_highlights;

    // FIXME: These should move onto Highlight.
    Vector<LayoutRect> m_currentSelectionRects;
    bool m_isTextOnly { false };

    Highlight::Type m_dirtyHighlightTypes { 0 };

    MonotonicTime m_lastSelectionChangeTime;
    MonotonicTime m_nextActiveHighlightChangeTime;
    MonotonicTime m_lastMouseUpTime;

    RefPtr<Highlight> m_currentMouseDownOnButtonHighlight;
    IntPoint m_mousePosition;

    Timer m_determineActiveHighlightTimer;
    Timer m_buildHighlightsTimer;
};

} // namespace WebCore

#endif // (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)) && PLATFORM(MAC)
