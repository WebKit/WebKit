/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "InspectorBrowserAgent.h"

#include "APIInspectorClient.h"
#include "WebPageInspectorController.h"
#include "WebPageProxy.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

using namespace Inspector;

InspectorBrowserAgent::InspectorBrowserAgent(WebPageAgentContext& context)
    : InspectorAgentBase("Browser"_s, context)
    , m_frontendDispatcher(makeUnique<Inspector::BrowserFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(Inspector::BrowserBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
{
}

InspectorBrowserAgent::~InspectorBrowserAgent() = default;

bool InspectorBrowserAgent::enabled() const
{
    return m_inspectedPage.inspectorController().enabledBrowserAgent() == this;
}

void InspectorBrowserAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorBrowserAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
    disable();
}

Inspector::Protocol::ErrorStringOr<void> InspectorBrowserAgent::enable()
{
    if (enabled())
        return makeUnexpected("Browser domain already enabled"_s);

    m_inspectedPage.inspectorController().setEnabledBrowserAgent(this);

    auto* inspector = m_inspectedPage.inspector();
    ASSERT(inspector);
    m_inspectedPage.inspectorClient().browserDomainEnabled(m_inspectedPage, *inspector);

    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorBrowserAgent::disable()
{
    if (!enabled())
        return makeUnexpected("Browser domain already disabled"_s);

    m_inspectedPage.inspectorController().setEnabledBrowserAgent(nullptr);

    if (auto* inspector = m_inspectedPage.inspector())
        m_inspectedPage.inspectorClient().browserDomainDisabled(m_inspectedPage, *inspector);

    return { };
}

void InspectorBrowserAgent::extensionsEnabled(HashMap<String, String>&& extensionIDToName)
{
    ASSERT(enabled());

    auto extensionsPayload = JSON::ArrayOf<Inspector::Protocol::Browser::Extension>::create();
    for (auto& [id, name] : extensionIDToName) {
        auto extensionPayload = Inspector::Protocol::Browser::Extension::create()
            .setExtensionId(id)
            .setName(name)
            .release();
        extensionsPayload->addItem(WTFMove(extensionPayload));
    }
    m_frontendDispatcher->extensionsEnabled(WTFMove(extensionsPayload));
}

void InspectorBrowserAgent::extensionsDisabled(HashSet<String>&& extensionIDs)
{
    ASSERT(enabled());

    auto extensionIdsPayload = JSON::ArrayOf<String>::create();
    for (auto& extensionId : extensionIDs)
        extensionIdsPayload->addItem(extensionId);
    m_frontendDispatcher->extensionsDisabled(WTFMove(extensionIdsPayload));
}


} // namespace WebCore
