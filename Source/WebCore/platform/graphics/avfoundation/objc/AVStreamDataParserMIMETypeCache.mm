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
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/HashSet.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

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

bool AVStreamDataParserMIMETypeCache::isAvailable() const
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    if (!PAL::AVFoundationLibrary())
        return false;

    return [PAL::getAVStreamDataParserClass() respondsToSelector:@selector(audiovisualMIMETypes)];
#else
    return false;
#endif
}

MediaPlayerEnums::SupportsType AVStreamDataParserMIMETypeCache::canDecodeType(const String& type)
{
    if (isAvailable())
        return MIMETypeCache::canDecodeType(type);

    auto& assetCache = AVAssetMIMETypeCache::singleton();
    if (assetCache.isAvailable())
        return assetCache.canDecodeType(type);

    return MediaPlayerEnums::SupportsType::IsNotSupported;
}

HashSet<String, ASCIICaseInsensitiveHash>& AVStreamDataParserMIMETypeCache::supportedTypes()
{
    if (isAvailable())
        return MIMETypeCache::supportedTypes();

    auto& assetCache = AVAssetMIMETypeCache::singleton();
    if (assetCache.isAvailable())
        return assetCache.supportedTypes();

    return MIMETypeCache::supportedTypes();
}

bool AVStreamDataParserMIMETypeCache::canDecodeExtendedType(const ContentType& type)
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    ASSERT(isAvailable());

    if ([PAL::getAVStreamDataParserClass() respondsToSelector:@selector(canParseExtendedMIMEType:)])
        return [PAL::getAVStreamDataParserClass() canParseExtendedMIMEType:type.raw()];

    // FIXME(rdar://50502771) AVStreamDataParser does not have an -canParseExtendedMIMEType: method on this system,
    //  so just replace the container type with a valid one from AVAssetMIMETypeCache and ask that cache if it
    //  can decode this type.
    auto& assetCache = AVAssetMIMETypeCache::singleton();
    if (!assetCache.isAvailable() || assetCache.supportedTypes().isEmpty())
        return false;

    String replacementType { type.raw() };
    replacementType.replace(type.containerType(), *assetCache.supportedTypes().begin());
    return assetCache.canDecodeType(replacementType) == MediaPlayerEnums::SupportsType::IsSupported;
#endif

    return false;
}

void AVStreamDataParserMIMETypeCache::initializeCache(HashSet<String, ASCIICaseInsensitiveHash>& cache)
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    if (!isAvailable())
        return;

    for (NSString* type in [PAL::getAVStreamDataParserClass() audiovisualMIMETypes])
        cache.add(type);
#endif
}

}

#endif
