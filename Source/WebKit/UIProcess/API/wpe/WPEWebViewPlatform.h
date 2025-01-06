/*
 * Copyright (C) 2024 Igalia S.L.
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

#if ENABLE(WPE_PLATFORM)

#include "GRefPtrWPE.h"
#include "RendererBufferFormat.h"
#include "WPEWebView.h"
#include <wpe/wpe-platform.h>
#include <wtf/HashMap.h>
#include <wtf/glib/GRefPtr.h>

namespace WebKit {
class AcceleratedBackingStoreDMABuf;
class WebPlatformTouchPoint;
}

namespace WKWPE {

class ViewPlatform final : public View {
public:
    static Ref<View> create(WPEDisplay* display, const API::PageConfiguration& configuration)
    {
        return adoptRef(*new ViewPlatform(display, configuration));
    }
    ~ViewPlatform();

#if ENABLE(FULLSCREEN_API)
    void enterFullScreen();
    void didEnterFullScreen();
    void exitFullScreen();
    void didExitFullScreen();
    void requestExitFullScreen();
#endif

    void updateAcceleratedSurface(uint64_t);
    WebKit::RendererBufferFormat renderBufferFormat() const;

private:
    ViewPlatform(WPEDisplay*, const API::PageConfiguration&);

    WPEView* wpeView() const override { return m_wpeView.get(); }
    void synthesizeCompositionKeyPress(const String&, std::optional<Vector<WebCore::CompositionUnderline>>&&, std::optional<WebKit::EditingRange>&&) override;
    void callAfterNextPresentationUpdate(CompletionHandler<void()>&&) override;
    void setCursor(const WebCore::Cursor&) override;

    void updateDisplayID();
    bool activityStateChanged(WebCore::ActivityState, bool);
    void toplevelStateChanged(WPEToplevelState previousState, WPEToplevelState);

#if ENABLE(POINTER_LOCK)
    void requestPointerLock() override;
    void didLosePointerLock() override;
#endif

#if ENABLE(TOUCH_EVENTS)
    Vector<WebKit::WebPlatformTouchPoint> touchPointsForEvent(WPEEvent*);
#endif

    gboolean handleEvent(WPEEvent*);
    void handleGesture(WPEEvent*);

    GRefPtr<WPEView> m_wpeView;
    RefPtr<WebKit::AcceleratedBackingStoreDMABuf> m_backingStore;
    uint32_t m_displayID { 0 };
    unsigned long m_bufferRenderedID { 0 };
    CompletionHandler<void()> m_nextPresentationUpdateCallback;
    HashMap<uint32_t, GRefPtr<WPEEvent>, IntHash<uint32_t>, WTF::UnsignedWithZeroKeyHashTraits<uint32_t>> m_touchEvents;
#if ENABLE(FULLSCREEN_API)
    bool m_viewWasAlreadyInFullScreen { false };
#endif
};

} // namespace WKWPE

#endif // ENABLE(WPE_PLATFORM)
