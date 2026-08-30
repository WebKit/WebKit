/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include "WebWheelEvent.h"

#if USE(APPKIT)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;
OBJC_CLASS NSEvent;
#endif

#if PLATFORM(GTK)
#include "GRefPtrGtk.h"
#include "GUniquePtrGtk.h"
#if USE(GTK4)
typedef struct _GdkEvent GdkEvent;
#else
typedef union _GdkEvent GdkEvent;
#endif
#endif

#if PLATFORM(WPE) && ENABLE(WPE_PLATFORM)
typedef struct _WPEEvent WPEEvent;
#endif

#if USE(LIBWPE)
struct wpe_input_axis_event;
#endif

#if PLATFORM(WIN)
#include <windows.h>
#endif

namespace WebKit {

class NativeWebWheelEvent : public WebWheelEvent {
    WTF_MAKE_TZONE_ALLOCATED(NativeWebWheelEvent);
public:
#if USE(APPKIT)
    static Ref<NativeWebWheelEvent> create(NSEvent *, NSView *);
    static Ref<NativeWebWheelEvent> create(const WebWheelEvent&);
#elif PLATFORM(GTK)
    static Ref<NativeWebWheelEvent> create(const NativeWebWheelEvent&);
    static Ref<NativeWebWheelEvent> create(GdkEvent*, const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase, bool hasPreciseDeltas = false);
#elif PLATFORM(WPE)
#if USE(LIBWPE)
    static Ref<NativeWebWheelEvent> create(struct wpe_input_axis_event*, float deviceScaleFactor, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
#endif
#if ENABLE(WPE_PLATFORM)
    static Ref<NativeWebWheelEvent> create(WPEEvent*);
    static Ref<NativeWebWheelEvent> create(WPEEvent*, WebWheelEvent::Phase);
#endif
#elif PLATFORM(PLAYSTATION)
    static Ref<NativeWebWheelEvent> create(struct wpe_input_axis_event*, float deviceScaleFactor, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
#elif PLATFORM(WIN)
    static Ref<NativeWebWheelEvent> create(HWND, UINT message, WPARAM, LPARAM, float deviceScaleFactor);
#endif

#if USE(APPKIT)
    NSEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(GTK)
    GdkEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(WIN)
    const MSG* nativeEvent() const LIFETIME_BOUND { return &m_nativeEvent; }
#else
    const void* nativeEvent() const { return nullptr; }
#endif

private:
#if USE(APPKIT)
    NativeWebWheelEvent(WebWheelEventInit&&, NSEvent *);

    RetainPtr<NSEvent> m_nativeEvent;
#elif PLATFORM(GTK) && USE(GTK4)
    NativeWebWheelEvent(WebWheelEventInit&&, GdkEvent*);

    GRefPtr<GdkEvent> m_nativeEvent;
#elif PLATFORM(GTK)
    NativeWebWheelEvent(WebWheelEventInit&&, GdkEvent*);

    GUniquePtr<GdkEvent> m_nativeEvent;
#elif PLATFORM(WIN)
    NativeWebWheelEvent(WebWheelEventInit&&, const MSG&);

    MSG m_nativeEvent;
#else
    explicit NativeWebWheelEvent(WebWheelEventInit&& init)
        : WebWheelEvent(WTF::move(init.event), WTF::move(init.wheel))
    {
    }
#endif
};

} // namespace WebKit
