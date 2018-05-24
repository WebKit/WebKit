/*
 * Copyright (C) 2016-2018 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AVFoundationMIMETypeCache.h"

#if PLATFORM(COCOA)

#import "ContentType.h"
#import <AVFoundation/AVAsset.h>
#import <wtf/HashSet.h>

#import <pal/cf/CoreMediaSoftLink.h>

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
SOFT_LINK_FRAMEWORK_OPTIONAL_PREFLIGHT(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVURLAsset)
#endif

namespace WebCore {

AVFoundationMIMETypeCache& AVFoundationMIMETypeCache::singleton()
{
    static NeverDestroyed<AVFoundationMIMETypeCache> cache;
    return cache.get();
}

void AVFoundationMIMETypeCache::setSupportedTypes(const Vector<String>& types)
{
    if (m_cache)
        return;

    m_cache = HashSet<String, ASCIICaseInsensitiveHash>();
    for (auto& type : types)
        m_cache->add(type);
}

const HashSet<String, ASCIICaseInsensitiveHash>& AVFoundationMIMETypeCache::types()
{
    if (!m_cache)
        loadMIMETypes();

    return *m_cache;
}

bool AVFoundationMIMETypeCache::supportsContentType(const ContentType& contentType)
{
    if (contentType.isEmpty())
        return false;

    return types().contains(contentType.containerType());
}

bool AVFoundationMIMETypeCache::canDecodeType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;

    if (!isAvailable() || !types().contains(ContentType { mimeType }.containerType()))
        return false;

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    return [getAVURLAssetClass() isPlayableExtendedMIMEType:mimeType];
#endif

    return false;
}

bool AVFoundationMIMETypeCache::isAvailable() const
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    return AVFoundationLibraryIsAvailable();
#else
    return false;
#endif
}

void AVFoundationMIMETypeCache::loadMIMETypes()
{
    m_cache = HashSet<String, ASCIICaseInsensitiveHash>();

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [this] {
        if (!AVFoundationLibrary())
            return;

        for (NSString* type in [getAVURLAssetClass() audiovisualMIMETypes])
            m_cache->add(type);

        if (m_cacheTypeCallback)
            m_cacheTypeCallback(copyToVector(*m_cache));
    });
#endif
}

}

#endif
