/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "NicosiaContentLayerTextureMapperImpl.h"

#if USE(TEXTURE_MAPPER)

#include "TextureMapperPlatformLayerProxy.h"

namespace Nicosia {

auto ContentLayerTextureMapperImpl::createFactory(Client& client) -> Factory
{
    return Factory(
        [&client](ContentLayer&) {
            return makeUnique<ContentLayerTextureMapperImpl>(client);
        });
}

ContentLayerTextureMapperImpl::ContentLayerTextureMapperImpl(Client& client)
    : m_proxy(adoptRef(*new WebCore::TextureMapperPlatformLayerProxy))
    , m_client { { }, &client }
{
}

ContentLayerTextureMapperImpl::~ContentLayerTextureMapperImpl()
{
#if !ASSERT_DISABLED
    LockHolder locker(m_client.lock);
    ASSERT(!m_client.client);
#endif
}

void ContentLayerTextureMapperImpl::invalidateClient()
{
    LockHolder locker(m_client.lock);
    m_client.client = nullptr;
}

bool ContentLayerTextureMapperImpl::flushUpdate()
{
    LockHolder locker(m_client.lock);
    return std::exchange(m_client.pendingUpdate, false);
}

void ContentLayerTextureMapperImpl::swapBuffersIfNeeded()
{
    LockHolder locker(m_client.lock);
    if (m_client.client)
        m_client.client->swapBuffersIfNeeded();
}

ContentLayerTextureMapperImpl::Client::~Client() = default;

} // namespace Nicosia

#endif // USE(TEXTURE_MAPPER)
