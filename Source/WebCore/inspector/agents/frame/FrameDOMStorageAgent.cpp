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

#include "config.h"
#include "FrameDOMStorageAgent.h"

#include "InstrumentingAgents.h"
#include "LocalFrame.h"
#include <JavaScriptCore/InspectorFrontendDispatchers.h>
#include <wtf/JSONValues.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

using namespace Inspector;

WTF_MAKE_TZONE_ALLOCATED_IMPL(FrameDOMStorageAgent);

FrameDOMStorageAgent::FrameDOMStorageAgent(FrameAgentContext& context)
    : InspectorAgentBase("DOMStorage"_s, context)
    , m_frontendDispatcher(makeUniqueRef<Inspector::DOMStorageFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::DOMStorageBackendDispatcher::create(Ref { context.backendDispatcher }, this))
    , m_inspectedFrame(context.inspectedFrame)
{
}

FrameDOMStorageAgent::~FrameDOMStorageAgent() = default;

void FrameDOMStorageAgent::didCreateFrontendAndBackend()
{
}

void FrameDOMStorageAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

Inspector::CommandResult<void> FrameDOMStorageAgent::enable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledFrameDOMStorageAgent() == this)
        return { };

    agents->setEnabledFrameDOMStorageAgent(this);

    return { };
}

Inspector::CommandResult<void> FrameDOMStorageAgent::disable()
{
    Ref agents = m_instrumentingAgents.get();
    if (agents->enabledFrameDOMStorageAgent() != this)
        return { };

    agents->setEnabledFrameDOMStorageAgent(nullptr);

    return { };
}

Inspector::CommandResult<Ref<JSON::ArrayOf<Inspector::Protocol::DOMStorage::Item>>> FrameDOMStorageAgent::getDOMStorageItems(Ref<JSON::Object>&&)
{
    // FIXME: <rdar://179249711>: Implement DOMStorage commands for frame targets.
    return makeUnexpected("DOMStorage commands are not yet implemented for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMStorageAgent::setDOMStorageItem(Ref<JSON::Object>&&, const String&, const String&)
{
    // FIXME: <rdar://179249711>: Implement DOMStorage commands for frame targets.
    return makeUnexpected("DOMStorage commands are not yet implemented for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMStorageAgent::removeDOMStorageItem(Ref<JSON::Object>&&, const String&)
{
    // FIXME: <rdar://179249711>: Implement DOMStorage commands for frame targets.
    return makeUnexpected("DOMStorage commands are not yet implemented for frame targets"_s);
}

Inspector::CommandResult<void> FrameDOMStorageAgent::clearDOMStorageItems(Ref<JSON::Object>&&)
{
    // FIXME: <rdar://179249711>: Implement DOMStorage commands for frame targets.
    return makeUnexpected("DOMStorage commands are not yet implemented for frame targets"_s);
}

} // namespace WebCore
