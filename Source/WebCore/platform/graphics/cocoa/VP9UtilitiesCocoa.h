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

#if ENABLE(VP9) && PLATFORM(COCOA)

#include "VP9Utilities.h"
#include <webm/dom_types.h>

typedef const struct opaqueCMFormatDescription* CMFormatDescriptionRef;

namespace vp9_parser {
class Vp9HeaderParser;
}

namespace WebCore {

struct MediaCapabilitiesInfo;
struct VideoConfiguration;

WEBCORE_EXPORT extern void setOverrideVP9HardwareDecoderDisabledForTesting(bool);
WEBCORE_EXPORT extern void setOverrideVP9ScreenSizeAndScaleForTesting(float width, float height, float scale);
WEBCORE_EXPORT extern void resetOverrideVP9ScreenSizeAndScaleForTesting();

WEBCORE_EXPORT extern void registerWebKitVP9Decoder();
WEBCORE_EXPORT extern void registerWebKitVP8Decoder();
WEBCORE_EXPORT extern void registerSupplementalVP9Decoder();
extern bool isVP9DecoderAvailable();
extern bool isVP8DecoderAvailable();
extern bool isVPCodecConfigurationRecordSupported(VPCodecConfigurationRecord&);
extern bool validateVPParameters(VPCodecConfigurationRecord&, MediaCapabilitiesInfo&, const VideoConfiguration&);

RetainPtr<CMFormatDescriptionRef> createFormatDescriptionFromVP9HeaderParser(const vp9_parser::Vp9HeaderParser&, const webm::Element<webm::Colour>&);

struct VP8FrameHeader {
    bool keyframe { false };
    uint8_t version { 0 };
    bool showFrame { true };
    uint32_t partitionSize { 0 };
    uint8_t horizontalScale { 0 };
    uint16_t width { 0 };
    uint8_t verticalScale { 0 };
    uint16_t height;
    bool colorSpace { false };
    bool needsClamping { false };
};

Optional<VP8FrameHeader> parseVP8FrameHeader(uint8_t* frameData, size_t frameSize);
RetainPtr<CMFormatDescriptionRef> createFormatDescriptionFromVP8Header(const VP8FrameHeader&, const webm::Element<webm::Colour>&);

}

#endif
