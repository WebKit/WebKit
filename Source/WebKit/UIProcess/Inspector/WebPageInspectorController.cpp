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
#include "ProxyingNetworkAgent.h"
#include "ProxyingPageAgent.h"
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

// For an uncommitted provisional page, which is delegated through its main frame target under SI.
static String getMainFrameTargetID(const ProvisionalPageProxy& provisionalPage)
{
    return FrameInspectorTarget::toTargetID(protect(provisionalPage.mainFrame())->frameID(), provisionalPage.process().coreProcessIdentifier());
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

    if (connectingFirstFrontend) {
        m_agents.didCreateFrontendAndBackend();
        if (RefPtr networkAgent = m_networkAgent)
            networkAgent->didCreateFrontendAndBackend();
        if (RefPtr pageAgent = m_pageAgent)
            pageAgent->didCreateFrontendAndBackend();
    }

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
    if (disconnectingLastFrontend) {
        m_agents.willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);
        if (RefPtr networkAgent = m_networkAgent)
            networkAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);
        if (RefPtr pageAgent = m_pageAgent)
            pageAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);
    }

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
    if (RefPtr networkAgent = m_networkAgent)
        networkAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);
    if (RefPtr pageAgent = m_pageAgent)
        pageAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

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

bool WebPageInspectorController::shouldPauseLoadingForPage(const ProvisionalPageProxy& provisionalPage) const
{
    if (!m_frontendRouter->hasFrontends())
        return false;

    if (!shouldManageFrameTargets()) {
        CheckedPtr target = m_targets.get(getTargetID(provisionalPage));
        ASSERT(target);
        return target->isPaused();
    }

    CheckedPtr target = m_targets.get(getMainFrameTargetID(provisionalPage));
    ASSERT(target);
    return target->isPaused();
}

void WebPageInspectorController::setContinueLoadingCallbackForPage(const ProvisionalPageProxy& provisionalPage, WTF::Function<void()>&& callback)
{
    if (!shouldManageFrameTargets()) {
        CheckedPtr target = m_targets.get(getTargetID(provisionalPage));
        ASSERT(target);
        target->setResumeCallback(WTF::move(callback));
        return;
    }

    CheckedPtr target = m_targets.get(getMainFrameTargetID(provisionalPage));
    ASSERT(target);
    target->setResumeCallback(WTF::move(callback));
}

bool WebPageInspectorController::shouldPauseLoadingForFrame(const ProvisionalFrameProxy& provisionalFrame) const
{
    if (!shouldManageFrameTargets())
        return false;

    if (!m_frontendRouter->hasFrontends())
        return false;

    CheckedPtr target = m_targets.get(getTargetID(provisionalFrame));
    ASSERT(target);
    return target->isPaused();
}

void WebPageInspectorController::setContinueLoadingCallbackForFrame(const ProvisionalFrameProxy& provisionalFrame, WTF::Function<void()>&& callback)
{
    ASSERT(shouldManageFrameTargets());

    CheckedPtr target = m_targets.get(getTargetID(provisionalFrame));
    ASSERT(target);
    target->setResumeCallback(WTF::move(callback));
}

void WebPageInspectorController::didCreateProvisionalPage(ProvisionalPageProxy& provisionalPage, WebCore::FrameIdentifier mainFrameID, WebProcessProxy& mainFrameProcess)
{
    addTarget(PageInspectorTargetProxy::create(provisionalPage, getTargetID(provisionalPage), Inspector::InspectorTargetType::Page));

    if (shouldManageFrameTargets()) {
        constexpr bool isProvisional = true;
        addTarget(makeUnique<FrameInspectorTargetProxy>(mainFrameID, mainFrameProcess, isProvisional));
    }
}

void WebPageInspectorController::willDestroyProvisionalPage(const ProvisionalPageProxy& provisionalPage, WebCore::FrameIdentifier mainFrameID, WebCore::ProcessIdentifier mainFrameProcessID)
{
    removeTarget(getTargetID(provisionalPage));

    if (shouldManageFrameTargets())
        removeTarget(FrameInspectorTarget::toTargetID(mainFrameID, mainFrameProcessID));
}

void WebPageInspectorController::didCommitProvisionalPage(std::optional<WebCore::FrameIdentifier> oldMainFrameID, WebCore::ProcessIdentifier oldProcessID, WebCore::PageIdentifier oldWebPageID, WebCore::PageIdentifier newWebPageID)
{
    String oldPageTargetID = PageInspectorTarget::toTargetID(oldWebPageID);
    String newPageTargetID = PageInspectorTarget::toTargetID(newWebPageID);
    CheckedPtr targetAgent = m_targetAgent;

    // Commit the provisional page target.
    CheckedPtr newPageTarget = m_targets.get(newPageTargetID);
    ASSERT(newPageTarget);
    newPageTarget->didCommitProvisionalTarget();
    targetAgent->didCommitProvisionalTarget(oldPageTargetID, newPageTargetID);

    // Commit the provisional main frame target.
    bool shouldManageFrameTargets = this->shouldManageFrameTargets();
    String mainFrameTargetID;
    if (shouldManageFrameTargets) {
        RefPtr mainFrame = protect(m_inspectedPage)->mainFrame();
        mainFrameTargetID = FrameInspectorTarget::toTargetID(mainFrame->frameID(), protect(mainFrame->process())->coreProcessIdentifier());

        CheckedPtr mainFrameTarget = m_targets.get(mainFrameTargetID);
        ASSERT(mainFrameTarget && mainFrameTarget->isProvisional());
        mainFrameTarget->didCommitProvisionalTarget();

        ASSERT(oldMainFrameID);
        String oldMainFrameTargetID = FrameInspectorTarget::toTargetID(*oldMainFrameID, oldProcessID);
        targetAgent->didCommitProvisionalTarget(oldMainFrameTargetID, mainFrameTargetID);
    }

    // Update target list to only include targets belonging to the committed page.
    Vector<String> targetIDsToRemove;
    for (auto& [targetID, target] : m_targets) {
        if (targetID == newPageTargetID)
            continue;
        if (shouldManageFrameTargets && targetID == mainFrameTargetID)
            continue;
        targetIDsToRemove.append(targetID);
    }

    for (auto& targetID : targetIDsToRemove) {
        if (CheckedPtr target = m_targets.get(targetID))
            targetAgent->targetDestroyed(*target);
    }

    for (auto& targetID : targetIDsToRemove)
        m_targets.remove(targetID);

    // Migrate per-process inspector instrumentation: the old process no
    // longer hosts the page, so unregister there to keep our message-receiver
    // count balanced, and register on the new process. Mirrors
    // didCommitProvisionalFrame.
    RefPtr oldProcess = WebProcessProxy::processForIdentifier(oldProcessID);
    Ref newProcess = protect(m_inspectedPage)->mainFrame()->process();

    RefPtr pageAgent = m_pageAgent;
    if (pageAgent && pageAgent->isEnabled()) {
        if (oldProcess)
            pageAgent->disableInstrumentationForProcess(*oldProcess, oldWebPageID);
        pageAgent->enableInstrumentationForProcess(newProcess, newWebPageID);
    }

    RefPtr networkAgent = m_networkAgent;
    if (networkAgent && networkAgent->isEnabled()) {
        if (oldProcess)
            networkAgent->disableInstrumentationForProcess(*oldProcess, oldWebPageID);
        networkAgent->enableInstrumentationForProcess(newProcess, newWebPageID);
    }
}

void WebPageInspectorController::didCreateFrame(WebFrameProxy& frame)
{
    if (!shouldManageFrameTargets())
        return;

    constexpr bool isProvisional = false;
    Ref process = frame.process();
    addTarget(makeUnique<FrameInspectorTargetProxy>(frame.frameID(), process, isProvisional));

    RefPtr networkAgent = m_networkAgent;
    if (networkAgent && networkAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            networkAgent->enableInstrumentationForProcess(process, *pageID);
    }

    RefPtr pageAgent = m_pageAgent;
    if (pageAgent && pageAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            pageAgent->enableInstrumentationForProcess(process, *pageID);
    }
}

void WebPageInspectorController::willDestroyFrame(const WebFrameProxy& frame)
{
    if (!shouldManageFrameTargets())
        return;

    Ref process = frame.process();

    RefPtr networkAgent = m_networkAgent;
    if (networkAgent && networkAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            networkAgent->disableInstrumentationForProcess(process, *pageID);
    }

    RefPtr pageAgent = m_pageAgent;
    if (pageAgent && pageAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            pageAgent->disableInstrumentationForProcess(process, *pageID);
    }

    removeTarget(getTargetID(frame));
}

void WebPageInspectorController::didCreateProvisionalFrame(ProvisionalFrameProxy& provisionalFrame)
{
    if (!shouldManageFrameTargets())
        return;

    constexpr bool isProvisional = true;
    addTarget(makeUnique<FrameInspectorTargetProxy>(protect(provisionalFrame.frame())->frameID(), protect(provisionalFrame.process()), isProvisional));

    // Register page instrumentation for the provisional frame's (possibly brand-new, cross-origin)
    // process *before* it commits, so the UIProcess ProxyingPageAgent has a message receiver ready
    // when the child's initial frameNavigated fires. didCommitProvisionalFrame is too late: the
    // child commits (and emits frameNavigated) in its own process before then. See webkit.org/b/308896.
    RefPtr pageAgent = m_pageAgent;
    Ref process = provisionalFrame.process();
    auto pageID = protect(m_inspectedPage)->webPageIDInProcess(process);
    if (pageAgent && pageAgent->isEnabled())
        pageAgent->enableInstrumentationForProcess(process, pageID);
}

void WebPageInspectorController::willDestroyProvisionalFrame(const ProvisionalFrameProxy& provisionalFrame)
{
    if (!shouldManageFrameTargets())
        return;

    String targetId = getTargetID(provisionalFrame);
    if (CheckedPtr target = m_targets.get(targetId)) {
        // The resume callback is required because it wraps a CompletionHandler from
        // prepareForProvisionalLoadInProcess. CompletionHandlers must be called before destruction.
        if (target->isPaused())
            target->resume();
    }
    removeTarget(targetId);

    // Balance the enableInstrumentationForProcess() done in didCreateProvisionalFrame for a
    // provisional frame that is being discarded WITHOUT committing. (On commit, this destructor
    // early-returns because takeFrameProcess() already nulled m_frameProcess, and the registration
    // is instead carried forward by didCommitProvisionalFrame.) See webkit.org/b/308896.
    RefPtr pageAgent = m_pageAgent;
    Ref process = provisionalFrame.process();
    auto pageID = protect(m_inspectedPage)->webPageIDInProcess(process);
    if (pageAgent && pageAgent->isEnabled())
        pageAgent->disableInstrumentationForProcess(process, pageID);
}

void WebPageInspectorController::didCommitProvisionalFrame(WebFrameProxy& frame, WebCore::ProcessIdentifier oldProcessID, std::optional<WebCore::PageIdentifier> oldPageID, WebCore::ProcessIdentifier newProcessID)
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

    // Instrument the new process for network events now that the frame has
    // committed in its final process. Also disable instrumentation for the
    // old process; the frame no longer lives there.
    RefPtr oldProcess = WebProcessProxy::processForIdentifier(oldProcessID);
    Ref process = frame.process();

    RefPtr networkAgent = m_networkAgent;
    if (networkAgent && networkAgent->isEnabled()) {
        if (oldProcess && oldPageID)
            networkAgent->disableInstrumentationForProcess(*oldProcess, *oldPageID);
        if (auto pageID = frame.webPageIDInCurrentProcess())
            networkAgent->enableInstrumentationForProcess(process, *pageID);
    }

    RefPtr pageAgent = m_pageAgent;
    if (pageAgent && pageAgent->isEnabled()) {
        if (oldProcess && oldPageID)
            pageAgent->disableInstrumentationForProcess(*oldProcess, *oldPageID);
        // Unlike the network agent, the page agent already registered the new (committing)
        // process in didCreateProvisionalFrame -- so the frame's initial Page.frameNavigated
        // is delivered. Re-registering here would double-count the receiver, so we only
        // release the old process. See webkit.org/b/308896.
    }
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

    if (protect(protect(m_inspectedPage)->preferences())->siteIsolationEnabled()) {
        // ProxyingNetworkAgent and ProxyingPageAgent are RefCounted (for IPC MessageReceiver)
        // so they can't be stored in AgentRegistry which expects UniqueRef ownership.
        // Their lifecycle (didCreateFrontendAndBackend / willDestroyFrontendAndBackend) is
        // managed explicitly in connectFrontend / disconnectFrontend / disconnectAllFrontends.
        m_networkAgent = adoptRef(*new Inspector::ProxyingNetworkAgent(webPageContext));
        m_pageAgent = adoptRef(*new Inspector::ProxyingPageAgent(webPageContext));
    }
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

bool WebPageInspectorController::isNetworkInstrumentationEnabled() const
{
    return m_networkAgent && m_networkAgent->isEnabled();
}

bool WebPageInspectorController::isPageInstrumentationEnabled() const
{
    return m_pageAgent && m_pageAgent->isEnabled();
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
