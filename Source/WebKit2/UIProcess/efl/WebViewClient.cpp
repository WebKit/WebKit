/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebViewClient.h"

#include "WKAPICast.h"
#include "WKRetainPtr.h"

using namespace WebCore;

namespace WebKit {

void WebViewClient::viewNeedsDisplay(WebView* view, const IntRect& area)
{
    if (!m_client.viewNeedsDisplay)
        return;

    m_client.viewNeedsDisplay(toAPI(view), toAPI(area), m_client.clientInfo);
}

void WebViewClient::didChangeContentsSize(WebView* view, const IntSize& size)
{
    if (!m_client.didChangeContentsSize)
        return;

    m_client.didChangeContentsSize(toAPI(view), toAPI(size), m_client.clientInfo);
}

void WebViewClient::webProcessCrashed(WebView* view, const String& url)
{
    if (!m_client.webProcessCrashed)
        return;

    m_client.webProcessCrashed(toAPI(view), adoptWK(toCopiedURLAPI(url)).get(), m_client.clientInfo);
}

void WebViewClient::webProcessDidRelaunch(WebView* view)
{
    if (!m_client.webProcessDidRelaunch)
        return;

    m_client.webProcessDidRelaunch(toAPI(view), m_client.clientInfo);
}

} // namespace WebKit
