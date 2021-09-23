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
#include "InspectorPlaywrightAgent.h"

#if ENABLE(REMOTE_INSPECTOR)

#include "APIPageConfiguration.h"
#include "FrameInfoData.h"
#include "InspectorPlaywrightAgentClient.h"
#include "InspectorTargetProxy.h"
#include "NetworkProcessMessages.h"
#include "NetworkProcessProxy.h"
#include "PageClient.h"
#include "StorageNamespaceIdentifier.h"
#include "WebAutomationSession.h"
#include "WebGeolocationManagerProxy.h"
#include "WebGeolocationPosition.h"
#include "WebInspectorUtilities.h"
#include "WebPageGroup.h"
#include "WebPageInspectorController.h"
#include "WebPageInspectorTarget.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebProcessProxy.h"
#include "WebsiteDataRecord.h"
#include <WebCore/FrameIdentifier.h>
#include <WebCore/GeolocationPositionData.h>
#include <WebCore/InspectorPageAgent.h>
#include <WebCore/ProcessIdentifier.h>
#include <WebCore/ResourceRequest.h>
#include <WebCore/SecurityOriginData.h>
#include <WebCore/WindowFeatures.h>
#include <JavaScriptCore/InspectorBackendDispatcher.h>
#include <JavaScriptCore/InspectorFrontendChannel.h>
#include <JavaScriptCore/InspectorFrontendRouter.h>
#include <pal/SessionID.h>
#include <stdlib.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/URL.h>


using namespace Inspector;

namespace WebKit {

class InspectorPlaywrightAgent::PageProxyChannel : public FrontendChannel {
    WTF_MAKE_FAST_ALLOCATED;
public:
    PageProxyChannel(FrontendChannel& frontendChannel, String browserContextID, String pageProxyID, WebPageProxy& page)
        : m_browserContextID(browserContextID)
        , m_pageProxyID(pageProxyID)
        , m_frontendChannel(frontendChannel)
        , m_page(page)
    {
    }

    ~PageProxyChannel() override = default;

    void dispatchMessageFromFrontend(const String& message)
    {
        m_page.inspectorController().dispatchMessageFromFrontend(message);
    }

    WebPageProxy& page() { return m_page; }

    void disconnect()
    {
        m_page.inspectorController().disconnectFrontend(*this);
    }

private:
    ConnectionType connectionType() const override { return m_frontendChannel.connectionType(); }
    void sendMessageToFrontend(const String& message) override
    {
        m_frontendChannel.sendMessageToFrontend(addTabIdToMessage(message));
    }

    String addTabIdToMessage(const String& message) {
        RefPtr<JSON::Value> parsedMessage = JSON::Value::parseJSON(message);
        if (!parsedMessage)
            return message;

        RefPtr<JSON::Object> messageObject = parsedMessage->asObject();
        if (!messageObject)
            return message;

        messageObject->setString("browserContextId"_s, m_browserContextID);
        messageObject->setString("pageProxyId"_s, m_pageProxyID);
        return messageObject->toJSONString();
    }

    String m_browserContextID;
    String m_pageProxyID;
    FrontendChannel& m_frontendChannel;
    WebPageProxy& m_page;
};

namespace {

String toBrowserContextIDProtocolString(const PAL::SessionID& sessionID)
{
    StringBuilder builder;
    builder.append(hex(sessionID.toUInt64(), 16));
    return builder.toString();
}

String toPageProxyIDProtocolString(const WebPageProxy& page)
{
    return makeString(page.identifier().toUInt64());
}


static Ref<JSON::ArrayOf<String>> getEnabledWindowFeatures(const WebCore::WindowFeatures& features) {
  auto result = JSON::ArrayOf<String>::create();
  if (features.x)
    result->addItem("left=" + String::number(*features.x));
  if (features.y)
    result->addItem("top=" + String::number(*features.y));
  if (features.width)
    result->addItem("width=" + String::number(*features.width));
  if (features.height)
    result->addItem("height=" + String::number(*features.height));
  if (features.menuBarVisible)
    result->addItem("menubar");
  if (features.toolBarVisible)
    result->addItem("toolbar");
  if (features.statusBarVisible)
    result->addItem("status");
  if (features.locationBarVisible)
    result->addItem("location");
  if (features.scrollbarsVisible)
    result->addItem("scrollbars");
  if (features.resizable)
    result->addItem("resizable");
  if (features.fullscreen)
    result->addItem("fullscreen");
  if (features.dialog)
    result->addItem("dialog");
  if (features.noopener)
    result->addItem("noopener");
  if (features.noreferrer)
    result->addItem("noreferrer");
  for (const auto& additionalFeature : features.additionalFeatures)
    result->addItem(additionalFeature);
  return result;
}

Inspector::Protocol::Playwright::CookieSameSitePolicy cookieSameSitePolicy(WebCore::Cookie::SameSitePolicy policy)
{
    switch (policy) {
    case WebCore::Cookie::SameSitePolicy::None:
        return Inspector::Protocol::Playwright::CookieSameSitePolicy::None;
    case WebCore::Cookie::SameSitePolicy::Lax:
        return Inspector::Protocol::Playwright::CookieSameSitePolicy::Lax;
    case WebCore::Cookie::SameSitePolicy::Strict:
        return Inspector::Protocol::Playwright::CookieSameSitePolicy::Strict;
    }
    ASSERT_NOT_REACHED();
    return Inspector::Protocol::Playwright::CookieSameSitePolicy::None;
}

Ref<Inspector::Protocol::Playwright::Cookie> buildObjectForCookie(const WebCore::Cookie& cookie)
{
    return Inspector::Protocol::Playwright::Cookie::create()
        .setName(cookie.name)
        .setValue(cookie.value)
        .setDomain(cookie.domain)
        .setPath(cookie.path)
        .setExpires(cookie.expires.value_or(-1))
        .setHttpOnly(cookie.httpOnly)
        .setSecure(cookie.secure)
        .setSession(cookie.session)
        .setSameSite(cookieSameSitePolicy(cookie.sameSite))
        .release();
}

}  // namespace

BrowserContext::BrowserContext() = default;

BrowserContext::~BrowserContext() = default;

class InspectorPlaywrightAgent::BrowserContextDeletion {
    WTF_MAKE_NONCOPYABLE(BrowserContextDeletion);
    WTF_MAKE_FAST_ALLOCATED;
public:
    BrowserContextDeletion(std::unique_ptr<BrowserContext>&& context, size_t numberOfPages, Ref<DeleteContextCallback>&& callback)
        : m_browserContext(WTFMove(context))
        , m_numberOfPages(numberOfPages)
        , m_callback(WTFMove(callback)) { }

    void didDestroyPage(const WebPageProxy& page)
    {
        ASSERT(m_browserContext->dataStore->sessionID() == page.sessionID());
        // Check if new pages have been created during the context destruction and
        // close all of them if necessary.
        if (m_numberOfPages == 1) {
            auto pages = m_browserContext->pages;
            size_t numberOfPages = pages.size();
            if (numberOfPages > 1) {
                m_numberOfPages = numberOfPages;
                for (auto* existingPage : pages) {
                    if (existingPage != &page)
                        existingPage->closePage();
                }
            }
        }
        --m_numberOfPages;
        if (m_numberOfPages)
            return;
        m_callback->sendSuccess();
    }

    bool isFinished() const { return !m_numberOfPages; }

    BrowserContext* context() const { return m_browserContext.get(); }

private:
    std::unique_ptr<BrowserContext> m_browserContext;
    size_t m_numberOfPages;
    Ref<DeleteContextCallback> m_callback;
};


InspectorPlaywrightAgent::InspectorPlaywrightAgent(std::unique_ptr<InspectorPlaywrightAgentClient> client)
    : m_frontendChannel(nullptr)
    , m_frontendRouter(FrontendRouter::create())
    , m_backendDispatcher(BackendDispatcher::create(m_frontendRouter.copyRef()))
    , m_client(std::move(client))
    , m_frontendDispatcher(makeUnique<PlaywrightFrontendDispatcher>(m_frontendRouter))
    , m_playwrightDispatcher(PlaywrightBackendDispatcher::create(m_backendDispatcher.get(), this))
{
}

InspectorPlaywrightAgent::~InspectorPlaywrightAgent()
{
    if (m_frontendChannel)
        disconnectFrontend();
}

void InspectorPlaywrightAgent::connectFrontend(FrontendChannel& frontendChannel)
{
    ASSERT(!m_frontendChannel);
    m_frontendChannel = &frontendChannel;
    WebPageInspectorController::setObserver(this);

    m_frontendRouter->connectFrontend(frontendChannel);
}

void InspectorPlaywrightAgent::disconnectFrontend()
{
    if (!m_frontendChannel)
        return;

    disable();

    m_frontendRouter->disconnectFrontend(*m_frontendChannel);
    ASSERT(!m_frontendRouter->hasFrontends());

    WebPageInspectorController::setObserver(nullptr);
    m_frontendChannel = nullptr;

    closeImpl([](String error){});
}

void InspectorPlaywrightAgent::dispatchMessageFromFrontend(const String& message)
{
    m_backendDispatcher->dispatch(message, [&](const RefPtr<JSON::Object>& messageObject) {
        RefPtr<JSON::Value> idValue;
        if (!messageObject->getValue("id"_s, idValue))
            return BackendDispatcher::InterceptionResult::Continue;
        RefPtr<JSON::Value> pageProxyIDValue;
        if (!messageObject->getValue("pageProxyId"_s, pageProxyIDValue))
            return BackendDispatcher::InterceptionResult::Continue;

        String pageProxyID;
        if (!pageProxyIDValue->asString(pageProxyID)) {
            m_backendDispatcher->reportProtocolError(BackendDispatcher::InvalidRequest, "The type of 'pageProxyId' must be string"_s);
            m_backendDispatcher->sendPendingErrors();
            return BackendDispatcher::InterceptionResult::Intercepted;
        }

        if (auto pageProxyChannel = m_pageProxyChannels.get(pageProxyID)) {
            pageProxyChannel->dispatchMessageFromFrontend(message);
            return BackendDispatcher::InterceptionResult::Intercepted;
        }

        std::optional<int> requestId = idValue->asInteger();
        if (!requestId) {
            m_backendDispatcher->reportProtocolError(BackendDispatcher::InvalidRequest, "The type of 'id' must be number"_s);
            m_backendDispatcher->sendPendingErrors();
            return BackendDispatcher::InterceptionResult::Intercepted;
        }

        m_backendDispatcher->reportProtocolError(*requestId, BackendDispatcher::InvalidParams, "Cannot find page proxy with provided 'pageProxyId'"_s);
        m_backendDispatcher->sendPendingErrors();
        return BackendDispatcher::InterceptionResult::Intercepted;
    });
}

void InspectorPlaywrightAgent::didCreateInspectorController(WebPageProxy& page)
{
    if (!m_isEnabled)
        return;

    if (isInspectorProcessPool(page.process().processPool()))
        return;

    ASSERT(m_frontendChannel);

    String browserContextID = toBrowserContextIDProtocolString(page.sessionID());
    String pageProxyID = toPageProxyIDProtocolString(page);
    auto* opener = page.configuration().relatedPage();
    String openerId;
    if (opener)
        openerId = toPageProxyIDProtocolString(*opener);

    BrowserContext* browserContext = getExistingBrowserContext(browserContextID);
    browserContext->pages.add(&page);
    m_frontendDispatcher->pageProxyCreated(browserContextID, pageProxyID, openerId);

    // Auto-connect to all new pages.
    auto pageProxyChannel = makeUnique<PageProxyChannel>(*m_frontendChannel, browserContextID, pageProxyID, page);
    page.inspectorController().connectFrontend(*pageProxyChannel);
    // Always pause new targets if controlled remotely.
    page.inspectorController().setPauseOnStart(true);
    m_pageProxyChannels.set(pageProxyID, WTFMove(pageProxyChannel));
}

void InspectorPlaywrightAgent::willDestroyInspectorController(WebPageProxy& page)
{
    if (!m_isEnabled)
        return;

    if (isInspectorProcessPool(page.process().processPool()))
        return;

    String browserContextID = toBrowserContextIDProtocolString(page.sessionID());
    BrowserContext* browserContext = getExistingBrowserContext(browserContextID);
    browserContext->pages.remove(&page);
    m_frontendDispatcher->pageProxyDestroyed(toPageProxyIDProtocolString(page));

    auto it = m_browserContextDeletions.find(browserContextID);
    if (it != m_browserContextDeletions.end()) {
        it->value->didDestroyPage(page);
        if (it->value->isFinished())
            m_browserContextDeletions.remove(it);
    }

    String pageProxyID = toPageProxyIDProtocolString(page);
    auto channelIt = m_pageProxyChannels.find(pageProxyID);
    ASSERT(channelIt != m_pageProxyChannels.end());
    channelIt->value->disconnect();
    m_pageProxyChannels.remove(channelIt);
}

void InspectorPlaywrightAgent::didFailProvisionalLoad(WebPageProxy& page, uint64_t navigationID, const String& error)
{
    if (!m_isEnabled)
        return;

    m_frontendDispatcher->provisionalLoadFailed(
        toPageProxyIDProtocolString(page),
        String::number(navigationID), error);
}

void InspectorPlaywrightAgent::willCreateNewPage(WebPageProxy& page, const WebCore::WindowFeatures& features, const URL& url)
{
    if (!m_isEnabled)
        return;

    m_frontendDispatcher->windowOpen(
        toPageProxyIDProtocolString(page),
        url.string(),
        getEnabledWindowFeatures(features));
}

void InspectorPlaywrightAgent::didFinishScreencast(const PAL::SessionID& sessionID, const String& screencastID)
{
    if (!m_isEnabled)
        return;

    m_frontendDispatcher->screencastFinished(screencastID);
}

static WebsiteDataStore* findDefaultWebsiteDataStore() {
    WebsiteDataStore* result = nullptr;
    WebsiteDataStore::forEachWebsiteDataStore([&result] (WebsiteDataStore& dataStore) {
        if (dataStore.isPersistent()) {
            RELEASE_ASSERT(result == nullptr);
            result = &dataStore;
        }
    });
    return result;
}

Inspector::Protocol::ErrorStringOr<void> InspectorPlaywrightAgent::enable()
{
    if (m_isEnabled)
        return { };

    m_isEnabled = true;

    auto* defaultDataStore = findDefaultWebsiteDataStore();
    if (!m_defaultContext && defaultDataStore) {
        auto context = std::make_unique<BrowserContext>();
        m_defaultContext = context.get();
        context->processPool = WebProcessPool::allProcessPools().first().ptr();
        context->dataStore = defaultDataStore;
        // Add default context to the map so that we can easily find it for
        // created/deleted pages.
        PAL::SessionID sessionID = context->dataStore->sessionID();
        m_browserContexts.set(toBrowserContextIDProtocolString(sessionID), WTFMove(context));
    }

    WebsiteDataStore::forEachWebsiteDataStore([this] (WebsiteDataStore& dataStore) {
        dataStore.setDownloadInstrumentation(this);
    });
    for (auto& pool : WebProcessPool::allProcessPools()) {
        for (auto& process : pool->processes()) {
            for (auto* page : process->pages())
                didCreateInspectorController(*page);
        }
    }
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPlaywrightAgent::disable()
{
    if (!m_isEnabled)
        return { };

    m_isEnabled = false;

    for (auto it = m_pageProxyChannels.begin(); it != m_pageProxyChannels.end(); ++it)
        it->value->disconnect();
    m_pageProxyChannels.clear();

    WebsiteDataStore::forEachWebsiteDataStore([] (WebsiteDataStore& dataStore) {
        dataStore.setDownloadInstrumentation(nullptr);
        dataStore.setDownloadForAutomation(std::optional<bool>(), String());
    });
    for (auto& it : m_browserContexts) {
        it.value->dataStore->setDownloadInstrumentation(nullptr);
        it.value->pages.clear();
    }
    m_browserContextDeletions.clear();
    return { };
}

void InspectorPlaywrightAgent::close(Ref<CloseCallback>&& callback)
{
    closeImpl([callback = WTFMove(callback)] (String error) {
        if (!callback->isActive())
            return;
        if (error.isNull())
            callback->sendSuccess();
        else
            callback->sendFailure(error);
    });
}

void InspectorPlaywrightAgent::closeImpl(Function<void(String)>&& callback)
{
    Vector<WebPageProxy*> pages;
    // If Web Process crashed it will be disconnected from its pool until
    // the page reloads. So we cannot discover such processes and the pages
    // by traversing all process pools and their processes. Instead we look at
    // all existing Web Processes wether in a pool or not.
    for (auto* process : WebProcessProxy::allProcessesForInspector()) {
        for (auto* page : process->pages())
            pages.append(page);
    }
    for (auto* page : pages)
        page->closePage();

    if (!m_defaultContext) {
        m_client->closeBrowser();
        callback(String());
        return;
    }

    m_defaultContext->dataStore->syncLocalStorage([this, callback = WTFMove(callback)] () {
        if (m_client == nullptr) {
            callback("no platform delegate to close browser");
        } else {
            m_client->closeBrowser();
            callback(String());
        }
    });

}

Inspector::Protocol::ErrorStringOr<String /* browserContextID */> InspectorPlaywrightAgent::createContext(const String& proxyServer, const String& proxyBypassList)
{
    String errorString;
    std::unique_ptr<BrowserContext> browserContext = m_client->createBrowserContext(errorString, proxyServer, proxyBypassList);
    if (!browserContext)
        return makeUnexpected(errorString);

    // Ensure network process.
    browserContext->dataStore->networkProcess();
    browserContext->dataStore->setDownloadInstrumentation(this);

    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    String browserContextID = toBrowserContextIDProtocolString(sessionID);
    m_browserContexts.set(browserContextID, WTFMove(browserContext));
    return browserContextID;
}

void InspectorPlaywrightAgent::deleteContext(const String& browserContextID, Ref<DeleteContextCallback>&& callback)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!lookupBrowserContext(errorString, browserContextID)) {
        callback->sendFailure(errorString);
        return;
    }

    if (browserContext == m_defaultContext) {
        callback->sendFailure("Cannot delete default context"_s);
        return;
    }

    auto pages = browserContext->pages;
    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    auto contextHolder = m_browserContexts.take(browserContextID);
    if (pages.isEmpty()) {
        callback->sendSuccess();
    } else {
        m_browserContextDeletions.set(browserContextID, makeUnique<BrowserContextDeletion>(WTFMove(contextHolder), pages.size(), WTFMove(callback)));
        for (auto* page : pages)
            page->closePage();
    }
    m_client->deleteBrowserContext(errorString, sessionID);
}

void InspectorPlaywrightAgent::getLocalStorageData(const String& browserContextID, Ref<GetLocalStorageDataCallback>&& callback)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!lookupBrowserContext(errorString, browserContextID)) {
        callback->sendFailure(errorString);
        return;
    }
    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    NetworkProcessProxy& networkProcess = browserContext->dataStore->networkProcess();
    networkProcess.sendWithAsyncReply(Messages::NetworkProcess::GetLocalStorageData(sessionID),
        [callback = WTFMove(callback)](Vector<std::pair<WebCore::SecurityOriginData, HashMap<String, String>>>&& data) {
            if (!callback->isActive())
                return;
            auto origins = JSON::ArrayOf<Inspector::Protocol::Playwright::OriginStorage>::create();
            for (const auto& originData : data) {
                auto items = JSON::ArrayOf<Inspector::Protocol::Playwright::NameValue>::create();
                for (const auto& entry : originData.second) {
                    items->addItem(Inspector::Protocol::Playwright::NameValue::create()
                        .setName(entry.key)
                        .setValue(entry.value)
                        .release());
                }
                origins->addItem(Inspector::Protocol::Playwright::OriginStorage::create()
                    .setOrigin(originData.first.toString())
                    .setItems(WTFMove(items))
                    .release());
            }
            callback->sendSuccess(WTFMove(origins));
        }, 0);
}

void InspectorPlaywrightAgent::setLocalStorageData(const String& browserContextID, Ref<JSON::Array>&& origins, Ref<SetLocalStorageDataCallback>&& callback)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!lookupBrowserContext(errorString, browserContextID)) {
        callback->sendFailure(errorString);
        return;
    }

    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    NetworkProcessProxy& networkProcess = browserContext->dataStore->networkProcess();
    if (!networkProcess.hasConnection()) {
       callback->sendFailure("No connection to the nework process");
       return;
    }

    WebKit::PageGroupIdentifier pageGroupID = browserContext->processPool->defaultPageGroup().pageGroupID();
    auto storageNamespaceID = makeObjectIdentifier<StorageNamespaceIdentifierType>(pageGroupID.toUInt64());

    Vector<std::pair<WebCore::SecurityOriginData, HashMap<String, String>>> data;
    for (const auto& value : origins.get()) {
        auto obj = value->asObject();
        if (!obj) {
            callback->sendFailure("Invalid OriginStorage format"_s);
            return;
        }

        String origin = obj->getString("origin");
        if (origin.isEmpty()) {
            callback->sendFailure("Empty origin"_s);
            return;
        }

        auto url = URL(URL(), origin);
        if (!url.isValid()) {
            callback->sendFailure("Invalid origin URL"_s);
            return;
        }

        auto items = obj->getArray("items");
        if (!items) {
            callback->sendFailure("Invalid item array format"_s);
            return;
        }

        HashMap<String, String> map;
        for (const auto& item : *items) {
            auto itemObj = item->asObject();
            if (!itemObj) {
                callback->sendFailure("Invalid item format"_s);
                return;
            }

            String name = itemObj->getString("name");
            String value = itemObj->getString("value");;
            if (name.isEmpty()) {
                callback->sendFailure("Item name cannot be empty"_s);
                return;
            }

            map.set(name, value);
        }
        data.append({ WebCore::SecurityOriginData::fromURL(url), WTFMove(map) });
    }

    networkProcess.sendWithAsyncReply(Messages::NetworkProcess::SetLocalStorageData(sessionID, storageNamespaceID, data), [callback = WTFMove(callback)] (const String& error) {
        if (error.isEmpty())
            callback->sendSuccess();
        else
            callback->sendFailure(error);
    });
}

Inspector::Protocol::ErrorStringOr<String /* pageProxyID */> InspectorPlaywrightAgent::createPage(const String& browserContextID)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!browserContext)
        return makeUnexpected(errorString);

    RefPtr<WebPageProxy> page = m_client->createPage(errorString, *browserContext);
    if (!page)
        return makeUnexpected(errorString);

    return toPageProxyIDProtocolString(*page);
}

WebFrameProxy* InspectorPlaywrightAgent::frameForID(const String& frameID, String& error)
{
    size_t dotPos = frameID.find(".");
    if (dotPos == notFound) {
        error = "Invalid frame id"_s;
        return nullptr;
    }

    if (!frameID.isAllASCII()) {
        error = "Invalid frame id"_s;
        return nullptr;
    }

    String processIDString = frameID.left(dotPos);
    uint64_t pid = strtoull(processIDString.ascii().data(), 0, 10);
    auto processID = makeObjectIdentifier<WebCore::ProcessIdentifierType>(pid);
    WebProcessProxy* process = WebProcessProxy::processForIdentifier(processID);
    if (!process) {
        error = "Cannot find web process for the frame id"_s;
        return nullptr;
    }

    String frameIDString = frameID.substring(dotPos + 1);
    uint64_t frameIDNumber = strtoull(frameIDString.ascii().data(), 0, 10);
    auto frameIdentifier = makeObjectIdentifier<WebCore::FrameIdentifierType>(frameIDNumber);
    WebFrameProxy* frame = process->webFrame(frameIdentifier);
    if (!frame) {
        error = "Cannot find web frame for the frame id"_s;
        return nullptr;
    }

    return frame;
}

void InspectorPlaywrightAgent::navigate(const String& url, const String& pageProxyID, const String& frameID, const String& referrer, Ref<NavigateCallback>&& callback)
{
    auto* pageProxyChannel = m_pageProxyChannels.get(pageProxyID);
    if (!pageProxyChannel) {
        callback->sendFailure("Cannot find page proxy with provided 'pageProxyId'"_s);
        return;
    }

    WebCore::ResourceRequest resourceRequest { url };

    if (!!referrer)
        resourceRequest.setHTTPReferrer(referrer);

    if (!resourceRequest.url().isValid()) {
        callback->sendFailure("Cannot navigate to invalid URL"_s);
        return;
    }

    WebFrameProxy* frame = nullptr;
    if (!!frameID) {
        String error;
        frame = frameForID(frameID, error);
        if (!frame) {
            callback->sendFailure(error);
            return;
        }

        if (frame->page() != &pageProxyChannel->page()) {
            callback->sendFailure("Frame with specified is not from the specified page"_s);
            return;
        }
    }

    pageProxyChannel->page().inspectorController().navigate(WTFMove(resourceRequest), frame, [callback = WTFMove(callback)](const String& error, uint64_t navigationID) {
        if (!error.isEmpty()) {
            callback->sendFailure(error);
            return;
        }

        String navigationIDString;
        if (navigationID)
            navigationIDString = String::number(navigationID);
        callback->sendSuccess(navigationIDString);
    });
}

Inspector::Protocol::ErrorStringOr<void> InspectorPlaywrightAgent::setIgnoreCertificateErrors(const String& browserContextID, bool ignore)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!errorString.isEmpty())
        return makeUnexpected(errorString);

    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    NetworkProcessProxy& networkProcess = browserContext->dataStore->networkProcess();
    networkProcess.send(Messages::NetworkProcess::SetIgnoreCertificateErrors(sessionID, ignore), 0);
    return { };
}

void InspectorPlaywrightAgent::getAllCookies(const String& browserContextID, Ref<GetAllCookiesCallback>&& callback) {
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!errorString.isEmpty()) {
        callback->sendFailure(errorString);
        return;
    }

    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    NetworkProcessProxy& networkProcess = browserContext->dataStore->networkProcess();
    networkProcess.sendWithAsyncReply(Messages::NetworkProcess::GetAllCookies(sessionID),
        [callback = WTFMove(callback)](Vector<WebCore::Cookie> allCookies) {
            if (!callback->isActive())
                return;
            auto cookies = JSON::ArrayOf<Inspector::Protocol::Playwright::Cookie>::create();
            for (const auto& cookie : allCookies)
                cookies->addItem(buildObjectForCookie(cookie));
            callback->sendSuccess(WTFMove(cookies));
        }, 0);
}

void InspectorPlaywrightAgent::setCookies(const String& browserContextID, Ref<JSON::Array>&& in_cookies, Ref<SetCookiesCallback>&& callback) {
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!errorString.isEmpty()) {
        callback->sendFailure(errorString);
        return;
    }

    NetworkProcessProxy& networkProcess = browserContext->dataStore->networkProcess();
    PAL::SessionID sessionID = browserContext->dataStore->sessionID();

    Vector<WebCore::Cookie> cookies;
    for (unsigned i = 0; i < in_cookies->length(); ++i) {
        RefPtr<JSON::Value> item = in_cookies->get(i);
        RefPtr<JSON::Object> obj = item->asObject();
        if (!obj) {
            callback->sendFailure("Invalid cookie payload format"_s);
            return;
        }

        WebCore::Cookie cookie;
        cookie.name = obj->getString("name");
        cookie.value = obj->getString("value");
        cookie.domain = obj->getString("domain");
        cookie.path = obj->getString("path");
        if (!cookie.name || !cookie.value || !cookie.domain || !cookie.path) {
            callback->sendFailure("Invalid file payload format"_s);
            return;
        }

        std::optional<double> expires = obj->getDouble("expires");
        if (expires && *expires != -1)
            cookie.expires = *expires;
        if (std::optional<bool> value = obj->getBoolean("httpOnly"))
            cookie.httpOnly = *value;
        if (std::optional<bool> value = obj->getBoolean("secure"))
            cookie.secure = *value;
        if (std::optional<bool> value = obj->getBoolean("session"))
            cookie.session = *value;
        String sameSite;
        if (obj->getString("sameSite", sameSite)) {
            if (sameSite == "None")
                cookie.sameSite = WebCore::Cookie::SameSitePolicy::None;
            if (sameSite == "Lax")
                cookie.sameSite = WebCore::Cookie::SameSitePolicy::Lax;
            if (sameSite == "Strict")
                cookie.sameSite = WebCore::Cookie::SameSitePolicy::Strict;
        }
        cookies.append(WTFMove(cookie));
    }

    networkProcess.sendWithAsyncReply(Messages::NetworkProcess::SetCookies(sessionID, WTFMove(cookies)),
        [callback = WTFMove(callback)](bool success) {
            if (!callback->isActive())
                return;

            if (success)
                callback->sendSuccess();
            else
                callback->sendFailure("Internal error: no network storage"_s);
            callback->sendSuccess();
        }, 0);
}

void InspectorPlaywrightAgent::deleteAllCookies(const String& browserContextID, Ref<DeleteAllCookiesCallback>&& callback) {
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!errorString.isEmpty()) {
        callback->sendFailure(errorString);
        return;
    }

    NetworkProcessProxy& networkProcess = browserContext->dataStore->networkProcess();
    PAL::SessionID sessionID = browserContext->dataStore->sessionID();
    networkProcess.sendWithAsyncReply(Messages::NetworkProcess::DeleteAllCookies(sessionID),
        [callback = WTFMove(callback)](bool success) {
            if (!callback->isActive())
                return;
            if (success)
                callback->sendSuccess();
            else
                callback->sendFailure("Internal error: no network storage"_s);
        }, 0);
}

Inspector::Protocol::ErrorStringOr<void> InspectorPlaywrightAgent::setLanguages(Ref<JSON::Array>&& languages, const String& browserContextID)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!errorString.isEmpty())
        return makeUnexpected(errorString);

    Vector<String> items;
    for (const auto& value : languages.get()) {
        String language;
        if (!value->asString(language))
            return makeUnexpected("Language must be a string"_s);

        items.append(language);
    }

    browserContext->dataStore->setLanguagesForAutomation(WTFMove(items));
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPlaywrightAgent::setDownloadBehavior(const String& behavior, const String& downloadPath, const String& browserContextID)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!errorString.isEmpty())
        return makeUnexpected(errorString);

    std::optional<bool> allow;
    if (behavior == "allow"_s)
      allow = true;
    if (behavior == "deny"_s)
      allow = false;
    browserContext->dataStore->setDownloadForAutomation(allow, downloadPath);
    return { };
}

Inspector::Protocol::ErrorStringOr<void> InspectorPlaywrightAgent::setGeolocationOverride(const String& browserContextID, RefPtr<JSON::Object>&& geolocation)
{
    String errorString;
    BrowserContext* browserContext = lookupBrowserContext(errorString, browserContextID);
    if (!errorString.isEmpty())
        return makeUnexpected(errorString);

    auto* geoManager = browserContext->processPool->supplement<WebGeolocationManagerProxy>();
    if (!geoManager)
        return makeUnexpected("Internal error: geolocation manager is not available."_s);

    if (geolocation) {
        std::optional<double> timestamp = geolocation->getDouble("timestamp");
        std::optional<double> latitude = geolocation->getDouble("latitude");
        std::optional<double> longitude = geolocation->getDouble("longitude");
        std::optional<double> accuracy = geolocation->getDouble("accuracy");
        if (!timestamp || !latitude || !longitude || !accuracy)
            return makeUnexpected("Invalid geolocation format"_s);

        auto position = WebGeolocationPosition::create(WebCore::GeolocationPositionData(*timestamp, *latitude, *longitude, *accuracy));
        geoManager->providerDidChangePosition(&position.get());
    } else {
        geoManager->providerDidFailToDeterminePosition("Position unavailable"_s);
    }
    return { };
}

void InspectorPlaywrightAgent::downloadCreated(const String& uuid, const WebCore::ResourceRequest& request, const FrameInfoData& frameInfoData, WebPageProxy* page, RefPtr<DownloadProxy> download)
{
    if (!m_isEnabled)
        return;
    String frameID = WebCore::InspectorPageAgent::makeFrameID(page->process().coreProcessIdentifier(), frameInfoData.frameID ? *frameInfoData.frameID : page->mainFrame()->frameID());
    m_downloads.set(uuid, download);
    m_frontendDispatcher->downloadCreated(
        toPageProxyIDProtocolString(*page),
        frameID,
        uuid, request.url().string());
}

void InspectorPlaywrightAgent::downloadFilenameSuggested(const String& uuid, const String& suggestedFilename)
{
    if (!m_isEnabled)
        return;
    m_frontendDispatcher->downloadFilenameSuggested(uuid, suggestedFilename);
}

void InspectorPlaywrightAgent::downloadFinished(const String& uuid, const String& error)
{
    if (!m_isEnabled)
        return;
    m_frontendDispatcher->downloadFinished(uuid, error);
    m_downloads.remove(uuid);
}

Inspector::Protocol::ErrorStringOr<void> InspectorPlaywrightAgent::cancelDownload(const String& uuid)
{
    if (!m_isEnabled)
        return { };
    auto download = m_downloads.get(uuid);
    if (!download)
        return { };
    download->cancel([] (auto*) {});
    return { };
}

BrowserContext* InspectorPlaywrightAgent::getExistingBrowserContext(const String& browserContextID)
{
    BrowserContext* browserContext = m_browserContexts.get(browserContextID);
    if (browserContext)
        return browserContext;

    auto it = m_browserContextDeletions.find(browserContextID);
    RELEASE_ASSERT(it != m_browserContextDeletions.end());
    return it->value->context();
}

BrowserContext* InspectorPlaywrightAgent::lookupBrowserContext(ErrorString& errorString, const String& browserContextID)
{
    if (!browserContextID) {
        if (!m_defaultContext)
            errorString = "Browser started with no default context"_s;
        return m_defaultContext;
    }

    BrowserContext* browserContext = m_browserContexts.get(browserContextID);
    if (!browserContext)
        errorString = "Could not find browser context for given id"_s;
    return browserContext;
}

} // namespace WebKit

#endif // ENABLE(REMOTE_INSPECTOR)
