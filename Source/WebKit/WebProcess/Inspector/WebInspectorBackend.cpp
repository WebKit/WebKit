/*
 * Copyright (C) 2010-2025 Apple Inc. All rights reserved.
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
#include "WebInspectorBackend.h"

#include "FrameNetworkAgentProxy.h"
#include "PageAgentProxy.h"
#include "WebFrame.h"
#include "WebInspectorBackendMessages.h"
#include "WebInspectorBackendProxyMessages.h"
#include "WebInspectorUIMessages.h"
#include "WebPage.h"
#include "WebProcess.h"
#include <JavaScriptCore/ContentSearchUtilities.h>
#include <JavaScriptCore/InspectorProtocolObjects.h>
#include <JavaScriptCore/RegularExpression.h>
#include <WebCore/CachedResource.h>
#include <WebCore/Chrome.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/DocumentView.h>
#include <WebCore/FrameInspectorController.h>
#include <WebCore/FrameLoadRequest.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/InspectorFrontendClient.h>
#include <WebCore/InspectorIdentifierRegistry.h>
#include <WebCore/InspectorPageAgent.h>
#include <WebCore/InspectorResourceUtilities.h>
#include <WebCore/LocalFrame.h>
#include <WebCore/LocalFrameInlines.h>
#include <WebCore/LocalFrameView.h>
#include <WebCore/NavigationAction.h>
#include <WebCore/NotImplemented.h>
#include <WebCore/Page.h>
#include <WebCore/PageInspectorController.h>
#include <WebCore/ScriptController.h>
#include <WebCore/Settings.h>
#include <WebCore/WebInjectedScriptManager.h>
#include <WebCore/WindowFeatures.h>
#include <wtf/Borrow.h>
#include <wtf/HashSet.h>
#include <wtf/URL.h>
#include <wtf/text/StringHash.h>

static const float minimumAttachedHeight = 250;
static const float maximumAttachedHeightRatio = 0.75;
static const float minimumAttachedWidth = 500;

namespace WebKit {
using namespace WebCore;

Ref<WebInspectorBackend> WebInspectorBackend::create(WebPage& page)
{
    return adoptRef(*new WebInspectorBackend(page));
}

WebInspectorBackend::WebInspectorBackend(WebPage& page)
    : m_page(page)
    , m_resourceDataStore(makeUniqueRef<BackendResourceDataStore>(BackendResourceDataStore::Settings { }))
{
}

WebInspectorBackend::~WebInspectorBackend()
{
    disableNetworkInstrumentation();

    if (RefPtr frontendConnection = m_frontendConnection)
        frontendConnection->invalidate();
}

WebPage* WebInspectorBackend::page() const
{
    return m_page.get();
}

void WebInspectorBackend::openLocalInspectorFrontend()
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::RequestOpenLocalInspectorFrontend(), m_page->identifier());
}

void WebInspectorBackend::setFrontendConnection(IPC::Connection::Handle&& connectionHandle)
{
    // We might receive multiple updates if this web process got swapped into a WebPageProxy
    // shortly after another process established the connection.
    if (RefPtr frontendConnection = std::exchange(m_frontendConnection, nullptr))
        frontendConnection->invalidate();

    if (!connectionHandle)
        return;

    Ref frontendConnection = IPC::Connection::createClientConnection(IPC::Connection::Identifier { WTF::move(connectionHandle) });
    m_frontendConnection = frontendConnection.copyRef();
    frontendConnection->open(*this);

    for (auto& callback : borrow(m_frontendConnectionActions).get())
        callback(frontendConnection.get());
    m_frontendConnectionActions.clear();
}

void WebInspectorBackend::closeFrontendConnection()
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::DidClose(), m_page->identifier());

    // If we tried to close the frontend before it was created, then no connection exists yet.
    if (RefPtr frontendConnection = m_frontendConnection) {
        frontendConnection->invalidate();
        m_frontendConnection = nullptr;
    }

    m_frontendConnectionActions.clear();

    m_attached = false;
    m_previousCanAttach = false;
}

void WebInspectorBackend::bringToFront()
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::BringToFront(), m_page->identifier());
}

void WebInspectorBackend::whenFrontendConnectionEstablished(Function<void(IPC::Connection&)>&& callback)
{
    if (RefPtr connection = m_frontendConnection) {
        callback(*connection);
        return;
    }

    m_frontendConnectionActions.append(WTF::move(callback));
}

// Called by WebInspectorBackend messages
void WebInspectorBackend::show(CompletionHandler<void(bool success)>&& completionHandler)
{
    if (!m_page->corePage()) {
        completionHandler(false);
        return;
    }

    m_page->corePage()->inspectorController().show();
    completionHandler(true);
}

void WebInspectorBackend::close()
{
    if (!m_page->corePage())
        return;

    // Close could be called multiple times during teardown.
    if (!m_frontendConnection)
        return;

    closeFrontendConnection();
}

void WebInspectorBackend::evaluateScriptForTest(const String& script)
{
    if (!m_page->corePage())
        return;

    m_page->corePage()->inspectorController().evaluateForTestInFrontend(script);
}

void WebInspectorBackend::showConsole()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::ShowConsole(), 0);
    });
}

void WebInspectorBackend::showResources()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::ShowResources(), 0);
    });
}

void WebInspectorBackend::showMainResourceForFrame(WebCore::FrameIdentifier frameIdentifier)
{
    RefPtr frame = WebProcess::singleton().webFrame(frameIdentifier);
    if (!frame)
        return;

    if (!m_page->corePage())
        return;

    String inspectorFrameIdentifier = CheckedRef { m_page->corePage()->inspectorController().ensurePageAgent() }->frameId(protect(frame->coreLocalFrame()).get());

    whenFrontendConnectionEstablished([inspectorFrameIdentifier](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::ShowMainResourceForFrame(inspectorFrameIdentifier), 0);
    });
}

void WebInspectorBackend::startPageProfiling()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StartPageProfiling(), 0);
    });
}

void WebInspectorBackend::stopPageProfiling()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StopPageProfiling(), 0);
    });
}

void WebInspectorBackend::startElementSelection()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StartElementSelection(), 0);
    });
}

void WebInspectorBackend::stopElementSelection()
{
    if (!m_page->corePage())
        return;

    whenFrontendConnectionEstablished([](auto& frontendConnection) {
        frontendConnection.send(Messages::WebInspectorUI::StopElementSelection(), 0);
    });
}

void WebInspectorBackend::elementSelectionChanged(bool active)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::ElementSelectionChanged(active), m_page->identifier());
}

void WebInspectorBackend::timelineRecordingChanged(bool active)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::TimelineRecordingChanged(active), m_page->identifier());
}

void WebInspectorBackend::setDeveloperPreferenceOverride(InspectorBackendClient::DeveloperPreference developerPreference, std::optional<bool> overrideValue)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::SetDeveloperPreferenceOverride(developerPreference, overrideValue), m_page->identifier());
}

#if ENABLE(INSPECTOR_NETWORK_THROTTLING)

void WebInspectorBackend::setEmulatedConditions(std::optional<int64_t>&& bytesPerSecondLimit)
{
    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::SetEmulatedConditions(WTF::move(bytesPerSecondLimit)), m_page->identifier());
}

#endif // ENABLE(INSPECTOR_NETWORK_THROTTLING)

// FIXME <https://webkit.org/b/283435>: Remove this unused canAttachWindow function. Its return value is no longer used
// or respected by the UI process.
bool WebInspectorBackend::canAttachWindow()
{
    if (!m_page->corePage())
        return false;

    // Don't allow attaching to another inspector -- two inspectors in one window is too much!
    if (m_page->isInspectorPage())
        return false;

    // If we are already attached, allow attaching again to allow switching sides.
    if (m_attached)
        return true;

    // Don't allow the attach if the window would be too small to accommodate the minimum inspector size.
    RefPtr localMainFrame = RefPtr { m_page.get() }->localMainFrame();
    if (!localMainFrame)
        return false;
    unsigned inspectedPageHeight = protect(localMainFrame->view())->visibleHeight();
    unsigned inspectedPageWidth = protect(localMainFrame->view())->visibleWidth();
    unsigned maximumAttachedHeight = inspectedPageHeight * maximumAttachedHeightRatio;
    return minimumAttachedHeight <= maximumAttachedHeight && minimumAttachedWidth <= inspectedPageWidth;
}

void WebInspectorBackend::updateDockingAvailability()
{
    if (m_attached)
        return;

    bool canAttachWindow = this->canAttachWindow();
    if (m_previousCanAttach == canAttachWindow)
        return;

    m_previousCanAttach = canAttachWindow;

    protect(WebProcess::singleton().parentProcessConnection())->send(Messages::WebInspectorBackendProxy::AttachAvailabilityChanged(canAttachWindow), m_page->identifier());
}

void WebInspectorBackend::ensureInstrumentationForFrame(LocalFrame& frame)
{
    ensureNetworkInstrumentationForFrame(frame);
    ensurePageInstrumentationForFrame(frame);
}

void WebInspectorBackend::ensureNetworkInstrumentationForFrame(LocalFrame& frame)
{
    if (!m_networkInstrumentationEnabled)
        return;

    auto frameID = frame.frameID();
    if (m_frameNetworkAgentProxies.contains(frameID))
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr corePage = page->corePage();
    if (!corePage)
        return;

    auto& pageInspectorController = corePage->inspectorController();
    auto& frameController = frame.inspectorController();
    Inspector::AgentContext baseContext = {
        frameController,
        pageInspectorController.injectedScriptManager(),
        pageInspectorController.frontendRouter(),
        pageInspectorController.backendDispatcher()
    };
    Ref instrumentingAgents = frameController.instrumentingAgents();
    WebAgentContext webContext = {
        baseContext,
        instrumentingAgents.get()
    };

    CheckedRef resourceDataStore = m_resourceDataStore.get();
    auto proxy = makeUnique<FrameNetworkAgentProxy>(webContext, *page, resourceDataStore.get());
    proxy->enable();
    m_frameNetworkAgentProxies.add(frameID, WTF::move(proxy));
}

void WebInspectorBackend::enableNetworkInstrumentation()
{
    if (!m_page)
        return;

    RefPtr corePage = m_page->corePage();
    if (!corePage)
        return;

    if (!m_networkInstrumentationEnabled) {
        m_networkInstrumentationEnabled = true;
        corePage->settings().setDeveloperExtrasEnabled(true);
        corePage->inspectorController().connectRemoteInstrumentation();
    }

    corePage->forEachLocalFrame([&](LocalFrame& frame) {
        ensureNetworkInstrumentationForFrame(frame);
    });
}

void WebInspectorBackend::disableNetworkInstrumentation()
{
    if (!m_networkInstrumentationEnabled)
        return;

    m_frameNetworkAgentProxies.clear();
    m_networkInstrumentationEnabled = false;

    if (!m_page)
        return;

    if (RefPtr corePage = m_page->corePage())
        corePage->inspectorController().disconnectRemoteInstrumentation();
}

void WebInspectorBackend::removeInstrumentationForFrame(FrameIdentifier frameID)
{
    m_frameNetworkAgentProxies.remove(frameID);
    m_framePageAgentProxies.remove(frameID);
}

void WebInspectorBackend::getResponseBody(ResourceLoaderIdentifier resourceID, CompletionHandler<void(String content, bool base64Encoded, String errorString)>&& completionHandler)
{
    CheckedRef resourceDataStore = m_resourceDataStore.get();
    auto result = resourceDataStore->getResponseBody(resourceID);
    if (result.has_value()) {
        auto& [content, base64Encoded] = result.value();
        completionHandler(content, base64Encoded, String());
    } else
        completionHandler(String(), false, result.error());
}

// Convert the JSON protocol matches from ContentSearchUtilities::searchInTextByLines into the plain
// typed-IPC mirror, so the UIProcess rebuilds the protocol objects on its side (as with FrameResource).
static Vector<Inspector::SearchMatch> convertSearchMatches(JSON::ArrayOf<Inspector::Protocol::GenericTypes::SearchMatch>& matches)
{
    Vector<Inspector::SearchMatch> result;
    auto length = matches.length();
    result.reserveInitialCapacity(length);
    for (size_t i = 0; i < length; ++i) {
        RefPtr object = matches.get(i)->asObject();
        if (!object)
            continue;
        Inspector::SearchMatch match;
        if (auto lineNumber = object->getInteger("lineNumber"_s))
            match.lineNumber = *lineNumber;
        match.lineContent = object->getString("lineContent"_s);
        result.append(WTF::move(match));
    }
    return result;
}

void WebInspectorBackend::searchInRequest(WebCore::ResourceLoaderIdentifier resourceID, const String& query, bool caseSensitive, bool isRegex, CompletionHandler<void(Vector<Inspector::SearchMatch>&&, String errorString)>&& completionHandler)
{
    // Mirrors InspectorNetworkAgent::searchInRequest, reading from this process's
    // BackendResourceDataStore (the Site Isolation analog of NetworkResourcesData).
    CheckedRef resourceDataStore = m_resourceDataStore.get();
    auto const* resourceData = resourceDataStore->data(resourceID);
    if (!resourceData) {
        completionHandler({ }, "Missing resource for given requestId"_s);
        return;
    }

    if (!resourceData->hasContent()) {
        completionHandler({ }, "Missing content of resource for given requestId"_s);
        return;
    }

    if (resourceData->base64Encoded()) {
        completionHandler({ }, "Search not supported on base64-encoded resource"_s);
        return;
    }

    auto matches = Inspector::ContentSearchUtilities::searchInTextByLines(resourceData->content(), query, caseSensitive, isRegex);
    completionHandler(convertSearchMatches(matches.get()), String());
}

void WebInspectorBackend::searchInFrameResource(WebCore::FrameIdentifier frameID, const String& url, const String& query, bool caseSensitive, bool isRegex, CompletionHandler<void(Vector<Inspector::SearchMatch>&&, String errorString)>&& completionHandler)
{
    // Mirrors the frame+URL half of InspectorPageAgent::searchInResource: find the resource by URL
    // (main resource vs. cached resource) in this process's copy of the frame, then search its text.
    RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
    if (!webFrame) {
        completionHandler({ }, "Frame not found in this process"_s);
        return;
    }
    RefPtr localFrame = webFrame->coreLocalFrame();
    if (!localFrame) {
        completionHandler({ }, "Frame is not local to this process"_s);
        return;
    }

    RefPtr loader = localFrame->loader().documentLoader();
    if (!loader) {
        completionHandler({ }, "No document loader for frame"_s);
        return;
    }

    URL parsedURL({ }, url);

    String content;
    bool success = false;
    if (equalIgnoringFragmentIdentifier(parsedURL, loader->url()))
        success = Inspector::ResourceUtilities::mainResourceContent(localFrame.get(), false, &content);

    if (!success) {
        if (RefPtr resource = Inspector::ResourceUtilities::cachedResource(localFrame.get(), parsedURL)) {
            if (auto textContent = Inspector::ResourceUtilities::textContentForCachedResource(*resource)) {
                content = *textContent;
                success = true;
            }
        }
    }

    if (!success) {
        completionHandler({ }, String());
        return;
    }

    auto matches = Inspector::ContentSearchUtilities::searchInTextByLines(content, query, caseSensitive, isRegex);
    completionHandler(convertSearchMatches(matches.get()), String());
}

void WebInspectorBackend::searchInFramesAndRequests(Vector<WebCore::FrameIdentifier>&& frameIDs, const String& query, bool caseSensitive, bool isRegex, CompletionHandler<void(Vector<Inspector::SearchResult>&&)>&& completionHandler)
{
    // Combines both halves of InspectorPageAgent::searchInResources for the frames this process
    // hosts: match each frame's cached subresources, and also this process's BackendResourceDataStore
    // (XHR/Fetch bodies). Consolidating them here collapses the legacy Page->Network cross-agent call.
    Vector<Inspector::SearchResult> results;

    // Track URLs seen in the cached-resource walk so the data-store walk can skip them: a resource
    // that is both a CachedResource and a tracked request is reported once. This intentionally
    // diverges from the legacy path, which double-counts it.
    // Caveat: the join key is a URL string, and a redirected resource has different URLs in the two
    // walks (request vs. final URL), so it can still be counted twice.
    HashSet<String> cachedResourceURLs;

    auto searchType = isRegex ? Inspector::ContentSearchUtilities::SearchType::Regex : Inspector::ContentSearchUtilities::SearchType::ContainsString;
    auto searchCaseSensitive = caseSensitive ? Inspector::ContentSearchUtilities::SearchCaseSensitive::Yes : Inspector::ContentSearchUtilities::SearchCaseSensitive::No;
    auto regex = Inspector::ContentSearchUtilities::createRegularExpressionForString(query, searchType, searchCaseSensitive);

    for (auto frameID : frameIDs) {
        RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
        if (!webFrame)
            continue;
        RefPtr localFrame = webFrame->coreLocalFrame();
        if (!localFrame)
            continue;

        for (RefPtr cachedResource : Inspector::ResourceUtilities::cachedResourcesForFrame(localFrame.get())) {
            auto textContent = Inspector::ResourceUtilities::textContentForCachedResource(*cachedResource);
            if (!textContent)
                continue;
            auto urlString = cachedResource->url().string();
            cachedResourceURLs.add(urlString);
            int matchesCount = Inspector::ContentSearchUtilities::countRegularExpressionMatches(regex, *textContent);
            if (!matchesCount)
                continue;
            Inspector::SearchResult result;
            result.url = urlString;
            result.frameID = frameID;
            result.matchesCount = matchesCount;
            results.append(WTF::move(result));
        }
    }

    CheckedRef resourceDataStore = m_resourceDataStore.get();
    resourceDataStore->forEach([&](const BackendResourceDataStore::ResourceData& entry) {
        if (!entry.hasContent() || entry.base64Encoded())
            return;
        // Skip entries already emitted by the cached-resource walk to avoid double-counting
        // (e.g. a stylesheet is both a CachedResource and a buffered data-store entry).
        if (cachedResourceURLs.contains(entry.url()))
            return;
        int matchesCount = Inspector::ContentSearchUtilities::countRegularExpressionMatches(regex, entry.content());
        if (!matchesCount)
            return;
        Inspector::SearchResult result;
        result.url = entry.url();
        if (auto frameID = entry.frameID())
            result.frameID = *frameID;
        result.matchesCount = matchesCount;
        result.resourceID = entry.resourceID();
        results.append(WTF::move(result));
    });

    completionHandler(WTF::move(results));
}

void WebInspectorBackend::ensurePageInstrumentationForFrame(LocalFrame& frame)
{
    if (!m_pageInstrumentationEnabled)
        return;

    auto frameID = frame.frameID();
    if (m_framePageAgentProxies.contains(frameID))
        return;

    RefPtr page = m_page.get();
    if (!page)
        return;

    RefPtr corePage = page->corePage();
    if (!corePage)
        return;

    // Register the PageAgentProxy on the frame's OWN InstrumentingAgents (mirroring
    // FrameNetworkAgentProxy in ensureNetworkInstrumentationForFrame), rather than the page's.
    // The frame's first commit dispatches frameNavigated via instrumentingAgents(frame),
    // which resolves the frame's own InstrumentingAgents; setting the slot there fires
    // the proxy directly without depending on the frame->page fallback. Under Site
    // Isolation a cross-origin child loads in a brand-new process whose page-level slot
    // is set up too late / via a fallback that doesn't fire, so the page-level + fallback
    // model never delivered the child's initial frameNavigated. See webkit.org/b/308896.
    auto& pageInspectorController = corePage->inspectorController();
    auto& frameController = frame.inspectorController();
    Inspector::AgentContext baseContext = {
        frameController,
        pageInspectorController.injectedScriptManager(),
        pageInspectorController.frontendRouter(),
        pageInspectorController.backendDispatcher()
    };
    Ref instrumentingAgents = frameController.instrumentingAgents();
    WebAgentContext webContext = {
        baseContext,
        instrumentingAgents.get()
    };

    auto proxy = makeUnique<PageAgentProxy>(webContext, *page);
    proxy->enable();
    m_framePageAgentProxies.add(frameID, WTF::move(proxy));
}

void WebInspectorBackend::enablePageInstrumentation()
{
    if (!m_page || !m_page->corePage())
        return;

    if (m_pageInstrumentationEnabled)
        return;

    m_pageInstrumentationEnabled = true;

    RefPtr corePage = m_page->corePage();
    corePage->settings().setDeveloperExtrasEnabled(true);

    corePage->forEachLocalFrame([&](LocalFrame& frame) {
        ensurePageInstrumentationForFrame(frame);
    });
}

void WebInspectorBackend::disablePageInstrumentation()
{
    if (!m_pageInstrumentationEnabled)
        return;

    // Clearing the map destroys each PageAgentProxy, whose destructor (via disable())
    // clears the enabledPageProxy slot on its frame's InstrumentingAgents. Without that,
    // a later frame commit in this process would dereference a freed pointer from
    // InspectorInstrumentation. The page is still alive here (this is an explicit
    // DisablePageInstrumentation IPC, not process teardown).
    m_framePageAgentProxies.clear();
    m_pageInstrumentationEnabled = false;
}


void WebInspectorBackend::getFrameResourceData(Vector<WebCore::FrameIdentifier>&& frameIDs, CompletionHandler<void(Vector<std::pair<WebCore::FrameIdentifier, Inspector::FrameResourceData>>&&)>&& completionHandler)
{
    // Return, for each requested frame that is local to this WebContent process, its committed
    // document's loaderId (as a ScriptExecutionContextIdentifier) and cached subresources. The
    // UIProcess ProxyingPageAgent walks the authoritative cross-process frame tree, groups frame
    // IDs by hosting process, and asks each process only for the frames it hosts; it then builds
    // the Page.getResourceTree protocol objects from this typed data under Site Isolation. Frames
    // not local to this process are silently skipped (another process answers for them).
    // See webkit.org/b/308896.
    Vector<std::pair<WebCore::FrameIdentifier, Inspector::FrameResourceData>> resourcesByFrame;
    resourcesByFrame.reserveInitialCapacity(frameIDs.size());

    for (auto frameID : frameIDs) {
        RefPtr webFrame = WebProcess::singleton().webFrame(frameID);
        if (!webFrame)
            continue;
        RefPtr localFrame = webFrame->coreLocalFrame();
        if (!localFrame)
            continue;

        Inspector::FrameResourceData frameData;
        if (RefPtr document = localFrame->document())
            frameData.loaderId = document->identifier();
        frameData.resources = Inspector::ResourceUtilities::buildResourceDataForFrame(*localFrame);
        resourcesByFrame.append({ frameID, WTF::move(frameData) });
    }

    completionHandler(WTF::move(resourcesByFrame));
}

} // namespace WebKit
