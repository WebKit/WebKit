/*
 * Copyright (C) 2021-2023 Apple Inc. All rights reserved.
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#include "GPUDevice.h"
#include "InspectorCanvas.h"
#include "InspectorCanvasArguments.h"
#include "InspectorCanvasProcessedArguments.h"
#include <type_traits>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class InspectorCanvasCallTracer {
public:
    using ProcessedArgument = InspectorCanvasProcessedArgument;
    using ProcessedArguments = InspectorCanvasProcessedArguments;

    template<typename IDLType, typename ArgumentType>
    static std::optional<ProcessedArgument> processArgument(CanvasRenderingContext& canvasRenderingContext, ArgumentType&& argument)
    {
        RefPtr inspectorCanvas = enabledInspectorCanvas(canvasRenderingContext);
        if (!inspectorCanvas)
            return std::nullopt;

        using Processor = InspectorCanvasArgumentProcessor<IDLType>;
        return Processor{}(*inspectorCanvas, std::forward<ArgumentType>(argument));
    }

    template<typename IDLType, typename ArgumentType>
    static std::optional<ProcessedArgument> processArgument(const CanvasBase& canvasBase, ArgumentType&& argument)
    {
        Ref renderingContext = *canvasBase.renderingContext();
        return processArgument<IDLType>(renderingContext, std::forward<ArgumentType>(argument));
    }

    template<typename IDLType, typename ArgumentType>
    static std::optional<ProcessedArgument> processArgument(GPUDevice& device, ArgumentType&& argument)
    {
        RefPtr inspectorCanvas = enabledInspectorCanvas(device);
        if (!inspectorCanvas)
            return std::nullopt;

        if constexpr (WTF::IsTemplate<IDLType, IDLNullable>::value || WTF::IsTemplate<IDLType, IDLOptional>::value) {
            if (!argument)
                return std::nullopt;
            return processArgument<typename IDLType::InnerType>(device, *argument);
        }
        if constexpr (IsIDLInterface<IDLType>::value) {
            using InterfaceType = typename IDLType::RawType;
            if constexpr (requires (InterfaceType& object) { object.device(); }) {
                using Processor = InspectorCanvasArgumentProcessor<IDLType>;
                return Processor { }(*inspectorCanvas, std::forward<ArgumentType>(argument));
            }
        }
        if constexpr (IsIDLString<IDLType>::value)
            return InspectorCanvasArgumentProcessor<IDLDOMString> { }(*inspectorCanvas, std::forward<ArgumentType>(argument));
        if constexpr (std::is_same_v<IDLType, IDLBoolean> || IsIDLNumber<IDLType>::value || IsIDLEnumeration<IDLType>::value) {
            using Processor = InspectorCanvasArgumentProcessor<IDLType>;
            return Processor { }(*inspectorCanvas, std::forward<ArgumentType>(argument));
        }
        return std::nullopt;
    }

    template<typename IDLType, typename Receiver, typename ArgumentType>
        requires (requires (Receiver& receiver) { receiver.context(); })
    static std::optional<ProcessedArgument> processArgument(Receiver& receiver, ArgumentType&& argument)
    {
        RefPtr context = receiver.context();
        if (!context)
            return std::nullopt;
        return processArgument<IDLType>(*context, std::forward<ArgumentType>(argument));
    }

    template<typename IDLType, typename Receiver, typename ArgumentType>
        requires (requires (Receiver& receiver) { receiver.device(); })
    static std::optional<ProcessedArgument> processArgument(Receiver& receiver, ArgumentType&& argument)
    {
        RefPtr device = receiver.device();
        if (!device)
            return std::nullopt;
        return processArgument<IDLType>(*device, std::forward<ArgumentType>(argument));
    }

    static void recordAction(CanvasRenderingContext&, String&&, ProcessedArguments&& = { });
    static void recordAction(const CanvasBase&, String&&, ProcessedArguments&& = { });
    static void recordAction(GPUDevice&, String&&, ProcessedArguments&& = { });

    template<typename Receiver>
        requires (requires (Receiver& receiver) { receiver.context(); })
    static void recordAction(Receiver& receiver, String&& name, ProcessedArguments&& arguments = { })
    {
        RefPtr context = receiver.context();
        if (!context)
            return;

        auto processedReceiver = processArgument<IDLInterface<std::remove_cvref_t<Receiver>>>(*context, protect(receiver));
        if (!processedReceiver)
            return;

        recordAction(*context, WTF::move(*processedReceiver), WTF::move(name), WTF::move(arguments));
    }

    template<typename Receiver>
        requires (requires (Receiver& receiver) { receiver.device(); })
    static void recordAction(Receiver& receiver, String&& name, ProcessedArguments&& arguments = { })
    {
        RefPtr device = receiver.device();
        if (!device)
            return;

        auto processedReceiver = processArgument<IDLInterface<std::remove_cvref_t<Receiver>>>(*device, protect(receiver));
        if (!processedReceiver)
            return;

        recordAction(*device, WTF::move(*processedReceiver), WTF::move(name), WTF::move(arguments));
    }

private:
    static RefPtr<InspectorCanvas> enabledInspectorCanvas(CanvasRenderingContext&);
    static RefPtr<InspectorCanvas> enabledInspectorCanvas(GPUDevice&);
    static void recordAction(CanvasRenderingContext&, ProcessedArgument&& receiver, String&&, ProcessedArguments&&);
    static void recordAction(GPUDevice&, ProcessedArgument&& receiver, String&&, ProcessedArguments&&);
};

} // namespace WebCore
