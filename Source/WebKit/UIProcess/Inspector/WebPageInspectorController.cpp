    /*
 * Copyright (C) 2018-2020 Apple Inc. All rights reserved.
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
#include "WebPageInspectorController.h"

#include "APIUIClient.h"
#include "InspectorBrowserAgent.h"
#include "ProvisionalPageProxy.h"
#include "WebFrameProxy.h"
#include "WebPageInspectorAgentBase.h"
#include "WebPageInspectorTarget.h"
#include "WebPageProxy.h"
#include "WebsiteDataStore.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;

static String getTargetID(const ProvisionalPageProxy& provisionalPage)
{
    return WebPageInspectorTarget::toTargetID(provisionalPage.webPageID());
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPageInspectorController);

WebPageInspectorController::WebPageInspectorController(WebPageProxy& inspectedPage)
    : m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_inspectedPage(inspectedPage)
{
    auto targetAgent = makeUnique<InspectorTargetAgent>(m_frontendRouter, m_backendDispatcher);
    m_targetAgent = targetAgent.get();
    m_agents.append(WTFMove(targetAgent));
}

WebPageInspectorController::~WebPageInspectorController() = default;

Ref<WebPageProxy> WebPageInspectorController::protectedInspectedPage()
{
    return m_inspectedPage.get();
}

void WebPageInspectorController::init()
{
    String pageTargetId = WebPageInspectorTarget::toTargetID(m_inspectedPage->webPageIDInMainFrameProcess());
    createInspectorTarget(pageTargetId, Inspector::InspectorTargetType::Page);
}

void WebPageInspectorController::pageClosed()
{
    disconnectAllFrontends();

    m_agents.discardValues();
}

bool WebPageInspectorController::hasLocalFrontend() const
{
    return m_frontendRouter->hasLocalFrontend();
}

void WebPageInspectorController::connectFrontend(Inspector::FrontendChannel& frontendChannel, bool, bool)
{
    createLazyAgents();

    bool connectingFirstFrontend = !m_frontendRouter->hasFrontends();

    m_frontendRouter->connectFrontend(frontendChannel);

    if (connectingFirstFrontend)
        m_agents.didCreateFrontendAndBackend();

    Ref inspectedPage = m_inspectedPage.get();
    inspectedPage->didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    if (hasLocalFrontend())
        inspectedPage->remoteInspectorInformationDidChange();
#endif
}

void WebPageInspectorController::disconnectFrontend(FrontendChannel& frontendChannel)
{
    m_frontendRouter->disconnectFrontend(frontendChannel);

    bool disconnectingLastFrontend = !m_frontendRouter->hasFrontends();
    if (disconnectingLastFrontend)
        m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);

    Ref inspectedPage = m_inspectedPage.get();
    inspectedPage->didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    if (disconnectingLastFrontend)
        inspectedPage->remoteInspectorInformationDidChange();
#endif
}

void WebPageInspectorController::disconnectAllFrontends()
{
    // FIXME: Handle a local inspector client.

    if (!m_frontendRouter->hasFrontends())
        return;

    // Notify agents first, since they may need to use InspectorBackendClient.
    m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

    // Disconnect any remaining remote frontends.
    m_frontendRouter->disconnectAllFrontends();

    Ref inspectedPage = m_inspectedPage.get();
    inspectedPage->didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    inspectedPage->remoteInspectorInformationDidChange();
#endif
}

void WebPageInspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

#if ENABLE(REMOTE_INSPECTOR)
void WebPageInspectorController::setIndicating(bool indicating)
{
    Ref inspectedPage = m_inspectedPage.get();
#if !PLATFORM(IOS_FAMILY)
    inspectedPage->setIndicating(indicating);
#else
    if (indicating)
        inspectedPage->showInspectorIndication();
    else
        inspectedPage->hideInspectorIndication();
#endif
}
#endif

void WebPageInspectorController::createInspectorTarget(const String& targetId, Inspector::InspectorTargetType type)
{
    addTarget(InspectorTargetProxy::create(protectedInspectedPage(), targetId, type));
}

void WebPageInspectorController::destroyInspectorTarget(const String& targetId)
{
    auto it = m_targets.find(targetId);
    if (it == m_targets.end())
        return;
    checkedTargetAgent()->targetDestroyed(*it->value);
    m_targets.remove(it);
}

void WebPageInspectorController::sendMessageToInspectorFrontend(const String& targetId, const String& message)
{
    checkedTargetAgent()->sendMessageFromTargetToFrontend(targetId, message);
}

bool WebPageInspectorController::shouldPauseLoading(const ProvisionalPageProxy& provisionalPage) const
{
    if (!m_frontendRouter->hasFrontends())
        return false;

    auto* target = m_targets.get(getTargetID(provisionalPage));
    ASSERT(target);
    return target->isPaused();
}

void WebPageInspectorController::setContinueLoadingCallback(const ProvisionalPageProxy& provisionalPage, WTF::Function<void()>&& callback)
{
    auto* target = m_targets.get(getTargetID(provisionalPage));
    ASSERT(target);
    target->setResumeCallback(WTFMove(callback));
}

void WebPageInspectorController::didCreateProvisionalPage(ProvisionalPageProxy& provisionalPage)
{
    addTarget(InspectorTargetProxy::create(provisionalPage, getTargetID(provisionalPage), Inspector::InspectorTargetType::Page));
}

void WebPageInspectorController::willDestroyProvisionalPage(const ProvisionalPageProxy& provisionalPage)
{
    destroyInspectorTarget(getTargetID(provisionalPage));
}

void WebPageInspectorController::didCommitProvisionalPage(WebCore::PageIdentifier oldWebPageID, WebCore::PageIdentifier newWebPageID)
{
    String oldID = WebPageInspectorTarget::toTargetID(oldWebPageID);
    String newID = WebPageInspectorTarget::toTargetID(newWebPageID);
    auto newTarget = m_targets.take(newID);
    CheckedPtr targetAgent = m_targetAgent;
    ASSERT(newTarget);
    newTarget->didCommitProvisionalTarget();
    targetAgent->didCommitProvisionalTarget(oldID, newID);

    // We've disconnected from the old page and will not receive any message from it, so
    // we destroy everything but the new target here.
    // FIXME: <https://webkit.org/b/202937> do not destroy targets that belong to the committed page.
    for (auto& target : m_targets.values())
        targetAgent->targetDestroyed(*target);
    m_targets.clear();
    m_targets.set(newTarget->identifier(), WTFMove(newTarget));
}

InspectorBrowserAgent* WebPageInspectorController::enabledBrowserAgent() const
{
    return m_enabledBrowserAgent.get();
}

WebPageAgentContext WebPageInspectorController::webPageAgentContext()
{
    return {
        m_frontendRouter,
        m_backendDispatcher,
        m_inspectedPage,
    };
}

void WebPageInspectorController::createLazyAgents()
{
    if (m_didCreateLazyAgents)
        return;

    m_didCreateLazyAgents = true;

    auto webPageContext = webPageAgentContext();

    m_agents.append(makeUnique<InspectorBrowserAgent>(webPageContext));
}

void WebPageInspectorController::addTarget(std::unique_ptr<InspectorTargetProxy>&& target)
{
    checkedTargetAgent()->targetCreated(*target);
    m_targets.set(target->identifier(), WTFMove(target));
}

void WebPageInspectorController::setEnabledBrowserAgent(InspectorBrowserAgent* agent)
{
    if (m_enabledBrowserAgent == agent)
        return;

    m_enabledBrowserAgent = agent;

    Ref inspectedPage = m_inspectedPage.get();
    if (m_enabledBrowserAgent)
        inspectedPage->uiClient().didEnableInspectorBrowserDomain(inspectedPage);
    else
        inspectedPage->uiClient().didDisableInspectorBrowserDomain(inspectedPage);
}

void WebPageInspectorController::browserExtensionsEnabled(HashMap<String, String>&& extensionIDToName)
{
    if (CheckedPtr enabledBrowserAgent = m_enabledBrowserAgent)
        enabledBrowserAgent->extensionsEnabled(WTFMove(extensionIDToName));
}

void WebPageInspectorController::browserExtensionsDisabled(HashSet<String>&& extensionIDs)
{
    if (CheckedPtr enabledBrowserAgent = m_enabledBrowserAgent)
        enabledBrowserAgent->extensionsDisabled(WTFMove(extensionIDs));
}

} // namespace WebKit
