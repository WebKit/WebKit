/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "URLKeepingBlobAlive.h"

#include "ThreadableBlobRegistry.h"

namespace WebCore {

URLKeepingBlobAlive::URLKeepingBlobAlive(URL&& url)
    : m_url(WTFMove(url))
{
    registerBlobURLHandleIfNecessary();
}

URLKeepingBlobAlive::URLKeepingBlobAlive(const URLKeepingBlobAlive& other)
    : m_url(other.m_url)
{
    registerBlobURLHandleIfNecessary();
}

URLKeepingBlobAlive::~URLKeepingBlobAlive()
{
    unregisterBlobURLHandleIfNecessary();
}

URLKeepingBlobAlive& URLKeepingBlobAlive::operator=(URL&& url)
{
    unregisterBlobURLHandleIfNecessary();
    m_url = WTFMove(url);
    registerBlobURLHandleIfNecessary();
    return *this;
}

URLKeepingBlobAlive& URLKeepingBlobAlive::operator=(const URLKeepingBlobAlive& other)
{
    if (&other == this)
        return *this;

    unregisterBlobURLHandleIfNecessary();
    m_url = other.m_url;
    registerBlobURLHandleIfNecessary();
    return *this;
}

URLKeepingBlobAlive& URLKeepingBlobAlive::operator=(URLKeepingBlobAlive&& other)
{
    if (&other == this)
        return *this;

    unregisterBlobURLHandleIfNecessary();
    m_url = std::exchange(other.m_url, URL { });
    return *this;
}

void URLKeepingBlobAlive::registerBlobURLHandleIfNecessary()
{
    if (m_url.protocolIsBlob())
        ThreadableBlobRegistry::registerBlobURLHandle(m_url);
}

void URLKeepingBlobAlive::unregisterBlobURLHandleIfNecessary()
{
    if (m_url.protocolIsBlob())
        ThreadableBlobRegistry::unregisterBlobURLHandle(m_url);
}

URLKeepingBlobAlive URLKeepingBlobAlive::isolatedCopy() const &
{
    return { m_url.isolatedCopy() };
}

URLKeepingBlobAlive URLKeepingBlobAlive::isolatedCopy() &&
{
    return { WTFMove(m_url).isolatedCopy() };
}

} // namespace WebCore
