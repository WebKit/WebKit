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

#if ENABLE(WEB_CODECS)

#include "BufferSource.h"
#include "HardwareAcceleration.h"
#include "VideoColorSpaceInit.h"
#include <optional>

namespace WebCore {

struct WebCodecsVideoDecoderConfig {
    String codec;
    std::optional<BufferSource::VariantType> description;
    std::optional<size_t> codedWidth;
    std::optional<size_t> codedHeight;
    std::optional<size_t> displayAspectWidth;
    std::optional<size_t> displayAspectHeight;
    std::optional<VideoColorSpaceInit> colorSpace;
    HardwareAcceleration hardwareAcceleration { HardwareAcceleration::NoPreference };
    std::optional<bool> optimizeForLatency;

    WebCodecsVideoDecoderConfig isolatedCopyWithoutDescription() && { return { WTFMove(codec).isolatedCopy(), { }, codedWidth, codedHeight, displayAspectWidth, displayAspectHeight, colorSpace, hardwareAcceleration, optimizeForLatency }; }
    WebCodecsVideoDecoderConfig isolatedCopyWithoutDescription() const & { return { codec.isolatedCopy(), { }, codedWidth, codedHeight, displayAspectWidth, displayAspectHeight, colorSpace, hardwareAcceleration, optimizeForLatency }; }
};

}

#endif
