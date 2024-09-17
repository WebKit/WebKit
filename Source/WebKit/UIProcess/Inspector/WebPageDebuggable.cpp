/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebPageDebuggable.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "WebFrameProxy.h"
#include "WebPageInspectorController.h"
#include "WebPageProxy.h"
#include <wtf/MainThread.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPageDebuggable);

Ref<WebPageDebuggable> WebPageDebuggable::create(WebPageProxy& page)
{
    return adoptRef(*new WebPageDebuggable(page));
}

WebPageDebuggable::WebPageDebuggable(WebPageProxy& page)
    : m_page(&page)
{
}

void WebPageDebuggable::detachFromPage()
{
    m_page = nullptr;
}

String WebPageDebuggable::name() const
{
    String name;
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &name] {
        if (!m_page || !m_page->mainFrame())
            return;
        name = m_page->mainFrame()->title().isolatedCopy();
    });
    return name;
}

String WebPageDebuggable::url() const
{
    String url;
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &url] {
        if (!m_page || !m_page->mainFrame()) {
            url = aboutBlankURL().string().isolatedCopy();
            return;
        }

        url = m_page->mainFrame()->url().string().isolatedCopy();
        if (url.isEmpty())
            url = aboutBlankURL().string().isolatedCopy();
    });
    return url;
}

bool WebPageDebuggable::hasLocalDebugger() const
{
    bool hasLocalDebugger;
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &hasLocalDebugger] {
        hasLocalDebugger = m_page->inspectorController().hasLocalFrontend();
    });
    return hasLocalDebugger;
}

void WebPageDebuggable::connect(FrontendChannel& channel, bool isAutomaticConnection, bool immediatelyPause)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &channel, isAutomaticConnection, immediatelyPause] {
        m_page->inspectorController().connectFrontend(channel, isAutomaticConnection, immediatelyPause);
    });
}

void WebPageDebuggable::disconnect(FrontendChannel& channel)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, &channel] {
        m_page->inspectorController().disconnectFrontend(channel);
    });
}

void WebPageDebuggable::dispatchMessageFromRemote(String&& message)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, message = WTFMove(message).isolatedCopy()]() mutable {
        m_page->inspectorController().dispatchMessageFromFrontend(WTFMove(message));
    });
}

void WebPageDebuggable::setIndicating(bool indicating)
{
    callOnMainRunLoopAndWait([this, protectedThis = Ref { *this }, indicating] {
        m_page->inspectorController().setIndicating(indicating);
    });
}

void WebPageDebuggable::setNameOverride(const String& name)
{
    m_nameOverride = name;
    update();
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
