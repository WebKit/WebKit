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
#include "ProxyingPageAgent.h"

#include "HandleMessage.h"
#include "ProxyingPageAgentMessages.h"
#include "WebFrameProxy.h"
#include "WebInspectorBackendMessages.h"
#include "WebPageProxy.h"
#include "WebProcessProxy.h"
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <WebCore/InspectorIdentifierRegistry.h>
#include <WebCore/SecurityOriginData.h>
#include <wtf/TZoneMallocInlines.h>

namespace Inspector {

using namespace WebKit;
using namespace WebCore;

WTF_MAKE_TZONE_ALLOCATED_IMPL(ProxyingPageAgent);

ProxyingPageAgent::ProxyingPageAgent(WebPageAgentContext& context)
    : InspectorAgentBase("Page"_s, context)
    , m_frontendDispatcher(makeUniqueRef<PageFrontendDispatcher>(context.frontendRouter))
    , m_backendDispatcher(PageBackendDispatcher::create(context.backendDispatcher, this))
    , m_inspectedPage(context.inspectedPage)
{
}

ProxyingPageAgent::~ProxyingPageAgent()
{
    // Backstop in case Inspector teardown bypasses willDestroyFrontendAndBackend().
    removeAllRegisteredReceivers();
}

void ProxyingPageAgent::removeAllRegisteredReceivers()
{
    // Use the pinned WebProcessProxy refs rather than WebProcessProxy::processForIdentifier(),
    // which can return null when the process has been destructed but its receiver map entry
    // has not yet been torn down. The pin guarantees the process (and its receiver map) is
    // still alive here so removeMessageReceiver() can decrement m_messageReceiverMapCount.
    for (auto& [key, _] : std::exchange(m_instrumentedProcessPageCounts, { })) {
        auto [processID, pageID] = key;
        auto it = m_pinnedInstrumentedProcesses.find(processID);
        ASSERT(it != m_pinnedInstrumentedProcesses.end());
        if (it == m_pinnedInstrumentedProcesses.end())
            continue;
        Ref webProcess = it->value;
        webProcess->removeMessageReceiver(Messages::ProxyingPageAgent::messageReceiverName(), pageID);
    }
    m_pinnedInstrumentedProcesses.clear();
}

// MARK: - IPC event handlers

// Resolve a frame's protocol ID from the authoritative UIProcess frame tree so the
// hosting process matches buildFrameTree() and the WebContent agents. The events
// carry only a FrameIdentifier; the hosting process can differ from the identifier's
// creating process after a process swap. Falls back to the identifier-derived process
// when the frame is no longer in the tree (e.g. already detached). See webkit.org/b/310164.
static String protocolFrameIdForFrameID(FrameIdentifier frameID)
{
    if (RefPtr frame = WebFrameProxy::webFrame(frameID))
        return IdentifierRegistry::protocolFrameId(frameID, frame->process().coreProcessIdentifier());
    return IdentifierRegistry::protocolFrameId(frameID);
}

void ProxyingPageAgent::frameNavigated(FrameIdentifier frameID, const URL& url, const String& mimeType, SecurityOriginData&& securityOrigin, std::optional<FrameIdentifier> parentFrameID, const String& name)
{
    auto frameObject = Protocol::Page::Frame::create()
        .setId(protocolFrameIdForFrameID(frameID))
        .setLoaderId(String()) // FIXME: <https://webkit.org/b/308895> get loaderId from document identifier
        .setUrl(url.string())
        .setMimeType(mimeType)
        .setSecurityOrigin(securityOrigin.toString())
        .release();

    if (parentFrameID)
        frameObject->setParentId(protocolFrameIdForFrameID(*parentFrameID));
    if (!name.isEmpty())
        frameObject->setName(name);

    m_frontendDispatcher->frameNavigated(WTF::move(frameObject));
}

void ProxyingPageAgent::domContentEventFired(double timestamp)
{
    m_frontendDispatcher->domContentEventFired(timestamp);
}

void ProxyingPageAgent::loadEventFired(double timestamp)
{
    m_frontendDispatcher->loadEventFired(timestamp);
}

void ProxyingPageAgent::frameDetached(FrameIdentifier frameID)
{
    m_frontendDispatcher->frameDetached(protocolFrameIdForFrameID(frameID));
}

// MARK: - Frontend lifecycle

void ProxyingPageAgent::didCreateFrontendAndBackend()
{
    enable();
}

void ProxyingPageAgent::willDestroyFrontendAndBackend(DisconnectReason)
{
    disable();
}

// MARK: - Enable / disable IPC flow

void ProxyingPageAgent::enableInstrumentationForProcess(WebProcessProxy& webProcess, PageIdentifier pageID)
{
    auto key = std::make_pair(webProcess.coreProcessIdentifier(), pageID);
    auto result = m_instrumentedProcessPageCounts.add(key, 0);
    if (++result.iterator->value > 1)
        return;

    m_pinnedInstrumentedProcesses.ensure(webProcess.coreProcessIdentifier(), [&] {
        return Ref { webProcess };
    });
    webProcess.addMessageReceiver(Messages::ProxyingPageAgent::messageReceiverName(), pageID, *this);
    webProcess.send(Messages::WebInspectorBackend::EnablePageInstrumentation { }, pageID);
}

void ProxyingPageAgent::disableInstrumentationForProcess(WebProcessProxy& webProcess, PageIdentifier pageID)
{
    auto processID = webProcess.coreProcessIdentifier();
    auto key = std::make_pair(processID, pageID);
    auto it = m_instrumentedProcessPageCounts.find(key);
    if (it == m_instrumentedProcessPageCounts.end())
        return;

    if (--it->value > 0)
        return;

    m_instrumentedProcessPageCounts.remove(it);
    webProcess.send(Messages::WebInspectorBackend::DisablePageInstrumentation { }, pageID);
    webProcess.removeMessageReceiver(Messages::ProxyingPageAgent::messageReceiverName(), pageID);

    // Drop the pin once this process has no remaining page registrations.
    bool processStillHasRegistrations = false;
    for (auto& entry : m_instrumentedProcessPageCounts) {
        if (entry.key.first == processID) {
            processStillHasRegistrations = true;
            break;
        }
    }
    if (!processStillHasRegistrations)
        m_pinnedInstrumentedProcesses.remove(processID);
}

CommandResult<void> ProxyingPageAgent::enable()
{
    Ref<WebPageProxy> inspectedPage = m_inspectedPage.get();
    Ref preferences = inspectedPage->preferences();
    if (!preferences->siteIsolationEnabled())
        return { };

    m_enabled = true;

    inspectedPage->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
        Ref protectedWebProcess { webProcess };
        enableInstrumentationForProcess(protectedWebProcess, pageID);
    });

    return { };
}

CommandResult<void> ProxyingPageAgent::disable()
{
    if (!m_enabled)
        return { };

    m_enabled = false;

    // Force-teardown: disable all processes unconditionally, bypassing the
    // refcount discipline in disableInstrumentationForProcess(). This is
    // correct because disable() is called when the Page domain is torn
    // down entirely -- no per-frame refcount preservation is needed.
    //
    // Iterate the registration map, not forEachWebContentProcess(): under
    // Site Isolation a process may have swapped out while still holding our
    // message receiver, in which case forEachWebContentProcess() would no
    // longer enumerate it. The pinned process refs (see m_pinnedInstrumentedProcesses)
    // keep the WebProcessProxy alive long enough for the send + remove below.
    for (auto& [key, _] : m_instrumentedProcessPageCounts) {
        auto [processID, pageID] = key;
        auto it = m_pinnedInstrumentedProcesses.find(processID);
        ASSERT(it != m_pinnedInstrumentedProcesses.end());
        if (it == m_pinnedInstrumentedProcesses.end())
            continue;
        Ref webProcess = it->value;
        webProcess->send(Messages::WebInspectorBackend::DisablePageInstrumentation { }, pageID);
    }
    removeAllRegisteredReceivers();

    return { };
}

// MARK: - Stubbed command handlers

Ref<Protocol::Page::FrameResourceTree> ProxyingPageAgent::buildFrameTree(const WebFrameProxy& frame, const String* parentProtocolId) const
{
    auto protocolId = IdentifierRegistry::protocolFrameId(frame.frameID(), frame.process().coreProcessIdentifier());

    // The UIProcess WebFrameProxy tree is the authoritative cross-process frame
    // tree under Site Isolation: childFrames() spans every WebContent process, so
    // walking it yields the full structure (frame ids, parent linkage, name)
    // regardless of which process hosts each frame.
    //
    // FIXME: <https://webkit.org/b/308896> url()/documentSecurityOriginData() are
    // still stale for cross-origin children because the inspectedPage's
    // WebFrameProxy never observes their owning process's didCommitLoadForFrame.
    // WebFrameProxy state propagation across processes is the remaining follow-up.
    SecurityOriginData securityOrigin = frame.documentSecurityOriginData();
    String mimeType = frame.mimeType();
    String name = frame.frameName();

    auto frameObject = Protocol::Page::Frame::create()
        .setId(protocolId)
        .setLoaderId(emptyString()) // FIXME: <https://webkit.org/b/308895> get loaderId from document identifier
        .setUrl(frame.url().string())
        .setMimeType(mimeType.isEmpty() ? "text/html"_s : mimeType)
        .setSecurityOrigin(securityOrigin.toString())
        .release();

    if (parentProtocolId)
        frameObject->setParentId(*parentProtocolId);
    if (!name.isEmpty())
        frameObject->setName(name);

    auto result = Protocol::Page::FrameResourceTree::create()
        .setFrame(WTF::move(frameObject))
        .setResources(JSON::ArrayOf<Protocol::Page::FrameResource>::create())
        .release();

    if (!frame.childFrames().isEmpty()) {
        auto childrenArray = JSON::ArrayOf<Protocol::Page::FrameResourceTree>::create();
        for (auto& child : frame.childFrames())
            childrenArray->addItem(buildFrameTree(child, &protocolId));
        result->setChildFrames(WTF::move(childrenArray));
    }

    return result;
}

CommandResult<Ref<Protocol::Page::FrameResourceTree>> ProxyingPageAgent::getResourceTree()
{
    if (!m_enabled)
        return makeUnexpected("Not supported without Site Isolation"_s);

    Ref inspectedPage = m_inspectedPage.get();
    RefPtr mainFrame = inspectedPage->mainFrame();
    if (!mainFrame)
        return makeUnexpected("Missing main frame"_s);

    return buildFrameTree(*mainFrame, nullptr);
}

// FIXME: <https://webkit.org/b/308898> Forward emulation overrides to all WebContent processes.
CommandResult<void> ProxyingPageAgent::reload(std::optional<bool>&&, std::optional<bool>&&)
{
    return { };
}

CommandResult<void> ProxyingPageAgent::overrideUserAgent(const String&)
{
    return { };
}

CommandResult<void> ProxyingPageAgent::overrideSetting(Protocol::Page::Setting, std::optional<bool>&&)
{
    return { };
}

CommandResult<void> ProxyingPageAgent::overrideUserPreference(Protocol::Page::UserPreferenceName, std::optional<Protocol::Page::UserPreferenceValue>&&)
{
    return { };
}

// FIXME: <https://webkit.org/b/308900> Forward cookie commands to correct WebContent process.
CommandResult<Ref<JSON::ArrayOf<Protocol::Page::Cookie>>> ProxyingPageAgent::getCookies()
{
    return makeUnexpected("Not yet implemented under Site Isolation"_s);
}

CommandResult<void> ProxyingPageAgent::setCookie(Ref<JSON::Object>&&, std::optional<bool>&&)
{
    return { };
}

CommandResult<void> ProxyingPageAgent::deleteCookie(const String&, const String&)
{
    return { };
}

CommandResultOf<String, bool> ProxyingPageAgent::getResourceContent(const Protocol::Network::FrameId&, const String&)
{
    return makeUnexpected("Not yet implemented under Site Isolation"_s);
}

// FIXME: <https://webkit.org/b/308897> Cross-process bootstrap script injection.
CommandResult<void> ProxyingPageAgent::setBootstrapScript(const String&)
{
    return { };
}

// FIXME: <https://webkit.org/b/308896> Cross-process resource search with result aggregation.
CommandResult<Ref<JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>>> ProxyingPageAgent::searchInResource(const Protocol::Network::FrameId&, const String&, const String&, std::optional<bool>&&, std::optional<bool>&&, const Protocol::Network::RequestId&)
{
    return JSON::ArrayOf<Protocol::GenericTypes::SearchMatch>::create();
}

CommandResult<Ref<JSON::ArrayOf<Protocol::Page::SearchResult>>> ProxyingPageAgent::searchInResources(const String&, std::optional<bool>&&, std::optional<bool>&&)
{
    return JSON::ArrayOf<Protocol::Page::SearchResult>::create();
}

// FIXME: <https://webkit.org/b/308899> Forward overlay state to all WebContent processes.
#if !PLATFORM(IOS_FAMILY)
CommandResult<void> ProxyingPageAgent::setShowRulers(bool)
{
    return { };
}
#endif

CommandResult<void> ProxyingPageAgent::setShowPaintRects(bool)
{
    return { };
}

CommandResult<void> ProxyingPageAgent::setEmulatedMedia(const String&)
{
    return { };
}

CommandResult<String> ProxyingPageAgent::snapshotNode(Protocol::DOM::NodeId)
{
    return makeUnexpected("Not yet implemented under Site Isolation"_s);
}

CommandResult<String> ProxyingPageAgent::snapshotRect(int, int, int, int, Protocol::Page::CoordinateSystem)
{
    return makeUnexpected("Not yet implemented under Site Isolation"_s);
}

#if ENABLE(WEB_ARCHIVE) && USE(CF)
CommandResult<String> ProxyingPageAgent::archive()
{
    return makeUnexpected("Not yet implemented under Site Isolation"_s);
}
#endif

#if !PLATFORM(COCOA)
CommandResult<void> ProxyingPageAgent::setScreenSizeOverride(std::optional<int>&&, std::optional<int>&&)
{
    return { };
}
#endif

} // namespace Inspector
