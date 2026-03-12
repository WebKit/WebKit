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
#include "FrameInspectorTarget.h"
#include "FrameInspectorTargetProxy.h"
#include "InspectorBrowserAgent.h"
#include "PageInspectorTarget.h"
#include "PageInspectorTargetProxy.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "WebFrameProxy.h"
#include "WebPageInspectorAgentBase.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/TZoneMallocInlines.h>

namespace WebKit {

using namespace Inspector;

static String getTargetID(const ProvisionalPageProxy& provisionalPage)
{
    return PageInspectorTarget::toTargetID(provisionalPage.webPageID());
}

static String getTargetID(const WebFrameProxy& frame)
{
    return FrameInspectorTarget::toTargetID(frame.frameID(), frame.process().coreProcessIdentifier());
}

static String getTargetID(const ProvisionalFrameProxy& provisionalFrame)
{
    return FrameInspectorTarget::toTargetID(provisionalFrame.frame().frameID(), provisionalFrame.process().coreProcessIdentifier());
}

WTF_MAKE_TZONE_ALLOCATED_IMPL(WebPageInspectorController);

WebPageInspectorController::WebPageInspectorController(WebPageProxy& inspectedPage)
    : m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_inspectedPage(inspectedPage)
{
    auto targetAgent = makeUniqueRef<InspectorTargetAgent>(m_frontendRouter, m_backendDispatcher);
    m_targetAgent = targetAgent.ptr();
    m_agents.append(WTF::move(targetAgent));
}

WebPageInspectorController::~WebPageInspectorController() = default;

void WebPageInspectorController::init()
{
    String pageTargetId = PageInspectorTarget::toTargetID(m_inspectedPage->webPageIDInMainFrameProcess());
    addTarget(PageInspectorTargetProxy::create(protect(m_inspectedPage), pageTargetId, Inspector::InspectorTargetType::Page));
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

void WebPageInspectorController::sendMessageToInspectorFrontend(const String& targetId, const String& message)
{
    if (!m_targets.contains(targetId)) {
        // FIXME <https://webkit.org/b/308182>: This assertion is currently relaxed under site isolation.
        // More fine-tuning is needed around reporting provisional frame targets' destruction.
        if (shouldManageFrameTargets())
            return;

        ASSERT_NOT_REACHED_WITH_MESSAGE("Sending a message from an untracked target to the frontend.");
    }

    protect(m_targetAgent)->sendMessageFromTargetToFrontend(targetId, message);
}

bool WebPageInspectorController::shouldPauseLoading(const ProvisionalPageProxy& provisionalPage) const
{
    if (!m_frontendRouter->hasFrontends())
        return false;

    CheckedPtr target = m_targets.get(getTargetID(provisionalPage));
    ASSERT(target);
    return target->isPaused();
}

void WebPageInspectorController::setContinueLoadingCallback(const ProvisionalPageProxy& provisionalPage, WTF::Function<void()>&& callback)
{
    CheckedPtr target = m_targets.get(getTargetID(provisionalPage));
    ASSERT(target);
    target->setResumeCallback(WTF::move(callback));
}

void WebPageInspectorController::didCreateProvisionalPage(ProvisionalPageProxy& provisionalPage)
{
    addTarget(PageInspectorTargetProxy::create(provisionalPage, getTargetID(provisionalPage), Inspector::InspectorTargetType::Page));
}

void WebPageInspectorController::willDestroyProvisionalPage(const ProvisionalPageProxy& provisionalPage)
{
    removeTarget(getTargetID(provisionalPage));
}

void WebPageInspectorController::didCommitProvisionalPage(WebCore::PageIdentifier oldWebPageID, WebCore::PageIdentifier newWebPageID)
{
    String oldID = PageInspectorTarget::toTargetID(oldWebPageID);
    String newID = PageInspectorTarget::toTargetID(newWebPageID);
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
    m_targets.set(newTarget->identifier(), WTF::move(newTarget));
}

void WebPageInspectorController::didCreateFrame(WebFrameProxy& frame)
{
    if (!shouldManageFrameTargets())
        return;

    addTarget(FrameInspectorTargetProxy::create(frame, getTargetID(frame)));
}

void WebPageInspectorController::willDestroyFrame(const WebFrameProxy& frame)
{
    if (!shouldManageFrameTargets())
        return;

    removeTarget(getTargetID(frame));
}

void WebPageInspectorController::didCreateProvisionalFrame(ProvisionalFrameProxy& provisionalFrame)
{
    if (!shouldManageFrameTargets())
        return;

    addTarget(FrameInspectorTargetProxy::create(provisionalFrame, getTargetID(provisionalFrame)));
}

void WebPageInspectorController::willDestroyProvisionalFrame(const ProvisionalFrameProxy& provisionalFrame)
{
    if (!shouldManageFrameTargets())
        return;

    removeTarget(getTargetID(provisionalFrame));
}

void WebPageInspectorController::didCommitProvisionalFrame(WebFrameProxy& frame, WebCore::ProcessIdentifier oldProcessID, WebCore::ProcessIdentifier newProcessID)
{
    if (!shouldManageFrameTargets())
        return;

    WebCore::FrameIdentifier frameID = frame.frameID();
    String oldTargetID = FrameInspectorTarget::toTargetID(frameID, oldProcessID);
    String newTargetID = FrameInspectorTarget::toTargetID(frameID, newProcessID);

    CheckedPtr targetAgent = m_targetAgent;
    CheckedPtr newTarget = m_targets.get(newTargetID);
    ASSERT(newTarget);
    newTarget->didCommitProvisionalTarget();
    targetAgent->didCommitProvisionalTarget(oldTargetID, newTargetID);

    if (auto oldTarget = m_targets.take(oldTargetID))
        targetAgent->targetDestroyed(protect(*oldTarget));
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

    m_agents.append(makeUniqueRef<InspectorBrowserAgent>(webPageContext));
}

void WebPageInspectorController::addTarget(std::unique_ptr<InspectorTargetProxy>&& target)
{
    protect(m_targetAgent)->targetCreated(*target);
    m_targets.set(target->identifier(), WTF::move(target));
}

void WebPageInspectorController::removeTarget(const String& targetId)
{
    auto it = m_targets.find(targetId);
    if (it == m_targets.end())
        return;
    protect(m_targetAgent)->targetDestroyed(CheckedRef { *it->value });
    m_targets.remove(it);
}

bool WebPageInspectorController::shouldManageFrameTargets() const
{
    return protect(protect(m_inspectedPage)->preferences())->siteIsolationEnabled();
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
        enabledBrowserAgent->extensionsEnabled(WTF::move(extensionIDToName));
}

void WebPageInspectorController::browserExtensionsDisabled(HashSet<String>&& extensionIDs)
{
    if (CheckedPtr enabledBrowserAgent = m_enabledBrowserAgent)
        enabledBrowserAgent->extensionsDisabled(WTF::move(extensionIDs));
}

} // namespace WebKit
