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

#pragma once

#if PLATFORM(COCOA)

#include <wtf/RetainPtr.h>

class AudioStreamDescription;
typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace WebCore {

WEBCORE_EXPORT bool isVorbisDecoderAvailable();
WEBCORE_EXPORT bool registerVorbisDecoderIfNeeded();
RetainPtr<CMFormatDescriptionRef> createVorbisAudioFormatDescription(size_t, const void*);

struct OpusCookieContents {
    uint8_t version { 0 };
    uint8_t channelCount { 0 };
    uint16_t preSkip { 0 };
    uint32_t sampleRate { 0 };
    int16_t outputGain { 0 };
    uint8_t mappingFamily { 0 };
    uint8_t config { 0 };
    Seconds frameDuration { 0_s };
    int32_t bandwidth { 0 };
    uint8_t framesPerPacket { 0 };
    bool isVBR { false };
    bool hasPadding { false };
};

WEBCORE_EXPORT bool isOpusDecoderAvailable();
WEBCORE_EXPORT bool registerOpusDecoderIfNeeded();
bool parseOpusPrivateData(size_t privateDataSize, const void* privateData, size_t frameDataSize, const void* frameData, OpusCookieContents&);
RetainPtr<CMFormatDescriptionRef> createOpusAudioFormatDescription(const OpusCookieContents&);

}

#endif // && PLATFORM(COCOA)
