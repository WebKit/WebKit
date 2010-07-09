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

double WebURLLoadTiming::proxyDuration() const
{
    return m_private->proxyDuration;
}

void WebURLLoadTiming::setProxyDuration(double duration)
{
    m_private->proxyDuration = duration;
}
double WebURLLoadTiming::dnsDuration() const
{
    return m_private->dnsDuration;
}

void WebURLLoadTiming::setDNSDuration(double duration)
{
    m_private->dnsDuration = duration;
}

double WebURLLoadTiming::connectDuration() const
{
    return m_private->connectDuration;
}

void WebURLLoadTiming::setConnectDuration(double duration)
{
    m_private->connectDuration = duration;
}

double WebURLLoadTiming::sendDuration() const
{
    return m_private->sendDuration;
}

void WebURLLoadTiming::setSendDuration(double duration)
{
    m_private->sendDuration = duration;
}

double WebURLLoadTiming::receiveHeadersDuration() const
{
    return m_private->receiveHeadersDuration;
}

void WebURLLoadTiming::setReceiveHeadersDuration(double duration)
{
    m_private->receiveHeadersDuration = duration;
}


double WebURLLoadTiming::sslDuration() const
{
    return m_private->sslDuration;
}

void WebURLLoadTiming::setSSLDuration(double duration)
{
    m_private->sslDuration = duration;
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
