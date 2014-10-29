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

#ifndef PageOverlay_h
#define PageOverlay_h

#include "Color.h"
#include "FloatPoint.h"
#include "IntRect.h"
#include "Timer.h"
#include <wtf/PassRefPtr.h>
#include <wtf/RefCounted.h>
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
        virtual ~Client() { }
    
    public:
        virtual void pageOverlayDestroyed(PageOverlay&) = 0;
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

    static PassRefPtr<PageOverlay> create(Client&, OverlayType = OverlayType::View);
    virtual ~PageOverlay();

    PageOverlayController* controller() const;

    void setPage(Page*);
    void setNeedsDisplay(const IntRect& dirtyRect);
    void setNeedsDisplay();

    void drawRect(GraphicsContext&, const IntRect& dirtyRect);
    bool mouseEvent(const PlatformMouseEvent&);
    void didScrollFrame(Frame&);

    bool copyAccessibilityAttributeStringValueForPoint(String attribute, FloatPoint parameter, String& value);
    bool copyAccessibilityAttributeBoolValueForPoint(String attribute, FloatPoint parameter, bool& value);
    Vector<String> copyAccessibilityAttributeNames(bool parameterizedNames);
    
    void startFadeInAnimation();
    void startFadeOutAnimation();
    void stopFadeOutAnimation();

    void clear();

    Client& client() const { return m_client; }

    enum class FadeMode { DoNotFade, Fade };

    OverlayType overlayType() { return m_overlayType; }

    IntRect bounds() const;
    IntRect frame() const;
    void setFrame(IntRect);

    RGBA32 backgroundColor() const { return m_backgroundColor; }
    void setBackgroundColor(RGBA32);
    
    // FIXME: PageOverlay should own its layer, instead of PageOverlayController.
    GraphicsLayer& layer();

private:
    explicit PageOverlay(Client&, OverlayType);

    void startFadeAnimation();
    void fadeAnimationTimerFired(Timer<PageOverlay>&);

    Client& m_client;
    Page* m_page;

    Timer<PageOverlay> m_fadeAnimationTimer;
    double m_fadeAnimationStartTime;
    double m_fadeAnimationDuration;

    enum FadeAnimationType {
        NoAnimation,
        FadeInAnimation,
        FadeOutAnimation,
    };

    FadeAnimationType m_fadeAnimationType;
    float m_fractionFadedIn;

    OverlayType m_overlayType;
    IntRect m_overrideFrame;

    RGBA32 m_backgroundColor;
};

} // namespace WebKit

#endif // PageOverlay_h
