/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "AVStreamDataParserMIMETypeCache.h"

#if ENABLE(MEDIA_SOURCE) && USE(AVFOUNDATION)

#import "AVAssetMIMETypeCache.h"
#import "ContentType.h"
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/HashSet.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

#if !PLATFORM(MACCATALYST)
SOFT_LINK_FRAMEWORK_OPTIONAL_PREFLIGHT(AVFoundation)
#endif

NS_ASSUME_NONNULL_BEGIN
@interface AVStreamDataParser (AVStreamDataParserExtendedMIMETypes)
+ (BOOL)canParseExtendedMIMEType:(NSString *)extendedMIMEType;
@end
NS_ASSUME_NONNULL_END

namespace WebCore {

AVStreamDataParserMIMETypeCache& AVStreamDataParserMIMETypeCache::singleton()
{
    static NeverDestroyed<AVStreamDataParserMIMETypeCache> cache;
    return cache.get();
}

const HashSet<String, ASCIICaseInsensitiveHash>& AVStreamDataParserMIMETypeCache::types()
{
    if (!m_cache)
        loadMIMETypes();

    return *m_cache;
}

bool AVStreamDataParserMIMETypeCache::supportsContentType(const ContentType& contentType)
{
    if (contentType.isEmpty())
        return false;

    return types().contains(contentType.containerType());
}

bool AVStreamDataParserMIMETypeCache::canDecodeType(const String& mimeType)
{
    if (mimeType.isEmpty())
        return false;

    ContentType type { mimeType };

    if (!isAvailable() || !types().contains(type.containerType()))
        return false;

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    if ([PAL::getAVStreamDataParserClass() respondsToSelector:@selector(canParseExtendedMIMEType:)])
        return [PAL::getAVStreamDataParserClass() canParseExtendedMIMEType:mimeType];

    // FIXME(rdar://50502771) AVStreamDataParser does not have an -canParseExtendedMIMEType: method on this system,
    //  so just replace the container type with a valid one from AVAssetMIMETypeCache and ask that cache if it
    //  can decode this type.
    auto& assetCache = AVAssetMIMETypeCache::singleton();
    if (!assetCache.isAvailable() || assetCache.types().isEmpty())
        return false;
    String replacementType { mimeType };
    replacementType.replace(type.containerType(), *assetCache.types().begin());
    return assetCache.canDecodeType(replacementType);
#endif

    return false;
}

bool AVStreamDataParserMIMETypeCache::isAvailable() const
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#if PLATFORM(MACCATALYST)
    // FIXME: This should be using AVFoundationLibraryIsAvailable() instead, but doing so causes soft-linking
    // to subsequently fail on certain symbols. See <rdar://problem/42224780> for more details.
    if (!PAL::AVFoundationLibrary())
        return false;
#else
    if (!AVFoundationLibraryIsAvailable())
        return false;
#endif

    return [PAL::getAVStreamDataParserClass() respondsToSelector:@selector(audiovisualMIMETypes)];
#else
    return false;
#endif
}

void AVStreamDataParserMIMETypeCache::loadMIMETypes()
{
    m_cache = HashSet<String, ASCIICaseInsensitiveHash>();

#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    static std::once_flag onceFlag;
    std::call_once(onceFlag, [this] {
        if (!isAvailable())
            return;

        for (NSString* type in [PAL::getAVStreamDataParserClass() audiovisualMIMETypes])
            m_cache->add(type);
    });
#endif
}

}

#endif
