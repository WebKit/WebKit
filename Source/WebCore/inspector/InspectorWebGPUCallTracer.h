/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include "InspectorCanvasProcessedArguments.h"
#include <type_traits>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class GPUDevice;

// Records WebGPU encoder/queue calls for Web Inspector, routing each to the InspectorCanvas(es) recording the traced object's GPUDevice; arguments are best-effort (scalars/strings kept, objects elided).
class InspectorWebGPUCallTracer {
public:
    using ProcessedArgument = InspectorCanvasProcessedArgument;
    using ProcessedArguments = InspectorCanvasProcessedArguments;

    template<typename IDLType, typename ImplType, typename ArgumentType>
    static std::optional<ProcessedArgument> processArgument(ImplType&, ArgumentType&& argument)
    {
        using Decayed = std::decay_t<ArgumentType>;
        if constexpr (std::is_same_v<Decayed, bool>)
            return { { JSON::Value::create(argument), RecordingSwizzleType::Boolean } };
        else if constexpr (std::is_enum_v<Decayed>)
            return { { JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number } };
        else if constexpr (std::is_arithmetic_v<Decayed>)
            return { { JSON::Value::create(static_cast<double>(argument)), RecordingSwizzleType::Number } };
        else if constexpr (std::is_same_v<Decayed, String>)
            return { { JSON::Value::create(argument), RecordingSwizzleType::String } };
        else
            return std::nullopt;
    }

    template<typename ImplType>
    static void recordAction(ImplType& impl, String&& name, ProcessedArguments&& arguments = { })
    {
        recordActionForDevice(impl.inspectorRecordingDevice(), WTF::move(name), WTF::move(arguments));
    }

private:
    static void recordActionForDevice(GPUDevice*, String&&, ProcessedArguments&&);
};

} // namespace WebCore
