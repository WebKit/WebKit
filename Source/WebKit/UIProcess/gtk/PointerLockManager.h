/*
 * Copyright (C) 2019 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebEvent.h"
#include <WebCore/FloatPoint.h>
#include <wtf/Noncopyable.h>

typedef struct _GdkDevice GdkDevice;
#if USE(GTK4)
typedef struct _GdkEvent GdkEvent;
#else
typedef union _GdkEvent GdkEvent;
#endif

namespace WebKit {

class WebPageProxy;

class PointerLockManager {
    WTF_MAKE_NONCOPYABLE(PointerLockManager); WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<PointerLockManager> create(WebPageProxy&, const WebCore::FloatPoint&, const WebCore::FloatPoint&, WebMouseEvent::Button, unsigned short, OptionSet<WebEvent::Modifier>);
    PointerLockManager(WebPageProxy&, const WebCore::FloatPoint&, const WebCore::FloatPoint&, WebMouseEvent::Button, unsigned short, OptionSet<WebEvent::Modifier>);
    virtual ~PointerLockManager();

    virtual bool lock();
    virtual bool unlock();
    virtual void didReceiveMotionEvent(const WebCore::FloatPoint&) { };

protected:
    void handleMotion(WebCore::FloatSize&&);

    WebPageProxy& m_webPage;
    WebCore::FloatPoint m_position;
    WebMouseEvent::Button m_button { WebMouseEvent::NoButton };
    unsigned short m_buttons { 0 };
    OptionSet<WebEvent::Modifier> m_modifiers;
    WebCore::FloatPoint m_initialPoint;
    GdkDevice* m_device { nullptr };
};

} // namespace WebKit
