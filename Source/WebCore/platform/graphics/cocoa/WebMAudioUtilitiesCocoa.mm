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
#import <AudioToolbox/AudioCodec.h>
#import <AudioToolbox/AudioComponent.h>
#import <AudioToolbox/AudioFormat.h>
#import <CoreMedia/CMFormatDescription.h>
#import <dlfcn.h>
#import <wtf/FlipBytes.h>
#import <wtf/Seconds.h>
#if ENABLE(OPUS)
#import <libwebrtc/opus_defines.h>
#endif
#import <pal/cf/AudioToolboxSoftLink.h>

namespace WebCore {

#if ENABLE(VORBIS) || ENABLE(OPUS)
static bool registerDecoderFactory(const char* decoderName, OSType decoderType)
{
    AudioComponentDescription desc { kAudioDecoderComponentType, decoderType, 'appl', kAudioComponentFlag_SandboxSafe, 0 };
    AudioComponent comp = PAL::AudioComponentFindNext(0, &desc);
    if (comp)
        return true; // Already registered.

    constexpr char audioComponentsDylib[] = "/System/Library/Components/AudioCodecs.component/Contents/MacOS/AudioCodecs";
    void *handle = dlopen(audioComponentsDylib, RTLD_LAZY | RTLD_LOCAL);
    if (!handle)
        return false;

    AudioComponentFactoryFunction decoderFactory = reinterpret_cast<AudioComponentFactoryFunction>(dlsym(handle, decoderName));
    if (!decoderFactory)
        return false;

    if (!AudioComponentRegister(&desc, CFSTR(""), 0, decoderFactory)) {
        dlclose(handle);
        return false;
    }

    return true;
}

static RetainPtr<CMFormatDescriptionRef> createAudioFormatDescriptionForFormat(OSType formatID, Vector<uint8_t>&& magicCookie)
{
    AudioStreamBasicDescription asbd { };
    asbd.mFormatID = formatID;
    uint32_t size = sizeof(asbd);
    auto error = PAL::AudioFormatGetProperty(kAudioFormatProperty_FormatInfo, magicCookie.size(), magicCookie.data(), &size, &asbd);
    if (error) {
        RELEASE_LOG_ERROR(Media, "createAudioFormatDescriptionForFormat failed with error %d (%.4s)", error, (char *)&error);
        return nullptr;
    }

    return createAudioFormatDescription(CAAudioStreamDescription(asbd), magicCookie.size(), magicCookie.data());
}
#endif // ENABLE(VORBIS) || ENABLE(OPUS)

#if ENABLE(OPUS)
constexpr Seconds opusConfigToFrameDuration(uint8_t config)
{
    // Refer to https://tools.ietf.org/html/rfc6716#section-3:
    // Section 3.1. The TOC Byte
    // Table 2: TOC Byte Configuration Parameters
    // Column 4: Frame Sizes
    constexpr Seconds frameSizeArray[] = {
        10_ms,
        20_ms,
        40_ms,
        60_ms,
        10_ms,
        20_ms,
        40_ms,
        60_ms,
        10_ms,
        20_ms,
        40_ms,
        60_ms,
        10_ms,
        20_ms,
        10_ms,
        20_ms,
        2.5_ms,
        5_ms,
        10_ms,
        20_ms,
        2.5_ms,
        5_ms,
        10_ms,
        20_ms,
        2.5_ms,
        5_ms,
        10_ms,
        20_ms,
        2.5_ms,
        5_ms,
        10_ms,
        20_ms,
    };
    if (config < std::size(frameSizeArray))
        return frameSizeArray[config];

    ASSERT_NOT_REACHED();
    return 1_ms;
}

constexpr int32_t opusConfigToBandwidth(uint8_t config)
{
    // Refer to https://tools.ietf.org/html/rfc6716#section-3:
    // Section 3.1. The TOC Byte
    // Table 2: TOC Byte Configuration Parameters
    // Column 3: Bandwidth
    if (config <= 3)
        return OPUS_BANDWIDTH_NARROWBAND;
    if (config <= 7)
        return OPUS_BANDWIDTH_MEDIUMBAND;
    if (config <= 11)
        return OPUS_BANDWIDTH_WIDEBAND;
    if (config <= 13)
        return OPUS_BANDWIDTH_SUPERWIDEBAND;
    if (config <= 15)
        return OPUS_BANDWIDTH_FULLBAND;
    if (config <= 19)
        return OPUS_BANDWIDTH_NARROWBAND;
    if (config <= 23)
        return OPUS_BANDWIDTH_WIDEBAND;
    if (config <= 27)
        return OPUS_BANDWIDTH_SUPERWIDEBAND;
    if (config <= 31)
        return OPUS_BANDWIDTH_FULLBAND;

    ASSERT_NOT_REACHED();
    return 0;
}
#endif

bool parseOpusPrivateData(size_t codecPrivateSize, const void* codecPrivateData, size_t frameSize, const void* frameData, OpusCookieContents& cookie)
{
#if ENABLE(OPUS)
    // https://tools.ietf.org/html/rfc7845
    // 5. Header Packets
    //
    //    An Ogg Opus logical stream contains exactly two mandatory header
    //    packets: an identification header and a comment header.
    //
    // 5.1. Identification Header
    //
    //       0                   1                   2                   3
    //       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //      |      'O'      |      'p'      |      'u'      |      's'      |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //      |      'H'      |      'e'      |      'a'      |      'd'      |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //      |  Version = 1  | Channel Count |           Pre-skip            |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //      |                     Input Sample Rate (Hz)                    |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    //      |   Output Gain (Q7.8 in dB)    | Mapping Family|               |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+               :
    //      |                                                               |
    //      |               Optional Channel Mapping Table                  |
    //      |                                                               |
    //      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

    auto* privateDataPtr = static_cast<const uint8_t*>(codecPrivateData);

    if (codecPrivateSize < 19)
        return { };

    // 1. Magic Signature:
    //
    //     This is an 8-octet (64-bit) field that allows codec
    //     identification and is human readable.
    if (strncmp("OpusHead", reinterpret_cast<const char*>(privateDataPtr), 8))
        return false;

    // 2. Version (8 bits, unsigned):
    cookie.version = *(privateDataPtr + 8);

    // 3. Output Channel Count 'C' (8 bits, unsigned):
    cookie.channelCount = *(privateDataPtr + 9);

    // 4. Pre-skip (16 bits, unsigned, little endian):
    cookie.preSkip = flipBytesIfLittleEndian(*reinterpret_cast<const uint16_t*>(privateDataPtr + 10), true);

    // 5. Input Sample Rate (32 bits, unsigned, little endian):
    cookie.sampleRate = flipBytesIfLittleEndian(*reinterpret_cast<const uint32_t*>(privateDataPtr + 12), true);

    // 6. Output Gain (16 bits, signed, little endian):
    cookie.outputGain = flipBytesIfLittleEndian(*reinterpret_cast<const int16_t*>(privateDataPtr + 16), true);

    // 7. Channel Mapping Family (8 bits, unsigned):
    cookie.mappingFamily = *(privateDataPtr + 18);

    auto framePtr = static_cast<const uint8_t*>(frameData);

    if (frameSize < 1)
        return false;

    // https://tools.ietf.org/html/rfc6716
    // 3.1. The TOC Byte
    //
    //    A well-formed Opus packet MUST contain at least one byte [R1]. This
    //    byte forms a table-of-contents (TOC) header that signals which of the
    //    various modes and configurations a given packet uses. It is composed
    //    of a configuration number, "config", a stereo flag, "s", and a frame
    //    count code, "c", arranged as illustrated in Figure 1. A description
    //    of each of these fields follows.
    //                          0
    //                          0 1 2 3 4 5 6 7
    //                         +-+-+-+-+-+-+-+-+
    //                         | config  |s| c |
    //                         +-+-+-+-+-+-+-+-+
    //                       Figure 1: The TOC Byte

    uint8_t tocByte = *framePtr;
    uint8_t config = (tocByte & 0b11111000) >> 3;
    cookie.frameDuration = opusConfigToFrameDuration(config);
    cookie.bandwidth = opusConfigToBandwidth(config);

    uint8_t frameCountValue = (tocByte & 0b00000011);

    cookie.framesPerPacket = 0;
    if (!frameCountValue)
        cookie.framesPerPacket = 1;
    else if (frameCountValue == 1 || frameCountValue == 2)
        cookie.framesPerPacket = 2;
    else if (frameCountValue == 3) {
        // 3.2.5. Code 3: A Signaled Number of Frames in the Packet
        //
        //    Code 3 packets signal the number of frames, as well as additional
        //    padding, called "Opus padding" to indicate that this padding is added
        //    at the Opus layer rather than at the transport layer. Code 3 packets
        //    MUST have at least 2 bytes [R6,R7]. The TOC byte is followed by a
        //    byte encoding the number of frames in the packet in bits 2 to 7
        //    (marked "M" in Figure 5), with bit 1 indicating whether or not Opus
        //    padding is inserted (marked "p" in Figure 5), and bit 0 indicating
        //    VBR (marked "v" in Figure 5). M MUST NOT be zero, and the audio
        //    duration contained within a packet MUST NOT exceed 120 ms [R5]. This
        //    limits the maximum frame count for any frame size to 48 (for 2.5 ms
        //    frames), with lower limits for longer frame sizes. Figure 5
        //    illustrates the layout of the frame count byte.
        //
        //                               0
        //                               0 1 2 3 4 5 6 7
        //                              +-+-+-+-+-+-+-+-+
        //                              |v|p|     M     |
        //                              +-+-+-+-+-+-+-+-+
        //
        //                       Figure 5: The frame count byte
        if (frameSize < 2)
            return false;

        uint8_t frameCountByte = *(framePtr + 1);
        cookie.isVBR = ((frameCountByte & 0b10000000) >> 7) == 1;
        cookie.hasPadding = ((frameCountByte & 0b01000000) >> 6) == 1;
        cookie.framesPerPacket = (frameCountByte & 0b00111111);
    }
    return true;
#else
    UNUSED_PARAM(codecPrivateSize);
    UNUSED_PARAM(codecPrivateData);
    UNUSED_PARAM(frameSize);
    UNUSED_PARAM(frameData);
    UNUSED_PARAM(cookie);
    return false;
#endif
}

#if ENABLE(OPUS)
static Vector<uint8_t> cookieFromOpusCookieContents(const OpusCookieContents& cookie)
{
    auto samplesPerPacket = cookie.framesPerPacket * (cookie.frameDuration.seconds() * cookie.sampleRate);

    struct CoreAudioOpusHeader {
        unsigned applicationID;
        unsigned sampleRate;
        unsigned packetFrameSize;
        int packetBandwidth;
        unsigned packetStreamChannels;
        unsigned useInbandFEC;
        unsigned encFinalRange { 0 };
    };

    CoreAudioOpusHeader header = {
        CFSwapInt32HostToBig(OPUS_APPLICATION_AUDIO),
        CFSwapInt32HostToBig(cookie.sampleRate),
        CFSwapInt32HostToBig(samplesPerPacket),
        static_cast<int>(CFSwapInt32HostToBig(cookie.bandwidth)),
        CFSwapInt32HostToBig(cookie.channelCount),
        CFSwapInt32HostToBig(1),
    };

    auto magicCookie = Vector<uint8_t>(sizeof(CoreAudioOpusHeader));
    *reinterpret_cast<CoreAudioOpusHeader*>(magicCookie.data()) = header;

    return magicCookie;
}
#endif

bool isOpusDecoderAvailable()
{
#if ENABLE(OPUS) && PLATFORM(MAC)
    if (!PlatformMediaSessionManager::opusDecoderEnabled())
        return false;

    return registerOpusDecoderIfNeeded();
#else
    return false;
#endif
}

bool registerOpusDecoderIfNeeded()
{
#if ENABLE(OPUS) && PLATFORM(MAC)
    static bool available;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        available = registerDecoderFactory("ACOpusDecoderFactory", kAudioFormatOpus);
    });

    return available;
#else
    return false;
#endif
}

RetainPtr<CMFormatDescriptionRef> createOpusAudioFormatDescription(const OpusCookieContents& cookieContents)
{
#if ENABLE(OPUS)
    if (!isOpusDecoderAvailable())
        return nullptr;

    auto cookieData = cookieFromOpusCookieContents(cookieContents);
    if (!cookieData.size())
        return nullptr;

    return createAudioFormatDescriptionForFormat('opus', WTFMove(cookieData));
#else
    UNUSED_PARAM(cookieContents);
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
#if ENABLE(VORBIS) && PLATFORM(MAC)
    if (!PlatformMediaSessionManager::vorbisDecoderEnabled())
        return false;

    return registerVorbisDecoderIfNeeded();
#else
    return false;
#endif
}

bool registerVorbisDecoderIfNeeded()
{
#if ENABLE(VORBIS) && PLATFORM(MAC)
    static bool available;

    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        available = registerDecoderFactory("ACVorbisDecoderFactory", 'vorb');
    });

    return available;
#else
    return false;
#endif
}

RetainPtr<CMFormatDescriptionRef> createVorbisAudioFormatDescription(size_t privateDataSize, const void* privateData)
{
#if ENABLE(VORBIS)
    if (!isVorbisDecoderAvailable())
        return nullptr;

    auto cookieData = cookieFromVorbisCodecPrivate(privateDataSize, privateData);
    if (!cookieData.size())
        return nullptr;

    return createAudioFormatDescriptionForFormat('vorb', WTFMove(cookieData));
#else
    UNUSED_PARAM(privateDataSize);
    UNUSED_PARAM(privateData);
    return nullptr;
#endif
}

}

#endif // PLATFORM(COCOA)
