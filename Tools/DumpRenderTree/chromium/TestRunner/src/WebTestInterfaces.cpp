/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebTestInterfaces.h"

#include "MockWebMediaStreamCenter.h"
#include "MockWebRTCPeerConnectionHandler.h"
#include "TestInterfaces.h"
#include "TestRunner.h"

using namespace WebKit;

namespace WebTestRunner {

WebTestInterfaces::WebTestInterfaces()
    : m_interfaces(new TestInterfaces())
{
}

WebTestInterfaces::~WebTestInterfaces()
{
}

void WebTestInterfaces::setWebView(WebView* webView, WebTestProxyBase* proxy)
{
    m_interfaces->setWebView(webView, proxy);
}

void WebTestInterfaces::setDelegate(WebTestDelegate* delegate)
{
    m_interfaces->setDelegate(delegate);
}

void WebTestInterfaces::bindTo(WebFrame* frame)
{
    m_interfaces->bindTo(frame);
}

void WebTestInterfaces::resetAll()
{
    m_interfaces->resetAll();
}

void WebTestInterfaces::setTestIsRunning(bool running)
{
    m_interfaces->setTestIsRunning(running);
}

void WebTestInterfaces::configureForTestWithURL(const WebURL& testURL, bool generatePixels)
{
    m_interfaces->configureForTestWithURL(testURL, generatePixels);
}

WebTestRunner* WebTestInterfaces::testRunner()
{
    return m_interfaces->testRunner();
}

WebThemeEngine* WebTestInterfaces::themeEngine()
{
    return m_interfaces->themeEngine();
}

TestInterfaces* WebTestInterfaces::testInterfaces()
{
    return m_interfaces.get();
}

#if ENABLE_WEBRTC
WebMediaStreamCenter* WebTestInterfaces::createMediaStreamCenter(WebMediaStreamCenterClient* client)
{
    return new MockWebMediaStreamCenter(client);
}

WebRTCPeerConnectionHandler* WebTestInterfaces::createWebRTCPeerConnectionHandler(WebRTCPeerConnectionHandlerClient* client)
{
    return new MockWebRTCPeerConnectionHandler(client, m_interfaces.get());
}
#endif // ENABLE_WEBRTC

}
