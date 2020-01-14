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

#include "RemoteMediaPlayerManagerProxyMessages.h"
#include <WebCore/MediaPlayerPrivate.h>

#if PLATFORM(COCOA)
#include <WebCore/AVAssetMIMETypeCache.h>
#endif

namespace WebKit {
using namespace WebCore;

RemoteMediaPlayerMIMETypeCache::RemoteMediaPlayerMIMETypeCache(RemoteMediaPlayerManager& manager, MediaPlayerEnums::MediaEngineIdentifier engineIdentifier)
    : m_manager(manager)
    , m_engineIdentifier(engineIdentifier)
{
}

MIMETypeCache* RemoteMediaPlayerMIMETypeCache::mimeCache() const
{
    switch (m_engineIdentifier) {
    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundation:
#if PLATFORM(COCOA)
        return &AVAssetMIMETypeCache::singleton();
        break;
#endif

    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMSE:
    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundationMediaStream:
    case MediaPlayerEnums::MediaEngineIdentifier::AVFoundationCF:
    case MediaPlayerEnums::MediaEngineIdentifier::GStreamer:
    case MediaPlayerEnums::MediaEngineIdentifier::GStreamerMSE:
    case MediaPlayerEnums::MediaEngineIdentifier::HolePunch:
    case MediaPlayerEnums::MediaEngineIdentifier::MediaFoundation:
    case MediaPlayerEnums::MediaEngineIdentifier::MockMSE:
        ASSERT_NOT_REACHED();
        break;
    }

    return nullptr;
}

const HashSet<String, ASCIICaseInsensitiveHash>& RemoteMediaPlayerMIMETypeCache::staticContainerTypeList()
{
    if (auto* mimeCache = this->mimeCache())
        return mimeCache->staticContainerTypeList();

    return MIMETypeCache::staticContainerTypeList();
}

bool RemoteMediaPlayerMIMETypeCache::isUnsupportedContainerType(const String& type)
{
    if (auto* mimeCache = this->mimeCache())
        return mimeCache->isUnsupportedContainerType(type);

    return false;
}

bool RemoteMediaPlayerMIMETypeCache::canDecodeExtendedType(const WebCore::ContentType& type)
{
    bool result;
    if (!m_manager.gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::CanDecodeExtendedType(m_engineIdentifier, type.raw()), Messages::RemoteMediaPlayerManagerProxy::CanDecodeExtendedType::Reply(result), 0))
        return false;

    return result;
}

WebCore::MediaPlayerEnums::SupportsType RemoteMediaPlayerMIMETypeCache::supportsTypeAndCodecs(const WebCore::MediaEngineSupportParameters& parameters)
{
    if (parameters.type.raw().isEmpty())
        return MediaPlayerEnums::SupportsType::MayBeSupported;

    if (parameters.contentTypesRequiringHardwareSupport.isEmpty())
        return canDecodeType(parameters.type.raw());

    if (m_supportsTypeAndCodecsCache) {
        auto it = m_supportsTypeAndCodecsCache->find(parameters.type.raw());
        if (it != m_supportsTypeAndCodecsCache->end())
            return it->value;
    }

    MediaPlayer::SupportsType result;
    if (!m_manager.gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::SupportsTypeAndCodecs(m_engineIdentifier, parameters), Messages::RemoteMediaPlayerManagerProxy::SupportsTypeAndCodecs::Reply(result), 0))
        return MediaPlayer::SupportsType::IsNotSupported;

    if (!m_supportsTypeAndCodecsCache)
        m_supportsTypeAndCodecsCache = HashMap<String, MediaPlayerEnums::SupportsType, ASCIICaseInsensitiveHash>();
    m_supportsTypeAndCodecsCache->add(parameters.type.raw(), result);

    return result;
}

void RemoteMediaPlayerMIMETypeCache::initializeCache(HashSet<String, ASCIICaseInsensitiveHash>& cache)
{
    auto* mimeCache = this->mimeCache();
    if (!isEmpty() || !mimeCache)
        return;

    Vector<String> types;
    if (!m_manager.gpuProcessConnection().connection().sendSync(Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes(m_engineIdentifier), Messages::RemoteMediaPlayerManagerProxy::GetSupportedTypes::Reply(types), 0))
        return;

    for (auto& type : types)
        cache.add(type);

    mimeCache->addSupportedTypes(types);
}

}

#endif // ENABLE(GPU_PROCESS)
