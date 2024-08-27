/*
 * Copyright (C) 2016-2021 Apple Inc. All rights reserved.
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
#import "WebMAudioUtilitiesCocoa.h"
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/AudioToolboxSPI.h>
#import <wtf/SortedArrayMap.h>
#import <wtf/text/MakeString.h>

#import <pal/cf/AudioToolboxSoftLink.h>
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

#if ENABLE(VIDEO) && USE(AVFOUNDATION) && ENABLE(OPUS)
static bool isMultichannelOpusAvailable()
{
    static bool isMultichannelOpusAvailable = [] {
        if (!isOpusDecoderAvailable())
            return false;

        AudioStreamBasicDescription asbd { };
        asbd.mFormatID = kAudioFormatOpus;

        // AvailableDecodeChannelLayoutTags is an array of AudioChannelLayoutTag objects
        UInt32 propertySize = 0;
        auto error = PAL::AudioFormatGetPropertyInfo(kAudioFormatProperty_AvailableDecodeChannelLayoutTags, sizeof(asbd), &asbd, &propertySize);
        if (error != noErr || propertySize < sizeof(AudioChannelLayoutTag))
            return false;

        size_t count = propertySize / sizeof(AudioChannelLayoutTag);
        Vector<AudioChannelLayoutTag> channelLayoutTags(count, { });

        error = PAL::AudioFormatGetProperty(kAudioFormatProperty_AvailableDecodeChannelLayoutTags, sizeof(asbd), &asbd, &propertySize, channelLayoutTags.data());
        if (error != noErr)
            return false;

        size_t maximumDecodeChannelCount = 0;
        for (auto& channelLayoutTag : channelLayoutTags) {
            UInt32 layoutIndicator = (channelLayoutTag & 0xFFFF0000);
            if (layoutIndicator == kAudioChannelLayoutTag_Unknown || layoutIndicator == kAudioChannelLayoutTag_DiscreteInOrder)
                continue;
            maximumDecodeChannelCount = std::max<size_t>(maximumDecodeChannelCount, AudioChannelLayoutTag_GetNumberOfChannels(channelLayoutTag));
        }

        return maximumDecodeChannelCount > 2;
    }();
    return isMultichannelOpusAvailable;
}
#endif

bool AVAssetMIMETypeCache::canDecodeExtendedType(const ContentType& typeParameter)
{
    ContentType type = typeParameter;
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
#if ENABLE(OPUS)
    // Disclaim support for 'opus' if multi-channel decode is not available.
    if ((type.containerType() == "video/mp4"_s || type.containerType() == "audio/mp4"_s)
        && type.codecs().contains("opus"_s) && !isMultichannelOpusAvailable())
        return false;
#endif

    // Some platforms will disclaim support for 'flac', and only support the MP4RA registered `fLaC`
    // codec string for flac, so convert the former to the latter before querying.
    if ((type.containerType() == "video/mp4"_s || type.containerType() == "audio/mp4"_s)
        && type.codecs().contains("flac"_s))
        type = ContentType(makeStringByReplacingAll(type.raw(), "flac"_s, "fLaC"_s));

    ASSERT(isAvailable());

#if HAVE(AVURLASSET_ISPLAYABLEEXTENDEDMIMETYPEWITHOPTIONS)
    if (PAL::canLoad_AVFoundation_AVURLAssetExtendedMIMETypePlayabilityTreatPlaylistMIMETypesAsISOBMFFMediaDataContainersKey()
        && [PAL::getAVURLAssetClass() respondsToSelector:@selector(isPlayableExtendedMIMEType:options:)]) {
        if ([PAL::getAVURLAssetClass() isPlayableExtendedMIMEType:type.raw() options:@{ AVURLAssetExtendedMIMETypePlayabilityTreatPlaylistMIMETypesAsISOBMFFMediaDataContainersKey: @YES }])
            return true;
    } else
#endif
    if ([PAL::getAVURLAssetClass() isPlayableExtendedMIMEType:type.raw()])
        return true;

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
    if (!lowerCaseType.startsWith("video/"_s) && !lowerCaseType.startsWith("audio/"_s) && !lowerCaseType.startsWith("application/"_s))
        return true;

    // Reject types we know AVFoundation does not support that sites commonly ask about.
    static constexpr ComparableASCIILiteral unsupportedTypesArray[] = { "video/h264", "video/x-flv" };
    static constexpr SortedArraySet unsupportedTypesSet { unsupportedTypesArray };
    return unsupportedTypesSet.contains(lowerCaseType);
}

bool AVAssetMIMETypeCache::isStaticContainerType(StringView type)
{
    static constexpr ComparableLettersLiteral staticContainerTypesArray[] = {
        "application/vnd.apple.mpegurl",
        "application/x-mpegurl",
        "audio/3gpp",
        "audio/aac",
        "audio/aacp",
        "audio/aiff",
        "audio/basic",
        "audio/mp3",
        "audio/mp4",
        "audio/mpeg",
        "audio/mpeg3",
        "audio/mpegurl",
        "audio/mpg",
        "audio/vnd.wave",
        "audio/wav",
        "audio/wave",
        "audio/x-aac",
        "audio/x-aiff",
        "audio/x-m4a",
        "audio/x-mpegurl",
        "audio/x-wav",
        "video/3gpp",
        "video/3gpp2",
        "video/mp4",
        "video/mpeg",
        "video/mpeg2",
        "video/mpg",
        "video/quicktime",
        "video/x-m4v",
        "video/x-mpeg",
        "video/x-mpg",
    };
    static constexpr SortedArraySet staticContainerTypesSet { staticContainerTypesArray };
    return staticContainerTypesSet.contains(type);
}

void AVAssetMIMETypeCache::addSupportedTypes(const Vector<String>& types)
{
    MIMETypeCache::addSupportedTypes(types);
    if (m_cacheTypeCallback)
        m_cacheTypeCallback(types);
}

void AVAssetMIMETypeCache::initializeCache(HashSet<String>& cache)
{
#if ENABLE(VIDEO) && USE(AVFOUNDATION)
    if (!isAvailable())
        return;

    for (NSString *type in [PAL::getAVURLAssetClass() audiovisualMIMETypes])
        cache.add(type);

    if (m_cacheTypeCallback)
        m_cacheTypeCallback(copyToVector(cache));
#endif
}

}

#endif
