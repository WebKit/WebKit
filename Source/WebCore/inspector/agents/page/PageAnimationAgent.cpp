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
#include "PageAnimationAgent.h"

#include "Document.h"
#include "DocumentPage.h"
#include "InspectorDOMAgent.h"
#include "InstrumentingAgents.h"
#include "Page.h"
#include "Styleable.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageAnimationAgent);

PageAnimationAgent::PageAnimationAgent(PageAgentContext& context)
    : InspectorAnimationAgent(context)
    , m_inspectedPage(context.inspectedPage)
{
}

PageAnimationAgent::~PageAnimationAgent() = default;

bool PageAnimationAgent::matchesCurrentContext(ScriptExecutionContext* scriptExecutionContext) const
{
    // FIXME: <https://webkit.org/b/168475> Web Inspector: Correctly display iframe's WebSockets
    auto* document = dynamicDowncast<Document>(scriptExecutionContext);
    return document && document->page() == m_inspectedPage.ptr();
}

Inspector::Protocol::ErrorStringOr<Ref<Inspector::Protocol::DOM::Styleable>> PageAnimationAgent::pushStyleablePathToFrontend(const Styleable& styleable)
{
    CheckedPtr domAgent = protect(m_instrumentingAgents)->persistentDOMAgent();
    if (!domAgent)
        return makeUnexpected("DOM domain must be enabled"_s);

    return domAgent->pushStyleablePathToFrontend({ }, styleable);
}

Inspector::Protocol::DOM::NodeId PageAnimationAgent::pushStyleableElementToFrontend(const Styleable& styleable)
{
    CheckedPtr domAgent = protect(m_instrumentingAgents)->persistentDOMAgent();
    if (!domAgent)
        return 0;

    return domAgent->pushStyleableElementToFrontend(styleable);
}

} // namespace WebCore
