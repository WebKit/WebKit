/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
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
#include "BackForwardListChromium.h"

#include "HistoryItem.h"
#include "WebViewClient.h"
#include "WebViewImpl.h"
#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

namespace WebKit {

const char backForwardNavigationScheme[] = "chrome-back-forward";

PassRefPtr<BackForwardListChromium> BackForwardListChromium::create(WebViewImpl* webView)
{
    return adoptRef(new BackForwardListChromium(webView));
}

BackForwardListChromium::BackForwardListChromium(WebViewImpl* webView)
    : m_webView(webView)
{
}

BackForwardListChromium::~BackForwardListChromium()
{
}

void BackForwardListChromium::addItem(PassRefPtr<HistoryItem> item)
{
    m_currentItem = item;

    // If WebCore adds a new HistoryItem, it means this is a new navigation (ie,
    // not a reload or back/forward).
    m_webView->observeNewNavigation();

    if (m_webView->client())
        m_webView->client()->didAddHistoryItem();
}

void BackForwardListChromium::goToItem(HistoryItem* item)
{
    m_currentItem = item;

    if (m_pendingHistoryItem == item)
        m_pendingHistoryItem = 0;
}

HistoryItem* BackForwardListChromium::itemAtIndex(int index)
{
    if (!m_webView->client())
        return 0;

    if (!index)
        return m_currentItem.get();

    if (index > forwardListCount() || -index > backListCount())
        return 0;

    // Since we don't keep the entire back/forward list, we have no way to
    // properly implement this method.  We return a dummy entry instead that we
    // intercept in our FrameLoaderClient implementation in case WebCore asks
    // to navigate to this HistoryItem.

    // FIXME: We should change WebCore to handle history.{back,forward,go}
    // differently.  It should perhaps just ask the FrameLoaderClient to
    // perform those navigations.

    String urlString = makeString(backForwardNavigationScheme, "://go/", String::number(index));
    m_pendingHistoryItem = HistoryItem::create(urlString, String(), 0);
    return m_pendingHistoryItem.get();
}

int BackForwardListChromium::backListCount()
{
    if (!m_webView->client())
        return 0;

    return m_webView->client()->historyBackListCount();
}

int BackForwardListChromium::forwardListCount()
{
    if (!m_webView->client())
        return 0;

    return m_webView->client()->historyForwardListCount();
}

bool BackForwardListChromium::isActive()
{
    return m_webView->client();
}

void BackForwardListChromium::close()
{
    m_currentItem = 0;
    m_pendingHistoryItem = 0;
}

} // namespace WebKit
