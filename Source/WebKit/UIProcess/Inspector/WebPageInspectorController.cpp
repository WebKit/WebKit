/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#include "ProvisionalPageProxy.h"
#include "WebFrameProxy.h"
#include "WebPageInspectorTarget.h"
#include "WebPageProxy.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <wtf/HashMap.h>

namespace WebKit {

using namespace Inspector;

static String getTargetID(const ProvisionalPageProxy& provisionalPage)
{
    return WebPageInspectorTarget::toTargetID(provisionalPage.webPageID());
}

WebPageInspectorController::WebPageInspectorController(WebPageProxy& page)
    : m_page(page)
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
{
    auto targetAgent = makeUnique<InspectorTargetAgent>(m_frontendRouter.get(), m_backendDispatcher.get());

    m_targetAgent = targetAgent.get();

    m_agents.append(WTFMove(targetAgent));
}

void WebPageInspectorController::init()
{
    String pageTargetId = WebPageInspectorTarget::toTargetID(m_page.webPageID());
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
    bool connectingFirstFrontend = !m_frontendRouter->hasFrontends();

    m_frontendRouter->connectFrontend(frontendChannel);

    if (connectingFirstFrontend)
        m_agents.didCreateFrontendAndBackend(&m_frontendRouter.get(), &m_backendDispatcher.get());

    m_page.didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    if (hasLocalFrontend())
        m_page.remoteInspectorInformationDidChange();
#endif
}

void WebPageInspectorController::disconnectFrontend(FrontendChannel& frontendChannel)
{
    m_frontendRouter->disconnectFrontend(frontendChannel);

    bool disconnectingLastFrontend = !m_frontendRouter->hasFrontends();
    if (disconnectingLastFrontend)
        m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);

    m_page.didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    if (disconnectingLastFrontend)
        m_page.remoteInspectorInformationDidChange();
#endif
}

void WebPageInspectorController::disconnectAllFrontends()
{
    // FIXME: Handle a local inspector client.

    if (!m_frontendRouter->hasFrontends())
        return;

    // Notify agents first, since they may need to use InspectorClient.
    m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

    // Disconnect any remaining remote frontends.
    m_frontendRouter->disconnectAllFrontends();

    m_page.didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());

#if ENABLE(REMOTE_INSPECTOR)
    m_page.remoteInspectorInformationDidChange();
#endif
}

void WebPageInspectorController::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message);
}

#if ENABLE(REMOTE_INSPECTOR)
void WebPageInspectorController::setIndicating(bool indicating)
{
#if !PLATFORM(IOS_FAMILY)
    m_page.setIndicating(indicating);
#else
    if (indicating)
        m_page.showInspectorIndication();
    else
        m_page.hideInspectorIndication();
#endif
}
#endif

void WebPageInspectorController::createInspectorTarget(const String& targetId, Inspector::InspectorTargetType type)
{
    addTarget(InspectorTargetProxy::create(m_page, targetId, type));
}

void WebPageInspectorController::destroyInspectorTarget(const String& targetId)
{
    auto it = m_targets.find(targetId);
    if (it == m_targets.end())
        return;
    m_targetAgent->targetDestroyed(*it->value);
    m_targets.remove(it);
}

void WebPageInspectorController::sendMessageToInspectorFrontend(const String& targetId, const String& message)
{
    m_targetAgent->sendMessageFromTargetToFrontend(targetId, message);
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
    ASSERT(newTarget);
    newTarget->didCommitProvisionalTarget();
    m_targetAgent->didCommitProvisionalTarget(oldID, newID);

    // We've disconnected from the old page and will not receive any message from it, so
    // we destroy everything but the new target here.
    // FIXME: <https://webkit.org/b/202937> do not destroy targets that belong to the committed page.
    for (auto& target : m_targets.values())
        m_targetAgent->targetDestroyed(*target);
    m_targets.clear();
    m_targets.set(newTarget->identifier(), WTFMove(newTarget));
}

void WebPageInspectorController::addTarget(std::unique_ptr<InspectorTargetProxy>&& target)
{
    m_targetAgent->targetCreated(*target);
    m_targets.set(target->identifier(), WTFMove(target));
}

} // namespace WebKit
