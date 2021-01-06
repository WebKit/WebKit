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
#import "WebMAudioUtilitiesCocoa.h"

#if PLATFORM(COCOA)

#import "CAAudioStreamDescription.h"
#import "Logging.h"
#import "MediaUtilities.h"
#import "PlatformMediaSessionManager.h"
#import <AudioToolbox/AudioComponent.h>
#import <AudioToolbox/AudioFormat.h>
#import <CoreMedia/CMFormatDescription.h>
#import <dlfcn.h>

namespace WebCore {

#if ENABLE(VORBIS) || ENABLE(OPUS)
static bool registerDecoderFactory(const char* decoderName, OSType decoderType)
{
    constexpr char audioComponentsDylib[] = "/System/Library/Components/AudioCodecs.component/Contents/MacOS/AudioCodecs";
    void *handle = dlopen(audioComponentsDylib, RTLD_LAZY | RTLD_LOCAL);
    if (!handle)
        return false;

    AudioComponentFactoryFunction decoderFactory = reinterpret_cast<AudioComponentFactoryFunction>(dlsym(handle, decoderName));
    if (!decoderFactory)
        return false;

    AudioComponentDescription desc { 'adec', decoderType, 'appl', kAudioComponentFlag_SandboxSafe, 0 };
    if (!AudioComponentRegister(&desc, CFSTR(""), 0, decoderFactory)) {
        dlclose(handle);
        return false;
    }

    return true;
}

static RetainPtr<CMFormatDescriptionRef> createAudioFormatDescriptionForFormat(OSType formatID, Vector<uint8_t> magicCookie)
{
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = formatID;
    uint32_t size = sizeof(asbd);
    auto error = AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, magicCookie.size(), magicCookie.data(), &size, &asbd);
    if (error) {
        RELEASE_LOG_ERROR(Media, "createAudioFormatDescriptionForFormat failed with error %d (%.4s)", error, (char *)&error);
        return nullptr;
    }

    return createAudioFormatDescription(CAAudioStreamDescription(asbd), magicCookie.size(), magicCookie.data());
}
#endif // ENABLE(VORBIS) || ENABLE(OPUS)

#if ENABLE(OPUS)
static Vector<uint8_t> cookieFromOpusCodecPrivate(size_t, const void*)
{
    return { };
}
#endif

bool isOpusDecoderAvailable()
{
    static bool available;

#if ENABLE(OPUS) && PLATFORM(MAC)
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        available = registerDecoderFactory("ACOpusDecoderFactory", 'opus');
    });
#endif

    return available;
}

RetainPtr<CMFormatDescriptionRef> createOpusAudioFormatDescription(size_t privateDataSize, const void* privateData)
{
#if ENABLE(OPUS)
    if (!isOpusDecoderAvailable())
        return nullptr;

    auto cookieData = cookieFromOpusCodecPrivate(privateDataSize, privateData);
    if (!cookieData.size())
        return nullptr;

    return createAudioFormatDescriptionForFormat('opus', cookieData);
#else
    UNUSED_PARAM(privateDataSize);
    UNUSED_PARAM(privateData);
    return nullptr;
#endif
}

#if ENABLE(VORBIS)
static Vector<uint8_t> cookieFromVorbisCodecPrivate(size_t codecPrivateSize, const void* codecPrivateData)
{
    // https://tools.ietf.org/html/draft-ietf-cellar-codec-03
    // 6.4.15. A_VORBIS
    // Codec ID: A_VORBIS
    // Codec Name: Vorbis
    // Initialization: The "Private Data" contains the first three Vorbis packet in order. The lengths of the
    // packets precedes them. The actual layout is:
    // - Byte 1: number of distinct packets '"#p"' minus one inside the CodecPrivate block.This MUST be '2'
    //   for current (as of 2016-07-08) Vorbis headers.
    // - Bytes 2..n: lengths of the first '"#p"' packets, coded in Xiph-style lacing. The length of the last
    //   packet is the length of the CodecPrivate block minus the lengths coded in these bytes minus one.
    // - Bytes n+1..: The Vorbis identification header, followed by the Vorbis comment header followed by the
    //   codec setup header.

    const unsigned char* privateDataPtr = static_cast<const unsigned char*>(codecPrivateData);

    const int vorbisMinimumCookieSize = 3;
    if (codecPrivateSize < vorbisMinimumCookieSize) {
        RELEASE_LOG_ERROR(Media, "cookieFromVorbisCodecPrivate: codec private data too small (%zu)", codecPrivateSize);
        return { };
    }

    // Despite the "This MUST be '2'" comment in the IETF document, packet count is not always equal to
    // 2 in real-word files, so ignore that field.
    const uint16_t idHeaderSize = privateDataPtr[1];
    const uint16_t commentHeaderSize = privateDataPtr[2];
    const uint16_t calculatedHeaderSize = 1 + idHeaderSize + commentHeaderSize;
    if (1 + idHeaderSize + commentHeaderSize > codecPrivateSize) {
        RELEASE_LOG_ERROR(Media, "cookieFromVorbisCodecPrivate: codec private data too small (%zu) for header sizes (%d)", codecPrivateSize, calculatedHeaderSize);
        return { };
    }

    const unsigned char* idHeader = &privateDataPtr[3];
    const unsigned char* commentHeader = idHeader + idHeaderSize;
    const unsigned char* codecSetupHeader = commentHeader + commentHeaderSize;
    const uint16_t codecSetupHeaderSize = codecPrivateSize - idHeaderSize - commentHeaderSize - 2 - 1;

    if ((idHeaderSize + commentHeaderSize + codecSetupHeaderSize + 3) > codecPrivateSize) {
        RELEASE_LOG_ERROR(Media, "cookieFromVorbisCodecPrivate: codec private header size is invalid - id = %d, comment = %d, header = %d, chunk size = %zu", idHeaderSize, commentHeaderSize, codecSetupHeaderSize, codecPrivateSize);
        return { };
    }

    Vector<uint8_t> cookieData;
    cookieData.append(reinterpret_cast_ptr<const uint8_t*>(&idHeaderSize), sizeof(idHeaderSize));
    cookieData.append(idHeader, idHeaderSize);

    cookieData.append(reinterpret_cast_ptr<const uint8_t*>(&commentHeaderSize), sizeof(commentHeaderSize));
    cookieData.append(commentHeader, commentHeaderSize);

    cookieData.append(reinterpret_cast_ptr<const uint8_t*>(&codecSetupHeaderSize), sizeof(codecSetupHeaderSize));
    cookieData.append(codecSetupHeader, codecSetupHeaderSize);

    return cookieData;
}
#endif // ENABLE(VORBIS)

bool isVorbisDecoderAvailable()
{
    static bool available;

#if ENABLE(VORBIS) && PLATFORM(MAC)
    if (!PlatformMediaSessionManager::vorbisDecoderEnabled())
        return false;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        available = registerDecoderFactory("ACVorbisDecoderFactory", 'vorb');
    });
#endif

    return available;
}

RetainPtr<CMFormatDescriptionRef> createVorbisAudioFormatDescription(size_t privateDataSize, const void* privateData)
{
#if ENABLE(VORBIS)
    if (!isVorbisDecoderAvailable())
        return nullptr;

    auto cookieData = cookieFromVorbisCodecPrivate(privateDataSize, privateData);
    if (!cookieData.size())
        return nullptr;

    return createAudioFormatDescriptionForFormat('vorb', cookieData);
#else
    UNUSED_PARAM(privateDataSize);
    UNUSED_PARAM(privateData);
    return nullptr;
#endif
}

}

#endif // PLATFORM(COCOA)
