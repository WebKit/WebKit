/*
 * Copyright (C) 2021-2022 Apple Inc. All rights reserved.
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

#include "config.h"
#include "WebGPUFragmentState.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <pal/graphics/WebGPU/WebGPUFragmentState.h>

namespace WebKit::WebGPU {

std::optional<FragmentState> ConvertToBackingContext::convertToBacking(const PAL::WebGPU::FragmentState& fragmentState)
{
    auto base = convertToBacking(static_cast<const PAL::WebGPU::ProgrammableStage&>(fragmentState));
    if (!base)
        return std::nullopt;

    Vector<std::optional<ColorTargetState>> targets;
    targets.reserveInitialCapacity(fragmentState.targets.size());
    for (const auto& target : fragmentState.targets) {
        if (target) {
            auto convertedTarget = convertToBacking(*target);
            if (!convertedTarget)
                return std::nullopt;
            targets.uncheckedAppend(WTFMove(*convertedTarget));
        } else
            targets.uncheckedAppend(std::nullopt);
    }

    return { { WTFMove(*base), WTFMove(targets) } };
}

std::optional<PAL::WebGPU::FragmentState> ConvertFromBackingContext::convertFromBacking(const FragmentState& fragmentState)
{
    auto base = convertFromBacking(static_cast<const ProgrammableStage&>(fragmentState));
    if (!base)
        return std::nullopt;

    Vector<std::optional<PAL::WebGPU::ColorTargetState>> targets;
    targets.reserveInitialCapacity(fragmentState.targets.size());
    for (const auto& backingTarget : fragmentState.targets) {
        if (backingTarget) {
            auto target = convertFromBacking(*backingTarget);
            if (!target)
                return std::nullopt;
            targets.uncheckedAppend(WTFMove(*target));
        } else
            targets.uncheckedAppend(std::nullopt);
    }

    return { { WTFMove(*base), WTFMove(targets) } };
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
