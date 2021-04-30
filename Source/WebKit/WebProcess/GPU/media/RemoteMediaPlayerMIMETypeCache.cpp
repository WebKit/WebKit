/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "RemoteMediaPlayerMIMETypeCache.h"

#if ENABLE(GPU_PROCESS)

#include "RemoteMediaPlayerManager.h"
#include "RemoteMediaPlayerManagerProxyMessages.h"
#include <wtf/Vector.h>

namespace WebKit {
using namespace WebCore;

RemoteMediaPlayerMIMETypeCache::RemoteMediaPlayerMIMETypeCache(RemoteMediaPlayerManager& manager, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier)
    : m_manager(manager)
    , m_engineIdentifier(engineIdentifier)
{
}

void RemoteMediaPlayerMIMETypeCache::addSupportedTypes(const Vector<String>& newTypes)
{
    if (!m_supportedTypesCache)
        m_supportedTypesCache = HashSet<String, ASCIICaseInsensitiveHash> { };

    for (auto& type : newTypes)
        m_supportedTypesCache->add(type);
}

bool RemoteMediaPlayerMIMETypeCache::isEmpty() const
{
    return m_supportedTypesCache && m_supportedTypesCache->isEmpty();
}

HashSet<String, ASCIICaseInsensitiveHash>& RemoteMediaPlayerMIMETypeCache::supportedTypes()
{
    if (m_supportedTypesCache)
        return *m_supportedTypesCache;

    Vector<String> types;
    if (m_manager.gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes(m_engineIdentifier), Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes::Reply(types), 0))
        addSupportedTypes(types);

    return *m_supportedTypesCache;
}

MediaPlayerEnums::SupportsType RemoteMediaPlayerMIMETypeCache::supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters)
{
    if (parameters.type.raw().isEmpty())
        return MediaPlayerEnums::SupportsType::MayBeSupported;

    SupportedTypesAndCodecsKey searchKey { parameters.type.raw(), parameters.isMediaSource, parameters.isMediaStream };

    if (m_supportsTypeAndCodecsCache) {
        auto it = m_supportsTypeAndCodecsCache->find(searchKey);
        if (it != m_supportsTypeAndCodecsCache->end())
            return it->value;
    }

    if (!m_supportsTypeAndCodecsCache)
        m_supportsTypeAndCodecsCache = HashMap<SupportedTypesAndCodecsKey, MediaPlayerEnums::SupportsType> { };

    MediaPlayerEnums::SupportsType result = MediaPlayerEnums::SupportsType::IsNotSupported;
    if (m_manager.gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::SupportsTypeAndCodecs(m_engineIdentifier, parameters), Messages::RemoteMediaPlayerManagerProxy::SupportsTypeAndCodecs::Reply(result), 0))
        m_supportsTypeAndCodecsCache->add(searchKey, result);

    return result;
}

}

#endif // ENABLE(GPU_PROCESS)
