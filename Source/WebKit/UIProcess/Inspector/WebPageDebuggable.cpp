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

namespace WebKit {

using namespace Inspector;

WebPageDebuggable::WebPageDebuggable(WebPageProxy& page)
    : m_page(page)
{
}

String WebPageDebuggable::name() const
{
    if (!m_nameOverride.isNull())
        return m_nameOverride;

    if (!m_page.mainFrame())
        return String();

    return m_page.mainFrame()->title();
}

String WebPageDebuggable::url() const
{
    if (!m_page.mainFrame())
        return WTF::blankURL().string();

    String url = m_page.mainFrame()->url().string();
    if (url.isEmpty())
        return WTF::blankURL().string();

    return url;
}

bool WebPageDebuggable::hasLocalDebugger() const
{
    return m_page.inspectorController().hasLocalFrontend();
}

void WebPageDebuggable::connect(FrontendChannel& channel, bool isAutomaticConnection, bool immediatelyPause)
{
    m_page.inspectorController().connectFrontend(channel, isAutomaticConnection, immediatelyPause);
}

void WebPageDebuggable::disconnect(FrontendChannel& channel)
{
    m_page.inspectorController().disconnectFrontend(channel);
}

void WebPageDebuggable::dispatchMessageFromRemote(const String& message)
{
    m_page.inspectorController().dispatchMessageFromFrontend(message);
}

void WebPageDebuggable::setIndicating(bool indicating)
{
    m_page.inspectorController().setIndicating(indicating);
}

void WebPageDebuggable::setNameOverride(const String& name)
{
    m_nameOverride = name;
    update();
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
