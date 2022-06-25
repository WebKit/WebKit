/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "Color.h"
#include "FloatPoint.h"
#include "IntRect.h"
#include "Timer.h"
#include <wtf/RefCounted.h>
#include <wtf/WallTime.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Frame;
class GraphicsContext;
class GraphicsLayer;
class Page;
class PageOverlayController;
class PlatformMouseEvent;

class PageOverlay final : public RefCounted<PageOverlay> {
    WTF_MAKE_NONCOPYABLE(PageOverlay);
    WTF_MAKE_FAST_ALLOCATED;
public:
    class Client {
    protected:
        virtual ~Client() = default;
    
    public:
        virtual void willMoveToPage(PageOverlay&, Page*) = 0;
        virtual void didMoveToPage(PageOverlay&, Page*) = 0;
        virtual void drawRect(PageOverlay&, GraphicsContext&, const IntRect& dirtyRect) = 0;
        virtual bool mouseEvent(PageOverlay&, const PlatformMouseEvent&) = 0;
        virtual void didScrollFrame(PageOverlay&, Frame&) { }

        virtual bool copyAccessibilityAttributeStringValueForPoint(PageOverlay&, String /* attribute */, FloatPoint, String&) { return false; }
        virtual bool copyAccessibilityAttributeBoolValueForPoint(PageOverlay&, String /* attribute */, FloatPoint, bool&)  { return false; }
        virtual Vector<String> copyAccessibilityAttributeNames(PageOverlay&, bool /* parameterizedNames */)  { return { }; }
    };

    enum class OverlayType {
        View, // Fixed to the view size; does not scale or scroll with the document, repaints on scroll.
        Document, // Scales and scrolls with the document.
    };

    WEBCORE_EXPORT static Ref<PageOverlay> create(Client&, OverlayType = OverlayType::View);
    WEBCORE_EXPORT virtual ~PageOverlay();

    WEBCORE_EXPORT PageOverlayController* controller() const;

    typedef uint64_t PageOverlayID;
    virtual PageOverlayID pageOverlayID() const { return m_pageOverlayID; }

    void setPage(Page*);
    Page* page() const { return m_page; }
    WEBCORE_EXPORT void setNeedsDisplay(const IntRect& dirtyRect);
    WEBCORE_EXPORT void setNeedsDisplay();

    void drawRect(GraphicsContext&, const IntRect& dirtyRect);
    bool mouseEvent(const PlatformMouseEvent&);
    void didScrollFrame(Frame&);

    bool copyAccessibilityAttributeStringValueForPoint(String attribute, FloatPoint parameter, String& value);
    bool copyAccessibilityAttributeBoolValueForPoint(String attribute, FloatPoint parameter, bool& value);
    Vector<String> copyAccessibilityAttributeNames(bool parameterizedNames);
    
    void startFadeInAnimation();
    void startFadeOutAnimation();
    WEBCORE_EXPORT void stopFadeOutAnimation();

    WEBCORE_EXPORT void clear();

    Client& client() const { return m_client; }

    enum class FadeMode { DoNotFade, Fade };

    OverlayType overlayType() { return m_overlayType; }

    WEBCORE_EXPORT IntRect bounds() const;
    WEBCORE_EXPORT IntRect frame() const;
    WEBCORE_EXPORT void setFrame(IntRect);

    WEBCORE_EXPORT IntSize viewToOverlayOffset() const;

    const Color& backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(const Color&);

    void setShouldIgnoreMouseEventsOutsideBounds(bool flag) { m_shouldIgnoreMouseEventsOutsideBounds = flag; }

    // FIXME: PageOverlay should own its layer, instead of PageOverlayController.
    WEBCORE_EXPORT GraphicsLayer& layer();

    bool needsSynchronousScrolling() const { return m_needsSynchronousScrolling; }
    void setNeedsSynchronousScrolling(bool needsSynchronousScrolling) { m_needsSynchronousScrolling = needsSynchronousScrolling; }

private:
    explicit PageOverlay(Client&, OverlayType);

    void startFadeAnimation();
    void fadeAnimationTimerFired();

    Client& m_client;
    Page* m_page { nullptr };

    Timer m_fadeAnimationTimer;
    WallTime m_fadeAnimationStartTime;
    Seconds m_fadeAnimationDuration;

    enum FadeAnimationType {
        NoAnimation,
        FadeInAnimation,
        FadeOutAnimation,
    };

    FadeAnimationType m_fadeAnimationType { NoAnimation };
    float m_fractionFadedIn { 1 };

    bool m_needsSynchronousScrolling;

    OverlayType m_overlayType;
    IntRect m_overrideFrame;

    Color m_backgroundColor { Color::transparentBlack };
    PageOverlayID m_pageOverlayID;

    bool m_shouldIgnoreMouseEventsOutsideBounds { true };
};

} // namespace WebKit
