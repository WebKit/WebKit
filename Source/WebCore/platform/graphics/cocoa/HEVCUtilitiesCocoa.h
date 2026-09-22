/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include <wtf/Platform.h>

#if PLATFORM(COCOA)

#include <WebCore/HEVCUtilities.h>
#include <wtf/Forward.h>
#include <wtf/Vector.h>

typedef struct opaqueCMSampleBuffer *CMSampleBufferRef;

namespace WebCore {

struct PlatformMediaCapabilitiesInfo;
class VideoInfo;

WEBCORE_EXPORT std::optional<PlatformMediaCapabilitiesInfo> validateHEVCParameters(const HEVCParameters&, bool hasAlphaChannel, bool hdrSupport);
std::optional<PlatformMediaCapabilitiesInfo> validateDoViParameters(const DoViParameters&, bool hasAlphaChannel, bool hdrSupport);

WEBCORE_EXPORT Vector<uint8_t> convertHEVCCMSampleBufferToAnnexB(CMSampleBufferRef, bool isKeyframe);

// Look for a leading VPS+SPS+PPS triplet in an HEVC Annex B chunk. If found, returns a VideoInfo describing it.
WEBCORE_EXPORT RefPtr<VideoInfo> createVideoInfoFromHEVCAnnexBStream(std::span<const uint8_t>, const HEVCAnnexBNaluIndices&);

// Converts an HEVC Annex B chunk into hvcC-style length-prefixed NAL units suitable for a CMSampleBuffer.
WEBCORE_EXPORT Vector<uint8_t> convertHEVCAnnexBToLengthPrefixed(std::span<const uint8_t>, const HEVCAnnexBNaluIndices&);

// Parses an HEVC decoder configuration record ("hvcC" box) directly into a VideoInfo, deriving
// width/height from the embedded parameter sets. Returns nullptr on failure.
WEBCORE_EXPORT RefPtr<VideoInfo> createVideoInfoFromHVCC(std::span<const uint8_t>, const HVCCParameterSets&);

}

#endif
