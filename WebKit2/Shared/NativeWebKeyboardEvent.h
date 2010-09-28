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

#ifndef NativeWebKeyboardEvent_h
#define NativeWebKeyboardEvent_h

#include "WebEvent.h"

#if PLATFORM(MAC)
#ifdef __OBJC__
@class NSView;
#else
class NSView;
#endif
#elif PLATFORM(QT)
#include <QKeyEvent>
#endif

namespace WebKit {

class NativeWebKeyboardEvent : public WebKeyboardEvent {
public:
#if PLATFORM(MAC)
    NativeWebKeyboardEvent(NSEvent *, NSView *);
#elif PLATFORM(WIN)
    NativeWebKeyboardEvent(HWND, UINT message, WPARAM, LPARAM);
#elif PLATFORM(QT)
    explicit NativeWebKeyboardEvent(QKeyEvent*);
#endif

#if PLATFORM(MAC)
    NSEvent *nativeEvent() const { return m_nativeEvent.get(); }
#elif PLATFORM(WIN)
    const MSG* nativeEvent() const { return &m_nativeEvent; }
#elif PLATFORM(QT)
    const QKeyEvent* nativeEvent() const { return &m_nativeEvent; }
#endif

private:
#if PLATFORM(MAC)
    RetainPtr<NSEvent> m_nativeEvent;
#elif PLATFORM(WIN)
    MSG m_nativeEvent;
#elif PLATFORM(QT)
    QKeyEvent m_nativeEvent;
#endif
};

} // namespace WebKit

#endif // NativeWebKeyboardEvent_h
