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
#include "BrowsingContextGroup.h"
#include "DebuggerPauseCoordinator.h"
#include "FrameInspectorTarget.h"
#include "FrameInspectorTargetProxy.h"
#include "ImageOptions.h"
#include "InspectorBrowserAgent.h"
#include "InspectorStorageAgent.h"
#include "PageInspectorTarget.h"
#include "PageInspectorTargetProxy.h"
#include "ProvisionalFrameProxy.h"
#include "ProvisionalPageProxy.h"
#include "ProxyingNetworkAgent.h"
#include "ProxyingPageAgent.h"
#include "WebFrameProxy.h"
#include "WebPageInspectorAgentBase.h"
#include "WebPageMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include "WebsiteDataStore.h"
#include <JavaScriptCore/InspectorAgentBase.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorBackendDispatchers.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/InspectorTargetAgent.h>
#include <WebCore/ImageUtilities.h>
#include <wtf/Assertions.h>
#include <wtf/HashMap.h>
#include <wtf/RunLoop.h>
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

WebPageInspectorController::~WebPageInspectorController()
{
    finishTimelineScreenshot();
}

void WebPageInspectorController::init()
{
    String pageTargetId = PageInspectorTarget::toTargetID(m_inspectedPage->webPageIDInMainFrameProcess());
    addTarget(PageInspectorTargetProxy::create(protect(m_inspectedPage), pageTargetId, Inspector::InspectorTargetType::Page));
}

void WebPageInspectorController::pageClosed()
{
    m_debuggerPauseCoordinator = nullptr;
    finishTimelineScreenshot();
    disconnectAllFrontends();

    m_agents.discardValues();
}

bool WebPageInspectorController::hasLocalFrontend() const
{
    return m_frontendRouter->hasLocalFrontend();
}

bool WebPageInspectorController::hasFrontends() const
{
    return m_frontendRouter->hasFrontends();
}

void WebPageInspectorController::connectFrontend(Inspector::FrontendChannel& frontendChannel, bool, bool)
{
    createLazyAgents();

    bool connectingFirstFrontend = !m_frontendRouter->hasFrontends();

    m_frontendRouter->connectFrontend(frontendChannel);

    if (connectingFirstFrontend) {
        m_agents.didCreateFrontendAndBackend();
        if (m_networkAgent)
            m_networkAgent->didCreateFrontendAndBackend();
        if (m_pageAgent)
            m_pageAgent->didCreateFrontendAndBackend();
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
        if (m_networkAgent)
            m_networkAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);
        if (m_pageAgent)
            m_pageAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectorDestroyed);
    }

    Ref inspectedPage = m_inspectedPage.get();
    inspectedPage->didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());
    if (disconnectingLastFrontend && m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->closeSession();

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
    if (m_networkAgent)
        m_networkAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);
    if (m_pageAgent)
        m_pageAgent->willDestroyFrontendAndBackend(DisconnectReason::InspectedTargetDestroyed);

    // Disconnect any remaining remote frontends.
    m_frontendRouter->disconnectAllFrontends();

    Ref inspectedPage = m_inspectedPage.get();
    inspectedPage->didChangeInspectorFrontendCount(m_frontendRouter->frontendCount());
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->closeSession();

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
    const Vector targetIDsToRemove = [&] {
        Vector<String> targetIDsToRemove;
        for (auto& [targetID, target] : m_targets) {
            if (targetID == newPageTargetID)
                continue;
            if (shouldManageFrameTargets && targetID == mainFrameTargetID)
                continue;
            targetIDsToRemove.append(targetID);
        }
        return targetIDsToRemove;
    }();

    RefPtr oldProcess = WebProcessProxy::processForIdentifier(oldProcessID);
    bool hasFrontends = m_frontendRouter->hasFrontends();

    for (auto& targetID : targetIDsToRemove) {
        CheckedPtr target = m_targets.get(targetID);
        if (!target)
            continue;

        if (hasFrontends) {
            // PageInspectorTargetProxy would route through the WebPageProxy, which already points at the new process.
            if (targetID == oldPageTargetID) {
                if (oldProcess)
                    oldProcess->send(Messages::WebPage::DisconnectInspector(), oldWebPageID);
            } else
                target->disconnect();
        }

        targetAgent->targetDestroyed(*target);
    }

    for (auto& targetID : targetIDsToRemove)
        m_targets.remove(targetID);

    // Migrate per-process inspector instrumentation: the old process no
    // longer hosts the page, so unregister there to keep our message-receiver
    // count balanced, and register on the new process. Unlike a provisional
    // frame, a provisional page does not register its process when it is created.
    // FIXME <https://webkit.org/b/328737>: Register it then, so the main document's Network and Page messages are not dropped before commit.
    Ref newProcess = protect(m_inspectedPage)->mainFrame()->process();

    if (m_pageAgent && m_pageAgent->isEnabled()) {
        if (oldProcess)
            m_pageAgent->disableInstrumentationForProcess(*oldProcess, oldWebPageID);
        m_pageAgent->enableInstrumentationForProcess(newProcess, newWebPageID);
    }

    if (m_networkAgent && m_networkAgent->isEnabled()) {
        if (oldProcess)
            m_networkAgent->disableInstrumentationForProcess(*oldProcess, oldWebPageID);
        m_networkAgent->enableInstrumentationForProcess(newProcess, newWebPageID);
    }
}

void WebPageInspectorController::frameDidPauseForDebugger(WebProcessProxy& process, WebCore::FrameIdentifier frameID)
{
    if (!m_debuggerPauseCoordinator)
        m_debuggerPauseCoordinator = makeUnique<DebuggerPauseCoordinator>(protect(m_inspectedPage));
    CheckedPtr { m_debuggerPauseCoordinator }->frameDidPause(process, frameID);
}

void WebPageInspectorController::frameDidContinueForDebugger(WebProcessProxy& process, WebCore::FrameIdentifier frameID)
{
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->frameDidContinue(process, frameID);
}

// The session of any page in the browsing context group may hold the added page.
void WebPageInspectorController::pageWasAddedToProcess(BrowsingContextGroup& group, WebProcessProxy& process, WebCore::PageIdentifier pageID)
{
    group.forEachPage([&](WebPageProxy& page) {
        page.inspectorController().browsingContextGroupPageWasAddedToProcess(process, pageID);
    });
}

void WebPageInspectorController::browsingContextGroupPageWasAddedToProcess(WebProcessProxy& process, WebCore::PageIdentifier pageID)
{
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->pageWasAdded(process, pageID);
}

void WebPageInspectorController::frameDidChangeProcess(WebCore::FrameIdentifier frameID, WebCore::ProcessIdentifier newProcessID)
{
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->frameDidChangeProcess(frameID, newProcessID);
}

void WebPageInspectorController::mainFrameDidStartProvisionalLoad()
{
    m_isMainFrameNavigating = true;
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->mainFrameDidStartProvisionalLoad();
}

void WebPageInspectorController::mainFrameDidCommitLoad()
{
    m_isMainFrameNavigating = false;
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->closeSession();
}

void WebPageInspectorController::mainFrameProvisionalLoadDidEnd(ProvisionalLoadIsReplaced isReplaced)
{
    if (isReplaced == ProvisionalLoadIsReplaced::Yes)
        return;

    m_isMainFrameNavigating = false;
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->openSessionIfFramesAreStopping();
}

void WebPageInspectorController::processDidTerminate(WebProcessProxy& process)
{
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->processDidTerminate(process);
}

void WebPageInspectorController::didCreateFrame(WebFrameProxy& frame)
{
    if (!shouldManageFrameTargets())
        return;

    constexpr bool isProvisional = false;
    Ref process = frame.process();
    addTarget(makeUnique<FrameInspectorTargetProxy>(frame.frameID(), process, isProvisional));

    if (m_networkAgent && m_networkAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            m_networkAgent->enableInstrumentationForProcess(process, *pageID);
    }

    if (m_pageAgent && m_pageAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            m_pageAgent->enableInstrumentationForProcess(process, *pageID);
    }
}

void WebPageInspectorController::willDestroyFrame(const WebFrameProxy& frame)
{
    if (m_debuggerPauseCoordinator)
        CheckedPtr { m_debuggerPauseCoordinator }->frameWillBeDestroyed(frame.frameID());

    if (!shouldManageFrameTargets())
        return;

    Ref process = frame.process();

    if (m_networkAgent && m_networkAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            m_networkAgent->disableInstrumentationForProcess(process, *pageID);
    }

    if (m_pageAgent && m_pageAgent->isEnabled()) {
        if (auto pageID = frame.webPageIDInCurrentProcess())
            m_pageAgent->disableInstrumentationForProcess(process, *pageID);

        // A WebFrameProxy is destroyed only when the frame is genuinely removed (never on a
        // process swap, where it persists), so this is the authoritative point to report the
        // frame's removal to the frontend. See webkit.org/b/308896.
        m_pageAgent->frameDestroyed(frame.frameID());
    }

    removeTarget(getTargetID(frame));
}

void WebPageInspectorController::didCreateProvisionalFrame(ProvisionalFrameProxy& provisionalFrame)
{
    if (!shouldManageFrameTargets())
        return;

    constexpr bool isProvisional = true;
    addTarget(makeUnique<FrameInspectorTargetProxy>(protect(provisionalFrame.frame())->frameID(), protect(provisionalFrame.process()), isProvisional));

    // Register instrumentation for the provisional frame's (possibly brand-new, cross-origin) process
    // *before* it commits, so the UIProcess agents have a message receiver ready when the child loads
    // its document and fires its initial frameNavigated. didCommitProvisionalFrame is too late: both
    // happen in the child's own process before then. See webkit.org/b/308896 and webkit.org/b/328708.
    Ref process = provisionalFrame.process();
    auto pageID = protect(m_inspectedPage)->webPageIDInProcess(process);
    if (m_networkAgent && m_networkAgent->isEnabled())
        m_networkAgent->enableInstrumentationForProcess(process, pageID);
    if (m_pageAgent && m_pageAgent->isEnabled())
        m_pageAgent->enableInstrumentationForProcess(process, pageID);
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
    Ref process = provisionalFrame.process();
    auto pageID = protect(m_inspectedPage)->webPageIDInProcess(process);
    if (m_networkAgent && m_networkAgent->isEnabled())
        m_networkAgent->disableInstrumentationForProcess(process, pageID);
    if (m_pageAgent && m_pageAgent->isEnabled()) {
        m_pageAgent->disableInstrumentationForProcess(process, pageID);
        m_pageAgent->provisionalFrameDiscarded(protect(provisionalFrame.frame())->frameID(), process->coreProcessIdentifier());
    }
}

void WebPageInspectorController::didCommitProvisionalFrame(WebFrameProxy& frame, WebCore::ProcessIdentifier oldProcessID, std::optional<WebCore::PageIdentifier> oldPageID, WebCore::ProcessIdentifier newProcessID)
{
    if (!shouldManageFrameTargets())
        return;

    WebCore::FrameIdentifier frameID = frame.frameID();
    String oldTargetID = FrameInspectorTarget::toTargetID(frameID, oldProcessID);
    String newTargetID = FrameInspectorTarget::toTargetID(frameID, newProcessID);

    CheckedPtr targetAgent = m_targetAgent;

    // Under site isolation the frame's target may already be gone (for example a cross-site main-frame
    // commit wiped this page's frame targets before this late subframe commit arrived). Skip the target
    // notifications in that case, but still run the instrumentation migration below.
    if (CheckedPtr newTarget = m_targets.get(newTargetID)) {
        newTarget->didCommitProvisionalTarget();
        targetAgent->didCommitProvisionalTarget(oldTargetID, newTargetID);
    }

    // A same-process commit has oldTargetID == newTargetID; taking it would destroy what we just committed.
    if (oldTargetID != newTargetID) {
        if (auto oldTarget = m_targets.take(oldTargetID))
            targetAgent->targetDestroyed(protect(*oldTarget));
    }

    // The new process was registered in didCreateProvisionalFrame, and that registration now belongs to the
    // committed frame. Re-registering here would double-count it, so only release the old process; the frame
    // no longer lives there. See webkit.org/b/308896 and webkit.org/b/328708.
    RefPtr oldProcess = WebProcessProxy::processForIdentifier(oldProcessID);

    if (m_networkAgent && m_networkAgent->isEnabled()) {
        if (oldProcess && oldPageID)
            m_networkAgent->disableInstrumentationForProcess(*oldProcess, *oldPageID);
    }

    if (m_pageAgent && m_pageAgent->isEnabled()) {
        if (oldProcess && oldPageID)
            m_pageAgent->disableInstrumentationForProcess(*oldProcess, *oldPageID);
        if (oldProcessID != newProcessID)
            m_pageAgent->frameSwappedOutOfProcess(frameID, oldProcessID);
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
    m_agents.append(makeUniqueRef<InspectorStorageAgent>(webPageContext));

    if (protect(protect(m_inspectedPage)->preferences())->siteIsolationEnabled()) {
        // ProxyingNetworkAgent and ProxyingPageAgent are RefCounted (for IPC MessageReceiver)
        // so they can't be stored in AgentRegistry which expects UniqueRef ownership.
        // Their lifecycle (didCreateFrontendAndBackend / willDestroyFrontendAndBackend) is
        // managed explicitly in connectFrontend / disconnectFrontend / disconnectAllFrontends.
        lazyInitialize(m_networkAgent, adoptRef(*new Inspector::ProxyingNetworkAgent(webPageContext)));
        lazyInitialize(m_pageAgent, adoptRef(*new Inspector::ProxyingPageAgent(webPageContext)));
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

void WebPageInspectorController::setShowPaintRects(bool show)
{
    if (m_pageAgent)
        std::ignore = m_pageAgent->setShowPaintRects(show);
}

void WebPageInspectorController::timelineRecordingChanged(bool active)
{
    m_isTimelineRecording = active;

    // A screenshot that is still being taken belongs to a recording that has ended.
    finishTimelineScreenshot();
}

// The page target asks for this when it cannot paint every frame itself. WebPageProxy::takeSnapshot() draws the
// frames of every process (remote snapshotting), and only the UI process receives the image. So the record goes
// to the frontend from here, on behalf of the page target, with the time that the page target sent.
void WebPageInspectorController::takeTimelineScreenshot(const WebCore::IntRect& requestedRect, double timestamp, CompletionHandler<void()>&& completionHandler)
{
    // The page target asks only while it records, and waits for each screenshot before it asks for the next one.
    // Do not let a WebContent process start more.
    if (m_timelineScreenshotCompletionHandler || !m_isTimelineRecording || !hasFrontends()) {
        completionHandler();
        return;
    }

#if PLATFORM(COCOA)
    Ref inspectedPage = m_inspectedPage;

    // The rect is in the main frame's content coordinates. Do not let it be larger than what the view can show,
    // which is more than the view's size only when the page or the view is scaled down.
    double contentScale = inspectedPage->pageScaleFactor() * inspectedPage->viewScaleFactor();
    contentScale = contentScale > 0 ? std::clamp(contentScale, 0.1, 1.0) : 1;
    auto rect = requestedRect;
    rect.setSize(rect.size().shrunkTo(expandedIntSize(WebCore::FloatSize { inspectedPage->viewSize() }.scaled(1 / contentScale))));

    m_timelineScreenshotCompletionHandler = WTF::move(completionHandler);
    auto screenshotID = ++m_timelineScreenshotID;

    // A process that hosts a frame may never finish its part. Give up after a while, so that later screenshots still happen.
    RunLoop::mainSingleton().dispatchAfter(3_s, [weakPage = WeakPtr { inspectedPage }, screenshotID] {
        if (RefPtr page = weakPage; page && page->inspectorController().m_timelineScreenshotID == screenshotID)
            page->inspectorController().finishTimelineScreenshot();
    });

    inspectedPage->takeSnapshot(rect, { }, { }, [weakPage = WeakPtr { inspectedPage }, screenshotID, timestamp](CGImageRef image) {
        RefPtr page = weakPage;
        if (!page)
            return;

        // Ignore a screenshot that timed out, or whose recording has ended.
        auto& controller = page->inspectorController();
        if (controller.m_timelineScreenshotID != screenshotID || !controller.m_timelineScreenshotCompletionHandler)
            return;

        if (image)
            controller.sendTimelineScreenshotRecord(timestamp, encodeDataURL(image, "image/png"_s));
        controller.finishTimelineScreenshot();
    });
#else
    UNUSED_PARAM(requestedRect);
    UNUSED_PARAM(timestamp);
    completionHandler();
#endif
}

void WebPageInspectorController::finishTimelineScreenshot()
{
    if (auto completionHandler = std::exchange(m_timelineScreenshotCompletionHandler, { }))
        completionHandler();
}

void WebPageInspectorController::sendTimelineScreenshotRecord(double timestamp, const String& imageData)
{
    // pageClosed() discards the target agent.
    if (!m_targetAgent || !hasFrontends())
        return;

    // The same record as the page target sends for its own screenshots (see TimelineRecordFactory::createScreenshotData()).
    auto data = JSON::Object::create();
    data->setString("imageData"_s, imageData);

    auto record = JSON::Object::create();
    record->setString("type"_s, Inspector::Protocol::Helpers::getEnumConstantValue(Inspector::Protocol::Timeline::EventType::Screenshot));
    record->setDouble("startTime"_s, timestamp);
    record->setObject("data"_s, WTF::move(data));

    auto params = JSON::Object::create();
    params->setObject("record"_s, WTF::move(record));

    auto message = JSON::Object::create();
    message->setString("method"_s, "Timeline.eventRecorded"_s);
    message->setObject("params"_s, WTF::move(params));

    // Resolve the page target now: the main frame may have moved to another process since the page target asked.
    // Under Site Isolation all page and frame targets share one clock, so the time still fits.
    sendMessageToInspectorFrontend(PageInspectorTarget::toTargetID(protect(m_inspectedPage)->webPageIDInMainFrameProcess()), message->toJSONString());
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

// Unlike closing the inspector, this leaves the page target and other frame targets connected.
void WebPageInspectorController::disconnectFrameTargetForTesting(WebCore::FrameIdentifier frameID)
{
    RefPtr frame = WebFrameProxy::webFrame(frameID);
    if (!frame)
        return;

    if (CheckedPtr target = m_targets.get(getTargetID(*frame)))
        target->disconnect();
}

} // namespace WebKit
