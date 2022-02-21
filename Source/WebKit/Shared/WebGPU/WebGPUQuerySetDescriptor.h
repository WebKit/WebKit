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

#pragma once

#if ENABLE(GPU_PROCESS)

#include "WebGPUObjectDescriptorBase.h"
#include <optional>
#include <pal/graphics/WebGPU/WebGPUIntegralTypes.h>
#include <pal/graphics/WebGPU/WebGPUQueryType.h>
#include <wtf/Vector.h>

namespace WebKit::WebGPU {

struct QuerySetDescriptor : public ObjectDescriptorBase {
    PAL::WebGPU::QueryType type { PAL::WebGPU::QueryType::Occlusion };
    PAL::WebGPU::Size32 count { 0 };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << static_cast<const ObjectDescriptorBase&>(*this);
        encoder << type;
        encoder << count;
    }

    template<class Decoder> static std::optional<QuerySetDescriptor> decode(Decoder& decoder)
    {
        std::optional<ObjectDescriptorBase> objectDescriptorBase;
        decoder >> objectDescriptorBase;
        if (!objectDescriptorBase)
            return std::nullopt;

        std::optional<PAL::WebGPU::QueryType> type;
        decoder >> type;
        if (!type)
            return std::nullopt;

        std::optional<PAL::WebGPU::Size32> count;
        decoder >> count;
        if (!count)
            return std::nullopt;

        return { { WTFMove(*objectDescriptorBase), WTFMove(*type), WTFMove(*count) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
