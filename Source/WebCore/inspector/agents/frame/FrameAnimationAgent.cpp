/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "FrameAnimationAgent.h"

#include "Document.h"
#include "FrameDOMAgent.h"
#include "FrameDestructionObserverInlines.h"
#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameAnimationAgent);

FrameAnimationAgent::FrameAnimationAgent(FrameAgentContext& context)
    : InspectorAnimationAgent(context)
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameAnimationAgent::~FrameAnimationAgent() = default;

bool FrameAnimationAgent::matchesCurrentContext(ScriptExecutionContext* scriptExecutionContext) const
{
    auto* document = dynamicDowncast<Document>(scriptExecutionContext);
    return document && document->frame() == m_inspectedFrame.ptr();
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::Styleable>> FrameAnimationAgent::pushStyleablePathToFrontend(const Styleable& styleable)
{
    CheckedPtr domAgent = protect(m_instrumentingAgents)->persistentFrameDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    return domAgent->pushStyleablePathToFrontend(styleable);
}

Inspector::Protocol::DOM::NodeId FrameAnimationAgent::pushStyleableElementToFrontend(const Styleable& styleable)
{
    CheckedPtr domAgent = protect(m_instrumentingAgents)->persistentFrameDOMAgent();
    if (!domAgent)
        return 0;

    return domAgent->pushStyleableElementToFrontend(styleable);
}

} // namespace WebCore
