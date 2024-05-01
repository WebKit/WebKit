/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <WebCore/LocalDOMWindow.h>
#include <wtf/Forward.h>

namespace WebCore {
class LocalFrame;
}

namespace WebKit {

class WebAutomationDOMWindowObserver final : public RefCounted<WebAutomationDOMWindowObserver>, public WebCore::LocalDOMWindowObserver {
public:
    static Ref<WebAutomationDOMWindowObserver> create(WebCore::LocalDOMWindow& window, WTF::Function<void(WebAutomationDOMWindowObserver&)>&& callback)
    {
        return adoptRef(*new WebAutomationDOMWindowObserver(window, WTFMove(callback)));
    }

    ~WebAutomationDOMWindowObserver();

    // All of these observer callbacks are interpreted as a signal that a frame has been detached and
    // can no longer accept new commands nor finish pending commands (eg, evaluating JavaScript).
    void willDestroyGlobalObjectInCachedFrame() final;
    void willDestroyGlobalObjectInFrame() final;
    void willDetachGlobalObjectFromFrame() final;

private:
    WebAutomationDOMWindowObserver(WebCore::LocalDOMWindow&, WTF::Function<void(WebAutomationDOMWindowObserver&)>&&);

    WeakPtr<WebCore::LocalDOMWindow, WebCore::WeakPtrImplWithEventTargetData> m_window;
    bool m_wasDetached { false };
    WTF::Function<void(WebAutomationDOMWindowObserver&)> m_callback;
};

} // namespace WebKit
