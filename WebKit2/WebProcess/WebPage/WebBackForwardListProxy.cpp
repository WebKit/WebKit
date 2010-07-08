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

#include "WebBackForwardListProxy.h"

#include <WebCore/HistoryItem.h>

using namespace WebCore;

namespace WebKit {

WebBackForwardListProxy::WebBackForwardListProxy(WebPage* page)
    : m_page(page)
{
}

WebBackForwardListProxy::~WebBackForwardListProxy()
{
}

void WebBackForwardListProxy::addItem(PassRefPtr<HistoryItem>)
{
}

void WebBackForwardListProxy::goToItem(HistoryItem*)
{
}

HistoryItem* WebBackForwardListProxy::backItem()
{
    return 0;
}

HistoryItem* WebBackForwardListProxy::forwardItem()
{
    return 0;
}

HistoryItem* WebBackForwardListProxy::currentItem()
{
    return 0;
}

int WebBackForwardListProxy::capacity()
{
    return 0;
}

void WebBackForwardListProxy::setCapacity(int)
{
}

bool WebBackForwardListProxy::enabled()
{
    return false;
}

void WebBackForwardListProxy::setEnabled(bool)
{
}

int WebBackForwardListProxy::backListCount()
{
    return 0;
}

int WebBackForwardListProxy::forwardListCount()
{
    return 0;
}

HistoryItem* WebBackForwardListProxy::itemAtIndex(int)
{
    return 0;
}

void WebBackForwardListProxy::pushStateItem(PassRefPtr<HistoryItem>)
{
}

HistoryItemVector& WebBackForwardListProxy::entries()
{
    static HistoryItemVector noEntries;
    return noEntries;
}

void WebBackForwardListProxy::close()
{
}

bool WebBackForwardListProxy::closed()
{
    return true;
}

void WebBackForwardListProxy::goBack()
{
}

void WebBackForwardListProxy::goForward()
{
}

void WebBackForwardListProxy::backListWithLimit(int, HistoryItemVector&)
{
}

void WebBackForwardListProxy::forwardListWithLimit(int, HistoryItemVector&)
{
}

bool WebBackForwardListProxy::containsItem(HistoryItem*)
{
    return false;
}

void WebBackForwardListProxy::removeItem(HistoryItem*)
{
}

#if ENABLE(WML)
void WebBackForwardListProxy::clearWMLPageHistory()
{
}
#endif

} // namespace WebKit
