/*
 * Copyright (c) 2021-2023 Apple Inc. All rights reserved.
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

#pragma once

#include "WebGPU.h"
#include "WebGPUExt.h"
#include <Metal/Metal.h>
#include <optional>
#include <wtf/Vector.h>

namespace WebGPU {

struct HardwareCapabilities {
    WGPULimits limits { };
    Vector<WGPUFeatureName> features;

    struct BaseCapabilities {
        MTLArgumentBuffersTier argumentBuffersTier { MTLArgumentBuffersTier1 };
        bool supportsNonPrivateDepthStencilTextures { false };
        id<MTLCounterSet> timestampCounterSet { nil };
        id<MTLCounterSet> statisticCounterSet { nil };
        // FIXME: canPresentRGB10A2PixelFormats isn't actually a _hardware_ capability,
        // as all hardware can render to this format. It's unclear whether this should
        // apply to _all_ PresentationContexts or just PresentationContextCoreAnimation.
        bool canPresentRGB10A2PixelFormats { false };
    } baseCapabilities;
};

std::optional<HardwareCapabilities> hardwareCapabilities(id<MTLDevice>);
bool isValid(const WGPULimits&);
WGPULimits defaultLimits();
bool anyLimitIsBetterThan(const WGPULimits& target, const WGPULimits& reference);
bool includesUnsupportedFeatures(const Vector<WGPUFeatureName>& target, const Vector<WGPUFeatureName>& reference);

} // namespace WebGPU
