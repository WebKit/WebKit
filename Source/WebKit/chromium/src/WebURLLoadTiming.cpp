/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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
#include "WebURLLoadTiming.h"

#include "ResourceLoadTiming.h"
#include "WebString.h"

using namespace WebCore;

namespace WebKit {

void WebURLLoadTiming::initialize()
{
    m_private = ResourceLoadTiming::create();
}

void WebURLLoadTiming::reset()
{
    m_private.reset();
}

void WebURLLoadTiming::assign(const WebURLLoadTiming& other)
{
    m_private = other.m_private;
}

double WebURLLoadTiming::requestTime() const
{
    return m_private->requestTime;
}

void WebURLLoadTiming::setRequestTime(double time)
{
    m_private->requestTime = time;
}

int WebURLLoadTiming::proxyStart() const
{
    return m_private->proxyStart;
}

void WebURLLoadTiming::setProxyStart(int start)
{
    m_private->proxyStart = start;
}

int WebURLLoadTiming::proxyEnd() const
{
    return m_private->proxyEnd;
}

void WebURLLoadTiming::setProxyEnd(int end)
{
    m_private->proxyEnd = end;
}

int WebURLLoadTiming::dnsStart() const
{
    return m_private->dnsStart;
}

void WebURLLoadTiming::setDNSStart(int start)
{
    m_private->dnsStart = start;
}

int WebURLLoadTiming::dnsEnd() const
{
    return m_private->dnsEnd;
}

void WebURLLoadTiming::setDNSEnd(int end)
{
    m_private->dnsEnd = end;
}

int WebURLLoadTiming::connectStart() const
{
    return m_private->connectStart;
}

void WebURLLoadTiming::setConnectStart(int start)
{
    m_private->connectStart = start;
}

int WebURLLoadTiming::connectEnd() const
{
    return m_private->connectEnd;
}

void WebURLLoadTiming::setConnectEnd(int end)
{
    m_private->connectEnd = end;
}

int WebURLLoadTiming::sendStart() const
{
    return m_private->sendStart;
}

void WebURLLoadTiming::setSendStart(int start)
{
    m_private->sendStart = start;
}

int WebURLLoadTiming::sendEnd() const
{
    return m_private->sendEnd;
}

void WebURLLoadTiming::setSendEnd(int end)
{
    m_private->sendEnd = end;
}

int WebURLLoadTiming::receiveHeadersEnd() const
{
    return m_private->receiveHeadersEnd;
}

void WebURLLoadTiming::setReceiveHeadersEnd(int end)
{
    m_private->receiveHeadersEnd = end;
}

int WebURLLoadTiming::sslStart() const
{
    return m_private->sslStart;
}

void WebURLLoadTiming::setSSLStart(int start)
{
    m_private->sslStart = start;
}

int WebURLLoadTiming::sslEnd() const
{
    return m_private->sslEnd;
}

void WebURLLoadTiming::setSSLEnd(int end)
{
    m_private->sslEnd = end;
}

WebURLLoadTiming::WebURLLoadTiming(const PassRefPtr<ResourceLoadTiming>& value)
    : m_private(value)
{
}

WebURLLoadTiming& WebURLLoadTiming::operator=(const PassRefPtr<ResourceLoadTiming>& value)
{
    m_private = value;
    return *this;
}

WebURLLoadTiming::operator PassRefPtr<ResourceLoadTiming>() const
{
    return m_private.get();
}

} // namespace WebKit
