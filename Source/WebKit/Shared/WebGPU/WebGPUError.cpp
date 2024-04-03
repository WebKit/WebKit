/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
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
#include "WebGPUError.h"

#if ENABLE(GPU_PROCESS)

#include "WebGPUConvertFromBackingContext.h"
#include "WebGPUConvertToBackingContext.h"
#include <WebCore/WebGPUError.h>

namespace WebKit::WebGPU {

std::optional<Error> ConvertToBackingContext::convertToBacking(const WebCore::WebGPU::Error& error)
{
    return WTF::switchOn(error, [this] (const Ref<WebCore::WebGPU::OutOfMemoryError>& outOfMemoryError) -> std::optional<Error> {
        auto result = convertToBacking(outOfMemoryError.get());
        if (!result)
            return std::nullopt;
        return { { *result } };
    }, [this] (const Ref<WebCore::WebGPU::ValidationError>& validationError) -> std::optional<Error> {
        auto result = convertToBacking(validationError.get());
        if (!result)
            return std::nullopt;
        return { { *result } };
    }, [this] (const Ref<WebCore::WebGPU::InternalError>& internalError) -> std::optional<Error> {
        auto result = convertToBacking(internalError.get());
        if (!result)
            return std::nullopt;
        return { { *result } };
    });
}

std::optional<WebCore::WebGPU::Error> ConvertFromBackingContext::convertFromBacking(const Error& error)
{
    return WTF::switchOn(error, [this] (const OutOfMemoryError& outOfMemoryError) -> std::optional<WebCore::WebGPU::Error> {
        auto result = convertFromBacking(outOfMemoryError);
        if (!result)
            return std::nullopt;
        return { result.releaseNonNull() };
    }, [this] (const ValidationError& validationError) -> std::optional<WebCore::WebGPU::Error> {
        auto result = convertFromBacking(validationError);
        if (!result)
            return std::nullopt;
        return { result.releaseNonNull() };
    }, [this] (const InternalError& internalError) -> std::optional<WebCore::WebGPU::Error> {
        auto result = convertFromBacking(internalError);
        if (!result)
            return std::nullopt;
        return { result.releaseNonNull() };
    });
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
