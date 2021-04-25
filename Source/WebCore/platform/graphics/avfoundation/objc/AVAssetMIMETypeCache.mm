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
#import "AVAssetMIMETypeCache.h"

#if PLATFORM(COCOA)

#import "ContentType.h"
#import "SourceBufferParserWebM.h"
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

AVAssetMIMETypeCache& AVAssetMIMETypeCache::singleton()
{
    static NeverDestroyed<AVAssetMIMETypeCache> cache;
    return cache.get();
}

bool AVAssetMIMETypeCache::isAvailable() const
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    return PAL::isAVFoundationFrameworkAvailable();
#else
    return false;
#endif
}

bool AVAssetMIMETypeCache::canDecodeExtendedType(const ContentType& type)
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    ASSERT(isAvailable());
    if ([PAL::getAVURLAssetClass() isPlayableExtendedMIMEType:type.raw()])
        return true;

#if ENABLE(WEBM_FORMAT_READER)
    if (SourceBufferParserWebM::isContentTypeSupported(type) == MediaPlayerEnums::SupportsType::IsSupported)
        return true;
#endif

#endif // ENABLE(VIDEO) && USE(AVFOUNDATION)

    return false;
}

bool AVAssetMIMETypeCache::isUnsupportedContainerType(const String& type)
{
    if (type.isEmpty())
        return false;

    String lowerCaseType = type.convertToASCIILowercase();

    // AVFoundation will return non-video MIME types which it claims to support, but which we
    // do not support in the <video> element. Reject all non video/, audio/, and application/ types.
    if (!lowerCaseType.startsWith("video/") && !lowerCaseType.startsWith("audio/") && !lowerCaseType.startsWith("application/"))
        return true;

    // Reject types we know AVFoundation does not support that sites commonly ask about.
    if (lowerCaseType == "video/x-flv")
        return true;

    if (lowerCaseType == "audio/ogg" || lowerCaseType == "video/ogg" || lowerCaseType == "application/ogg")
        return true;

    if (lowerCaseType == "video/h264")
        return true;

    return false;
}

const HashSet<String, ASCIICaseInsensitiveHash>& AVAssetMIMETypeCache::staticContainerTypeList()
{
    static const auto cache = makeNeverDestroyed(HashSet<String, ASCIICaseInsensitiveHash> {
        "application/vnd.apple.mpegurl"_s,
        "application/x-mpegurl"_s,
        "audio/3gpp"_s,
        "audio/aac"_s,
        "audio/aacp"_s,
        "audio/aiff"_s,
        "audio/basic"_s,
        "audio/mp3"_s,
        "audio/mp4"_s,
        "audio/mpeg"_s,
        "audio/mpeg3"_s,
        "audio/mpegurl"_s,
        "audio/mpg"_s,
        "audio/vnd.wave"_s,
        "audio/wav"_s,
        "audio/wave"_s,
        "audio/x-aac"_s,
        "audio/x-aiff"_s,
        "audio/x-m4a"_s,
        "audio/x-mpegurl"_s,
        "audio/x-wav"_s,
        "video/3gpp"_s,
        "video/3gpp2"_s,
        "video/mp4"_s,
        "video/mpeg"_s,
        "video/mpeg2"_s,
        "video/mpg"_s,
        "video/quicktime"_s,
        "video/x-m4v"_s,
        "video/x-mpeg"_s,
        "video/x-mpg"_s,
    });
    return cache;
}

void AVAssetMIMETypeCache::addSupportedTypes(const Vector<String>& types)
{
    MIMETypeCache::addSupportedTypes(types);
    if (m_cacheTypeCallback)
        m_cacheTypeCallback(types);
}

void AVAssetMIMETypeCache::initializeCache(HashSet<String, ASCIICaseInsensitiveHash>& cache)
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    if (!isAvailable())
        return;

    for (NSString* type in [PAL::getAVURLAssetClass() audiovisualMIMETypes])
        cache.add(type);

#if ENABLE(WEBM_FORMAT_READER)
    if (SourceBufferParserWebM::isWebMFormatReaderAvailable()) {
        auto webmTypes = SourceBufferParserWebM::webmMIMETypes();
        cache.add(webmTypes.begin(), webmTypes.end());
    }
#endif

    if (m_cacheTypeCallback)
        m_cacheTypeCallback(copyToVector(cache));
#endif
}

}

#endif
