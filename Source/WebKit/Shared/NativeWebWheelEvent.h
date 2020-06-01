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

#ifndef NativeWebWheelEvent_h
#define NativeWebWheelEvent_h

#include "WebEvent.h"

#if USE(APPKIT)
#include <wtf/RetainPtr.h>
OBJC_CLASS NSView;
#endif

#if PLATFORM(GTK)
#include <WebCore/GUniquePtrGtk.h>
#if USE(GTK4)
typedef struct _GdkEvent GdkEvent;
#else
typedef union _GdkEvent GdkEvent;
#endif
#endif

#if USE(LIBWPE)
struct wpe_input_axis_event;
#endif

#if PLATFORM(WIN)
#include <windows.h>
#endif

namespace WebKit {

class NativeWebWheelEvent : public WebWheelEvent {
public:
#if USE(APPKIT)
    NativeWebWheelEvent(NSEvent *, NSView *);
#elif PLATFORM(GTK)
    NativeWebWheelEvent(const NativeWebWheelEvent&);
    NativeWebWheelEvent(GdkEvent*);
    NativeWebWheelEvent(GdkEvent*, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
    NativeWebWheelEvent(GdkEvent*, const WebCore::IntPoint&, const WebCore::FloatSize& wheelTicks);
    NativeWebWheelEvent(const WebCore::IntPoint& position, const WebCore::IntPoint& globalPosition, const WebCore::FloatSize& delta, const WebCore::FloatSize& wheelTicks, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
#elif USE(LIBWPE)
    NativeWebWheelEvent(struct wpe_input_axis_event*, float deviceScaleFactor, WebWheelEvent::Phase, WebWheelEvent::Phase momentumPhase);
#elif PLATFORM(WIN)
    NativeWebWheelEvent(HWND, UINT message, WPARAM, LPARAM);
#endif

#if USE(APPKIT)
    NSEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(GTK)
    GdkEvent* nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(WIN)
    const MSG* nativeEvent() const { return &m_nativeEvent; }
#else
    const void* nativeEvent() const { return nullptr; }
#endif

private:
#if USE(APPKIT)
    RetainPtr<NSEvent> m_nativeEvent;
#elif PLATFORM(GTK)
    GUniquePtr<GdkEvent> m_nativeEvent;
#elif PLATFORM(WIN)
    MSG m_nativeEvent;
#endif
};

} // namespace WebKit

#endif // NativeWebWheelEvent_h
