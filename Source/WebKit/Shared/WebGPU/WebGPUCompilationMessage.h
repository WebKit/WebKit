/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <cstdint>
#include <optional>
#include <pal/graphics/WebGPU/WebGPUCompilationMessageType.h>
#include <wtf/text/WTFString.h>

namespace WebKit::WebGPU {

struct CompilationMessage {
    String message;
    PAL::WebGPU::CompilationMessageType type { PAL::WebGPU::CompilationMessageType::Error };
    uint64_t lineNum { 0 };
    uint64_t linePos { 0 };
    uint64_t offset { 0 };
    uint64_t length { 0 };

    template<class Encoder> void encode(Encoder& encoder) const
    {
        encoder << message;
        encoder << type;
        encoder << lineNum;
        encoder << linePos;
        encoder << offset;
        encoder << length;
    }

    template<class Decoder> static std::optional<CompilationMessage> decode(Decoder& decoder)
    {
        std::optional<String> message;
        decoder >> message;
        if (!message)
            return std::nullopt;

        std::optional<PAL::WebGPU::CompilationMessageType> type;
        decoder >> type;
        if (!type)
            return std::nullopt;

        std::optional<uint64_t> lineNum;
        decoder >> lineNum;
        if (!lineNum)
            return std::nullopt;

        std::optional<uint64_t> linePos;
        decoder >> linePos;
        if (!linePos)
            return std::nullopt;

        std::optional<uint64_t> offset;
        decoder >> offset;
        if (!offset)
            return std::nullopt;

        std::optional<uint64_t> length;
        decoder >> length;
        if (!length)
            return std::nullopt;

        return { { WTFMove(*message), WTFMove(*type), WTFMove(*lineNum), WTFMove(*linePos), WTFMove(*offset), WTFMove(*length) } };
    }
};

} // namespace WebKit::WebGPU

#endif // ENABLE(GPU_PROCESS)
