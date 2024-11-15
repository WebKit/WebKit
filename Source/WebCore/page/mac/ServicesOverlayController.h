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

#include "DataDetectorHighlight.h"
#include "GraphicsLayer.h"
#include "GraphicsLayerClient.h"
#include "PageOverlay.h"
#include "Timer.h"
#include <wtf/MonotonicTime.h>
#include <wtf/OptionSet.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/WeakHashSet.h>

namespace WebCore {
    
class LayoutRect;
class Page;

enum class RenderingUpdateStep : uint32_t;

struct GapRects;

class ServicesOverlayController : private DataDetectorHighlightClient, private PageOverlayClient {
    WTF_MAKE_TZONE_ALLOCATED(ServicesOverlayController);
public:
    explicit ServicesOverlayController(Page&);
    ~ServicesOverlayController();

    void ref() const;
    void deref() const;

    void selectedTelephoneNumberRangesChanged();
    void selectionRectsDidChange(const Vector<LayoutRect>&, const Vector<GapRects>&, bool isTextOnly);

private:
    // PageOverlayClient
    void willMoveToPage(PageOverlay&, Page*) override;
    void didMoveToPage(PageOverlay&, Page*) override;
    void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) override;
    bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) override;
    void didScrollFrame(PageOverlay&, LocalFrame&) override;

    void createOverlayIfNeeded();
    void handleClick(const IntPoint&, DataDetectorHighlight&);

    void drawHighlight(DataDetectorHighlight&, GraphicsContext&);

    void invalidateHighlightsOfType(DataDetectorHighlight::Type);
    void buildPotentialHighlightsIfNeeded();

    void replaceHighlightsOfTypePreservingEquivalentHighlights(HashSet<RefPtr<DataDetectorHighlight>>&, DataDetectorHighlight::Type);
    void removeAllPotentialHighlightsOfType(DataDetectorHighlight::Type);
    void buildPhoneNumberHighlights();
    void buildSelectionHighlight();

    void determineActiveHighlight(bool& mouseIsOverButton);
    void clearActiveHighlight();

#if ENABLE(DATA_DETECTION)
    // DataDetectorHighlightClient
    DataDetectorHighlight* activeHighlight() const final { return m_activeHighlight.get(); }
    void scheduleRenderingUpdate(OptionSet<RenderingUpdateStep>) final;
    float deviceScaleFactor() const final;
    RefPtr<GraphicsLayer> createGraphicsLayer(GraphicsLayerClient&) final;
#endif

    DataDetectorHighlight* findTelephoneNumberHighlightContainingSelectionHighlight(DataDetectorHighlight&);

    bool hasRelevantSelectionServices();

    bool mouseIsOverHighlight(DataDetectorHighlight&, bool& mouseIsOverButton) const;
    Seconds remainingTimeUntilHighlightShouldBeShown(DataDetectorHighlight*) const;
    void determineActiveHighlightTimerFired();

    Vector<SimpleRange> telephoneNumberRangesForFocusedFrame();

    Page& page() const { return m_page; }
    Ref<Page> protectedPage() const { return m_page.get(); }

    WeakRef<Page> m_page;
    WeakPtr<PageOverlay> m_servicesOverlay;

    RefPtr<DataDetectorHighlight> m_activeHighlight;
    RefPtr<DataDetectorHighlight> m_nextActiveHighlight;
    HashSet<RefPtr<DataDetectorHighlight>> m_potentialHighlights;
    HashSet<RefPtr<DataDetectorHighlight>> m_animatingHighlights;
    WeakHashSet<DataDetectorHighlight> m_highlights;

    Vector<LayoutRect> m_currentSelectionRects;
    bool m_isTextOnly { false };

    OptionSet<DataDetectorHighlight::Type> m_dirtyHighlightTypes;

    MonotonicTime m_lastSelectionChangeTime;
    MonotonicTime m_nextActiveHighlightChangeTime;
    MonotonicTime m_lastMouseUpTime;

    RefPtr<DataDetectorHighlight> m_currentMouseDownOnButtonHighlight;
    IntPoint m_mousePosition;

    Timer m_determineActiveHighlightTimer;
    Timer m_buildHighlightsTimer;
};

} // namespace WebCore

#endif // (ENABLE(SERVICE_CONTROLS) || ENABLE(TELEPHONE_NUMBER_DETECTION)) && PLATFORM(MAC)
