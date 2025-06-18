/*
 * Copyright (C) 2008-2025 Apple Inc. All rights reserved.
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
#include "ApplicationCacheResource.h"
#include <stdio.h>

namespace WebCore {

Ref<ApplicationCacheResource> ApplicationCacheResource::create(URL&& url, ResourceResponse&& response, unsigned type, RefPtr<FragmentedSharedBuffer>&& buffer, String&& path)
{
    ASSERT(!url.hasFragmentIdentifier());
    if (!buffer)
        buffer = SharedBuffer::create();
    auto resourceResponse = WTFMove(response);
    resourceResponse.setSource(ResourceResponse::Source::ApplicationCache);

    return adoptRef(*new ApplicationCacheResource(WTFMove(url), WTFMove(resourceResponse), type, buffer.releaseNonNull(), WTFMove(path)));
}

ApplicationCacheResource::ApplicationCacheResource(URL&& url, ResourceResponse&& response, unsigned type, Ref<FragmentedSharedBuffer>&& data, String&& path)
    : SubstituteResource(WTFMove(url), WTFMove(response), WTFMove(data))
    , m_type(type)
    , m_storageID(0)
    , m_estimatedSizeInStorage(0)
    , m_path(WTFMove(path))
{
}

void ApplicationCacheResource::deliver(ResourceLoader& loader)
{
    if (m_path.isEmpty())
        loader.deliverResponseAndData(ResourceResponse { response() }, RefPtr { &data() });
    else
        loader.deliverResponseAndData(ResourceResponse { response() }, SharedBuffer::createWithContentsOfFile(m_path));
}

void ApplicationCacheResource::addType(unsigned type) 
{
    // Caller should take care of storing the new type in database.
    m_type |= type; 
}

int64_t ApplicationCacheResource::estimatedSizeInStorage()
{
    if (m_estimatedSizeInStorage)
      return m_estimatedSizeInStorage;

    m_estimatedSizeInStorage = data().size();

    for (const auto& headerField : response().httpHeaderFields())
        m_estimatedSizeInStorage += (headerField.key.length() + headerField.value.length() + 2) * sizeof(char16_t);

    m_estimatedSizeInStorage += url().string().length() * sizeof(char16_t);
    m_estimatedSizeInStorage += sizeof(int); // response().m_httpStatusCode
    m_estimatedSizeInStorage += response().url().string().length() * sizeof(char16_t);
    m_estimatedSizeInStorage += sizeof(unsigned); // dataId
    m_estimatedSizeInStorage += response().mimeType().length() * sizeof(char16_t);
    m_estimatedSizeInStorage += response().textEncodingName().length() * sizeof(char16_t);

    return m_estimatedSizeInStorage;
}

#ifndef NDEBUG
void ApplicationCacheResource::dumpType(unsigned type)
{
    if (type & Master)
        printf("master ");
    if (type & Manifest)
        printf("manifest ");
    if (type & Explicit)
        printf("explicit ");
    if (type & Foreign)
        printf("foreign ");
    if (type & Fallback)
        printf("fallback ");
    
    printf("\n");
}
#endif

} // namespace WebCore
