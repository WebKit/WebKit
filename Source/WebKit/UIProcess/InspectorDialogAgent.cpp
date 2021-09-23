/*
 * Copyright (C) 2019 Microsoft Corporation.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "InspectorDialogAgent.h"

#include "APINavigation.h"
#include "APIUIClient.h"
#include "WebPageProxy.h"
#include <JavaScriptCore/InspectorFrontendRouter.h>


namespace WebKit {

using namespace Inspector;

InspectorDialogAgent::InspectorDialogAgent(Inspector::BackendDispatcher& backendDispatcher, Inspector::FrontendRouter& frontendRouter, WebPageProxy& page)
    : InspectorAgentBase("Dialog"_s)
    , m_frontendDispatcher(makeUnique<DialogFrontendDispatcher>(frontendRouter))
    , m_backendDispatcher(DialogBackendDispatcher::create(backendDispatcher, this))
    , m_page(page)
{
}

InspectorDialogAgent::~InspectorDialogAgent()
{
    disable();
}

void InspectorDialogAgent::didCreateFrontendAndBackend(Inspector::FrontendRouter*, Inspector::BackendDispatcher*)
{
}

void InspectorDialogAgent::willDestroyFrontendAndBackend(Inspector::DisconnectReason)
{
}

Inspector::Protocol::ErrorStringOr<void> InspectorDialogAgent::enable()
{
    if (m_page.inspectorDialogAgent())
        return makeUnexpected("Dialog domain is already enabled."_s);

    m_page.setInspectorDialogAgent(this);
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDialogAgent::disable()
{
    if (m_page.inspectorDialogAgent() != this)
        return { };

    m_page.setInspectorDialogAgent(nullptr);
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorDialogAgent::handleJavaScriptDialog(bool accept, const String& value)
{
    m_page.uiClient().handleJavaScriptDialog(m_page, accept, value);
    return { };
}

void InspectorDialogAgent::javascriptDialogOpening(const String& type, const String& message, const String& defaultValue) {
    m_frontendDispatcher->javascriptDialogOpening(type, message, defaultValue);
}

} // namespace WebKit
