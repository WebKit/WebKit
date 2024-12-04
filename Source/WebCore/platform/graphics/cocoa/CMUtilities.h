/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#pragma once

#if PLATFORM(COCOA)

#include <CoreAudio/CoreAudioTypes.h>
#include <memory>
#include <wtf/Expected.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

typedef struct AudioFormatVorbisModeInfo AudioFormatVorbisModeInfo;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;
typedef struct opaqueCMSampleBuffer* CMSampleBufferRef;
typedef struct __CVBuffer* CVPixelBufferRef;

namespace WebCore {

class MediaSamplesBlock;
class SharedBuffer;
struct AudioInfo;
struct PlatformVideoColorSpace;
struct TrackInfo;

WEBCORE_EXPORT RetainPtr<CMFormatDescriptionRef> createFormatDescriptionFromTrackInfo(const TrackInfo&);
// Convert MediaSamplesBlock to the equivalent CMSampleBufferRef. If CMFormatDescriptionRef
// is set it will be used, otherwise it will be created from the MediaSamplesBlock's TrackInfo.
WEBCORE_EXPORT Expected<RetainPtr<CMSampleBufferRef>, CString> toCMSampleBuffer(MediaSamplesBlock&&, CMFormatDescriptionRef = nullptr);

WEBCORE_EXPORT void attachColorSpaceToPixelBuffer(const PlatformVideoColorSpace&, CVPixelBufferRef);

class PacketDurationParser final {
    WTF_MAKE_TZONE_ALLOCATED(PacketDurationParser);
public:
    explicit PacketDurationParser(const AudioInfo&);
    ~PacketDurationParser();

    bool isValid() const { return m_isValid; }
    size_t framesInPacket(SharedBuffer&);
    void reset();

private:
    uint32_t m_audioFormatID { 0 };
    uint32_t m_constantFramesPerPacket { 0 };
    std::optional<Seconds> m_frameDuration;
    uint32_t m_sampleRate { 0 };
#if HAVE(AUDIOFORMATPROPERTY_VARIABLEPACKET_SUPPORTED)
    std::unique_ptr<AudioFormatVorbisModeInfo> m_vorbisModeInfo;
#endif
    uint32_t m_vorbisModeMask { 0 };
    uint32_t m_lastVorbisBlockSize { 0 };
    bool m_isValid { false };
};

Vector<AudioStreamPacketDescription> getPacketDescriptions(CMSampleBufferRef);

}

#endif
