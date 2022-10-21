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

#include "Logging.h"
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
    m_supportedTypesCache.add(newTypes.begin(), newTypes.end());
}

bool RemoteMediaPlayerMIMETypeCache::isEmpty() const
{
    return m_hasPopulatedSupportedTypesCacheFromGPUProcess && m_supportedTypesCache.isEmpty();
}

HashSet<String, ASCIICaseInsensitiveHash>& RemoteMediaPlayerMIMETypeCache::supportedTypes()
{
    ASSERT(isMainRunLoop());
    if (!m_hasPopulatedSupportedTypesCacheFromGPUProcess) {
        auto sendResult = m_manager.gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes(m_engineIdentifier), 0);
        if (sendResult) {
            auto& [types] = sendResult.reply();
            addSupportedTypes(types);
            m_hasPopulatedSupportedTypesCacheFromGPUProcess = true;
        } else
            RELEASE_LOG_ERROR(Media, "RemoteMediaPlayerMIMETypeCache::supportedTypes: Sync IPC to the GPUProcess failed.");
    }
    return m_supportedTypesCache;
}

MediaPlayerEnums::SupportsType RemoteMediaPlayerMIMETypeCache::supportsTypeAndCodecs(const MediaEngineSupportParameters& parameters)
{
    if (parameters.type.raw().isEmpty())
        return MediaPlayerEnums::SupportsType::MayBeSupported;

    SupportedTypesAndCodecsKey searchKey { parameters.type.raw(), parameters.isMediaSource, parameters.isMediaStream, parameters.requiresRemotePlayback };

    if (m_supportsTypeAndCodecsCache) {
        auto it = m_supportsTypeAndCodecsCache->find(searchKey);
        if (it != m_supportsTypeAndCodecsCache->end())
            return it->value;
    }

    if (!m_supportsTypeAndCodecsCache)
        m_supportsTypeAndCodecsCache = HashMap<SupportedTypesAndCodecsKey, MediaPlayerEnums::SupportsType> { };

    auto sendResult = m_manager.gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::SupportsTypeAndCodecs(m_engineIdentifier, parameters), 0);
    auto [result] = sendResult.takeReplyOr(MediaPlayerEnums::SupportsType::IsNotSupported);
    if (sendResult)
        m_supportsTypeAndCodecsCache->add(searchKey, result);

    return result;
}

}

#endif // ENABLE(GPU_PROCESS)
