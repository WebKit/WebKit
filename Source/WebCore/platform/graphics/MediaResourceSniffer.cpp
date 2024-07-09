/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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
#include "MediaResourceSniffer.h"

#if ENABLE(VIDEO)

#include "MIMESniffer.h"
#include "ResourceRequest.h"
#include <limits.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

Ref<MediaResourceSniffer> MediaResourceSniffer::create(PlatformMediaResourceLoader& loader, ResourceRequest&& request, std::optional<size_t> maxSize)
{
    if (maxSize)
        request.addHTTPHeaderField(HTTPHeaderName::Range, makeString("bytes="_s, 0, '-', *maxSize));
    auto resource = loader.requestResource(WTFMove(request), PlatformMediaResourceLoader::LoadOption::DisallowCaching);
    if (!resource)
        return adoptRef(*new MediaResourceSniffer());
    Ref sniffer = adoptRef(*new MediaResourceSniffer(*resource , maxSize.value_or(SIZE_MAX)));
    resource->setClient(sniffer.copyRef());
    return sniffer;
}

MediaResourceSniffer::MediaResourceSniffer()
    : m_maxSize(0)
{
    m_producer.reject(PlatformMediaError::NetworkError);
}

MediaResourceSniffer::MediaResourceSniffer(Ref<PlatformMediaResource>&& resource, size_t maxSize)
    : m_resource(WTFMove(resource))
    , m_maxSize(maxSize)
{
}

MediaResourceSniffer::~MediaResourceSniffer()
{
    cancel();
}

void MediaResourceSniffer::cancel()
{
    if (auto resource = std::exchange(m_resource, { }))
        resource->shutdown();
    if (!m_producer.isSettled())
        m_producer.reject(PlatformMediaError::Cancelled);
    m_content.reset();
}

MediaResourceSniffer::Promise& MediaResourceSniffer::promise() const
{
    return m_producer.promise().get();
}

void MediaResourceSniffer::dataReceived(PlatformMediaResource&, const SharedBuffer& buffer)
{
    m_received += buffer.size();
    m_content.append(buffer);
    auto contiguousBuffer = m_content.get()->makeContiguous();
    auto mimeType = MIMESniffer::getMIMETypeFromContent(contiguousBuffer->span());
    if (mimeType.isEmpty() && m_received < m_maxSize)
        return;
    if (!m_producer.isSettled())
        m_producer.resolve(ContentType { WTFMove(mimeType) });
    cancel();
}

void MediaResourceSniffer::loadFailed(PlatformMediaResource&, const ResourceError&)
{
    if (!m_producer.isSettled())
        m_producer.reject(PlatformMediaError::NetworkError);
    cancel();
}

void MediaResourceSniffer::loadFinished(PlatformMediaResource&, const NetworkLoadMetrics&)
{
    if (m_producer.isSettled())
        return;
    Ref contiguousBuffer = m_content.takeAsContiguous();
    auto mimeType = MIMESniffer::getMIMETypeFromContent(contiguousBuffer->span());
    m_producer.resolve(ContentType { WTFMove(mimeType) });
    cancel();
}

} // namespace WebCore

#endif // ENABLE(VIDEO)
