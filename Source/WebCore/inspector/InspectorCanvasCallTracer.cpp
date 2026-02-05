/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "config.h"
#include "InspectorCanvasCallTracer.h"

#include "CanvasBase.h"
#include "CanvasRenderingContext.h"
#include "InspectorCanvasAgent.h"
#include "InspectorInstrumentation.h"
#include "InstrumentingAgents.h"

namespace WebCore {

static InspectorCanvasAgent* enabledCanvasAgent(CanvasRenderingContext& canvasRenderingContext)
{
    ASSERT(InspectorInstrumentationPublic::hasFrontends());

    RefPtr agents = InspectorInstrumentation::instrumentingAgents(canvasRenderingContext.canvasBase().protectedScriptExecutionContext().get());
    ASSERT(agents);
    if (!agents)
        return nullptr;

    ASSERT(agents->enabledCanvasAgent());
    return agents->enabledCanvasAgent();
}

RefPtr<InspectorCanvas> InspectorCanvasCallTracer::enabledInspectorCanvas(CanvasRenderingContext& canvasRenderingContext)
{
    auto* canvasAgent = enabledCanvasAgent(canvasRenderingContext);
    if (!canvasAgent)
        return nullptr;
    return canvasAgent->findInspectorCanvas(canvasRenderingContext);
}

void InspectorCanvasCallTracer::recordAction(CanvasRenderingContext& canvasRenderingContext, String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    if (auto* canvasAgent = enabledCanvasAgent(canvasRenderingContext))
        canvasAgent->recordAction(canvasRenderingContext, WTF::move(name), WTF::move(arguments));
}

void InspectorCanvasCallTracer::recordAction(const CanvasBase& canvasBase, String&& name, InspectorCanvasProcessedArguments&& arguments)
{
    ASSERT(canvasBase.renderingContext());
    recordAction(*canvasBase.renderingContext(), WTF::move(name), WTF::move(arguments));
}

} // namespace WebCore
