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

#include "config.h"
#include "WebAutomationDOMWindowObserver.h"

#include <WebCore/Element.h>
#include <WebCore/Frame.h>

namespace WebKit {

WebAutomationDOMWindowObserver::WebAutomationDOMWindowObserver(WebCore::DOMWindow& window, WTF::Function<void(WebAutomationDOMWindowObserver&)>&& callback)
    : m_window(makeWeakPtr(window))
    , m_callback(WTFMove(callback))
{
    ASSERT(m_window->frame());
    m_window->registerObserver(*this);
}

WebAutomationDOMWindowObserver::~WebAutomationDOMWindowObserver()
{
    if (m_window)
        m_window->unregisterObserver(*this);
}

void WebAutomationDOMWindowObserver::willDestroyGlobalObjectInCachedFrame()
{
    Ref<WebAutomationDOMWindowObserver> protectedThis(*this);

    if (!m_wasDetached) {
        ASSERT(m_window && m_window->frame());
        m_callback(*this);
    }

    ASSERT(m_window);
    if (m_window)
        m_window->unregisterObserver(*this);
    m_window = nullptr;
}

void WebAutomationDOMWindowObserver::willDestroyGlobalObjectInFrame()
{
    Ref<WebAutomationDOMWindowObserver> protectedThis(*this);

    if (!m_wasDetached) {
        ASSERT(m_window && m_window->frame());
        m_callback(*this);
    }

    ASSERT(m_window);
    if (m_window)
        m_window->unregisterObserver(*this);
    m_window = nullptr;
}

void WebAutomationDOMWindowObserver::willDetachGlobalObjectFromFrame()
{
    ASSERT(!m_wasDetached);

    Ref<WebAutomationDOMWindowObserver> protectedThis(*this);

    m_wasDetached = true;

    m_callback(*this);

    ASSERT(m_window);
    if (m_window)
        m_window->unregisterObserver(*this);
    m_window = nullptr;
}

} // namespace WebKit
