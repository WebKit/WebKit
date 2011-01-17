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
#include "WebHistoryItem.h"

#include "FormData.h"
#include "HistoryItem.h"
#include "KURL.h"

#include "WebHTTPBody.h"
#include "WebPoint.h"
#include "WebSerializedScriptValue.h"
#include "WebString.h"
#include "WebVector.h"

using namespace WebCore;

namespace WebKit {

void WebHistoryItem::initialize()
{
    m_private = HistoryItem::create();
}

void WebHistoryItem::reset()
{
    m_private.reset();
}

void WebHistoryItem::assign(const WebHistoryItem& other)
{
    m_private = other.m_private;
}

WebString WebHistoryItem::urlString() const
{
    return m_private->urlString();
}

void WebHistoryItem::setURLString(const WebString& url)
{
    ensureMutable();
    m_private->setURLString(KURL(ParsedURLString, url).string());
}

WebString WebHistoryItem::originalURLString() const
{
    return m_private->originalURLString();
}

void WebHistoryItem::setOriginalURLString(const WebString& originalURLString)
{
    ensureMutable();
    m_private->setOriginalURLString(originalURLString);
}

WebString WebHistoryItem::referrer() const
{
    return m_private->referrer();
}

void WebHistoryItem::setReferrer(const WebString& referrer)
{
    ensureMutable();
    m_private->setReferrer(referrer);
}

WebString WebHistoryItem::target() const
{
    return m_private->target();
}

void WebHistoryItem::setTarget(const WebString& target)
{
    ensureMutable();
    m_private->setTarget(target);
}

WebString WebHistoryItem::parent() const
{
    return m_private->parent();
}

void WebHistoryItem::setParent(const WebString& parent)
{
    ensureMutable();
    m_private->setParent(parent);
}

WebString WebHistoryItem::title() const
{
    return m_private->title();
}

void WebHistoryItem::setTitle(const WebString& title)
{
    ensureMutable();
    m_private->setTitle(title);
}

WebString WebHistoryItem::alternateTitle() const
{
    return m_private->alternateTitle();
}

void WebHistoryItem::setAlternateTitle(const WebString& alternateTitle)
{
    ensureMutable();
    m_private->setAlternateTitle(alternateTitle);
}

double WebHistoryItem::lastVisitedTime() const
{
    return m_private->lastVisitedTime();
}

void WebHistoryItem::setLastVisitedTime(double lastVisitedTime)
{
    ensureMutable();
    // FIXME: setLastVisitedTime increments the visit count, so we have to
    // correct for that.  Instead, we should have a back-door to just mutate
    // the last visited time directly.
    int count = m_private->visitCount();
    m_private->setLastVisitedTime(lastVisitedTime);
    m_private->setVisitCount(count);
}

WebPoint WebHistoryItem::scrollOffset() const
{
    return m_private->scrollPoint();
}

void WebHistoryItem::setScrollOffset(const WebPoint& scrollOffset)
{
    ensureMutable();
    m_private->setScrollPoint(scrollOffset);
}

bool WebHistoryItem::isTargetItem() const
{
    return m_private->isTargetItem();
}

void WebHistoryItem::setIsTargetItem(bool isTargetItem)
{
    ensureMutable();
    m_private->setIsTargetItem(isTargetItem);
}

int WebHistoryItem::visitCount() const
{
    return m_private->visitCount();
}

void WebHistoryItem::setVisitCount(int count)
{
    ensureMutable();
    m_private->setVisitCount(count);
}

WebVector<WebString> WebHistoryItem::documentState() const
{
    return m_private->documentState();
}

void WebHistoryItem::setDocumentState(const WebVector<WebString>& state)
{
    ensureMutable();
    // FIXME: would be nice to avoid the intermediate copy
    Vector<String> ds;
    for (size_t i = 0; i < state.size(); ++i)
        ds.append(state[i]);
    m_private->setDocumentState(ds);
}

long long WebHistoryItem::itemSequenceNumber() const
{
    return m_private->itemSequenceNumber();
}

void WebHistoryItem::setItemSequenceNumber(long long itemSequenceNumber)
{
    ensureMutable();
    m_private->setItemSequenceNumber(itemSequenceNumber);
}

long long WebHistoryItem::documentSequenceNumber() const
{
    return m_private->documentSequenceNumber();
}

void WebHistoryItem::setDocumentSequenceNumber(long long documentSequenceNumber)
{
    ensureMutable();
    m_private->setDocumentSequenceNumber(documentSequenceNumber);
}

WebSerializedScriptValue WebHistoryItem::stateObject() const
{
    return WebSerializedScriptValue(m_private->stateObject());
}

void WebHistoryItem::setStateObject(const WebSerializedScriptValue& object)
{
    ensureMutable();
    m_private->setStateObject(object);
}

WebString WebHistoryItem::httpContentType() const
{
    return m_private->formContentType();
}

void WebHistoryItem::setHTTPContentType(const WebString& httpContentType)
{
    ensureMutable();
    m_private->setFormContentType(httpContentType);
}

WebHTTPBody WebHistoryItem::httpBody() const
{
    return WebHTTPBody(m_private->formData());
}

void WebHistoryItem::setHTTPBody(const WebHTTPBody& httpBody)
{
    ensureMutable();
    m_private->setFormData(httpBody);
}

WebVector<WebHistoryItem> WebHistoryItem::children() const
{
    return m_private->children();
}

void WebHistoryItem::setChildren(const WebVector<WebHistoryItem>& items)
{
    ensureMutable();
    m_private->clearChildren();
    for (size_t i = 0; i < items.size(); ++i)
        m_private->addChildItem(items[i]);
}

void WebHistoryItem::appendToChildren(const WebHistoryItem& item)
{
    ensureMutable();
    m_private->addChildItem(item);
}

WebHistoryItem::WebHistoryItem(const PassRefPtr<HistoryItem>& item)
    : m_private(item)
{
}

WebHistoryItem& WebHistoryItem::operator=(const PassRefPtr<HistoryItem>& item)
{
    m_private = item;
    return *this;
}

WebHistoryItem::operator PassRefPtr<HistoryItem>() const
{
    return m_private.get();
}

void WebHistoryItem::ensureMutable()
{
    if (!m_private->hasOneRef())
        m_private = m_private->copy();
}

} // namespace WebKit
