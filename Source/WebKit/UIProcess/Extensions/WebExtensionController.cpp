/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "APIError.h"
#include "Logging.h"
#include "WebExtensionContextMessages.h"
#include "WebExtensionControllerMessages.h"
#include "WebExtensionControllerParameters.h"
#include "WebExtensionControllerProxyMessages.h"
#include "WebExtensionMatchPattern.h"
#include "WebPageProxy.h"
#include "WebProcessPool.h"
#include "WebsiteDataStore.h"
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>

#if PLATFORM(COCOA)
#include <wtf/BlockPtr.h>
#include <wtf/darwin/DispatchExtras.h>
#endif

namespace WebKit {

#if PLATFORM(COCOA)
constexpr auto freshlyCreatedTimeout = 5_s;
#endif

static void addContextMessageReceivers(WebProcessPool& processPool, WebExtensionContext& context)
{
#if PLATFORM(COCOA)
    processPool.addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context.identifier(), context);
    processPool.addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context.privilegedIdentifier(), context);
#else
    UNUSED_PARAM(processPool);
    UNUSED_PARAM(context);
#endif
}

static void removeContextMessageReceivers(WebProcessPool& processPool, WebExtensionContext& context)
{
#if PLATFORM(COCOA)
    processPool.removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context.identifier());
    processPool.removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context.privilegedIdentifier());
#else
    UNUSED_PARAM(processPool);
    UNUSED_PARAM(context);
#endif
}

static HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionController>>& NODELETE webExtensionControllers()
{
    static MainRunLoopNeverDestroyed<HashMap<WebExtensionControllerIdentifier, WeakPtr<WebExtensionController>>> controllers;
    return controllers;
}

RefPtr<WebExtensionController> WebExtensionController::get(WebExtensionControllerIdentifier identifier)
{
    return webExtensionControllers().get(identifier);
}

WebExtensionController::WebExtensionController(Ref<WebExtensionControllerConfiguration> configuration)
    : m_configuration(configuration)
{
    ASSERT(!get(identifier()));
    webExtensionControllers().add(identifier(), *this);

    initializePlatform();

    // A freshly created extension controller will be used to determine if the startup event
    // should be fired for any loaded extensions during a brief time window. Start a timer
    // when the first extension is about to be loaded.

#if PLATFORM(COCOA)
    dispatch_after(dispatch_time(DISPATCH_TIME_NOW, (int64_t)(freshlyCreatedTimeout.seconds() * NSEC_PER_SEC)), mainDispatchQueueSingleton(), makeBlockPtr([this, weakThis = WeakPtr { *this }] {
        if (!weakThis)
            return;

        m_freshlyCreated = false;
    }).get());
#endif
}

WebExtensionController::~WebExtensionController()
{
    webExtensionControllers().remove(identifier());

    for (Ref pool : m_processPools)
        pool->removeMessageReceiver(Messages::WebExtensionController::messageReceiverName(), identifier());

    unloadAll();
}

WebExtensionControllerParameters WebExtensionController::parameters(const API::PageConfiguration& pageConfiguration, WebProcessProxy& destinationProcess) const
{
    return {
        .identifier = identifier(),
        .testingMode = inTestingMode(),
        .contextParameters = WTF::map(extensionContexts(), [&](auto& context) {
            bool isForThisExtension = context->isURLForThisExtension(pageConfiguration.requiredWebExtensionBaseURL());
            auto includePrivilegedIdentifier = isForThisExtension ? WebExtensionContext::IncludePrivilegedIdentifier::Yes : WebExtensionContext::IncludePrivilegedIdentifier::No;
            return context->parameters(includePrivilegedIdentifier, destinationProcess);
        })
    };
}

WebExtensionController::WebProcessProxySet WebExtensionController::allProcesses() const
{
    WebProcessProxySet result;

    for (Ref page : m_pages) {
        page->forEachWebContentProcess([&](auto& webProcess, auto pageID) {
            result.addVoid(webProcess);
        });
    }

    return result;
}

Expected<bool, RefPtr<API::Error>> WebExtensionController::load(WebExtensionContext& extensionContext)
{
    if (!m_extensionContexts.add(extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded");
        return makeUnexpected(extensionContext.createError(WebExtensionContext::Error::AlreadyLoaded));
    }

    if (!m_extensionContextBaseURLMap.add(extensionContext.baseURL().protocolHostAndPort(), extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded with same base URL: %" PRIVATE_LOG_STRING, extensionContext.baseURL().string().utf8().data());
        m_extensionContexts.remove(extensionContext);
        return makeUnexpected(extensionContext.createError(WebExtensionContext::Error::BaseURLAlreadyInUse));
    }

    for (Ref processPool : m_processPools)
        addContextMessageReceivers(processPool, extensionContext);

    auto scheme = extensionContext.baseURL().protocol().toString();
    m_registeredSchemeHandlers.ensure(scheme, [&]() {
        Ref handler = WebExtensionURLSchemeHandler::create(*this);

        for (Ref page : m_pages)
            page->setURLSchemeHandlerForScheme(handler.copyRef(), scheme);

        return handler;
    });

    auto extensionDirectory = storageDirectory(extensionContext);
    if (!!extensionDirectory && !FileSystem::makeAllDirectories(extensionDirectory))
        RELEASE_LOG_ERROR(Extensions, "Failed to create directory: %" PRIVATE_LOG_STRING, extensionDirectory.utf8().data());

    auto loadResult = extensionContext.load(*this, extensionDirectory);
    if (!loadResult) {
        m_extensionContexts.remove(extensionContext);
        m_extensionContextBaseURLMap.remove(extensionContext.baseURL().protocolHostAndPort());

        for (Ref processPool : m_processPools)
            removeContextMessageReceivers(processPool, extensionContext);

        return makeUnexpected(loadResult.error());
    }

    return true;
}

Expected<bool, RefPtr<API::Error>> WebExtensionController::unload(WebExtensionContext& extensionContext)
{
    Ref protectedExtensionContext = extensionContext;

    if (!m_extensionContexts.remove(extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        return makeUnexpected(extensionContext.createError(WebExtensionContext::Error::NotLoaded));
    }

    bool result = m_extensionContextBaseURLMap.remove(extensionContext.baseURL().protocolHostAndPort());
    UNUSED_VARIABLE(result);
    ASSERT(result);

    sendToAllProcesses(Messages::WebExtensionControllerProxy::Unload(extensionContext.identifier()), identifier());

    for (Ref processPool : m_processPools)
        removeContextMessageReceivers(processPool, extensionContext);

    auto unloadResult = extensionContext.unload();
    if (!unloadResult)
        return makeUnexpected(unloadResult.error());

    return true;
}

void WebExtensionController::unloadAll()
{
    auto contextsCopy = m_extensionContexts;
    for (Ref context : contextsCopy)
        std::ignore = unload(context);
}

void WebExtensionController::dispatchDidLoad(WebExtensionContext& context)
{
    for (Ref process : allProcesses()) {
        if (!process->canSendMessage())
            continue;

        process->send(Messages::WebExtensionControllerProxy::Load(context.parameters(WebExtensionContext::IncludePrivilegedIdentifier::No, process)), identifier());
    }
}

void WebExtensionController::addPage(WebPageProxy& page)
{
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);

    for (auto& entry : m_registeredSchemeHandlers)
        page.setURLSchemeHandlerForScheme(entry.value.copyRef(), entry.key);

    Ref pool = page.configuration().processPool();
    addProcessPool(pool);

    Ref dataStore = page.websiteDataStore();
    addWebsiteDataStore(dataStore);

    Ref controller = page.userContentController();
    bool isOwnStore = !dataStore->isPersistent() && (&dataStore.get() == &m_configuration->defaultWebsiteDataStore());
    addUserContentController(controller, (dataStore->isPersistent() || isOwnStore) ? ForPrivateBrowsing::No : ForPrivateBrowsing::Yes);
}

void WebExtensionController::removePage(WebPageProxy& page)
{
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);

    Ref pool = page.configuration().processPool();
    removeProcessPool(pool);

    Ref dataStore = page.websiteDataStore();
    removeWebsiteDataStore(dataStore);

    Ref controller = page.userContentController();
    removeUserContentController(controller);

    for (Ref context : m_extensionContexts)
        context->removePage(page);
}

void WebExtensionController::addProcessPool(WebProcessPool& processPool)
{
    if (!m_processPools.add(processPool))
        return;

    for (auto& urlScheme : WebExtensionMatchPattern::extensionSchemes()) {
        processPool.registerURLSchemeAsSecure(urlScheme);
        processPool.registerURLSchemeAsBypassingContentSecurityPolicy(urlScheme);
        processPool.setDomainRelaxationForbiddenForURLScheme(urlScheme);
    }

    processPool.addMessageReceiver(Messages::WebExtensionController::messageReceiverName(), identifier(), *this);

    for (Ref context : m_extensionContexts)
        addContextMessageReceivers(processPool, context);
}

void WebExtensionController::removeProcessPool(WebProcessPool& processPool)
{
    // Only remove the message receiver and process pool if no other pages use the same process pool.
    for (Ref knownPage : m_pages) {
        if (knownPage->configuration().processPool() == processPool)
            return;
    }

    processPool.removeMessageReceiver(Messages::WebExtensionController::messageReceiverName(), identifier());

    for (Ref context : m_extensionContexts)
        removeContextMessageReceivers(processPool, context);

    m_processPools.remove(processPool);
}

void WebExtensionController::addUserContentController(WebUserContentControllerProxy& userContentController, ForPrivateBrowsing forPrivateBrowsing)
{
    if (forPrivateBrowsing == ForPrivateBrowsing::No)
        m_allNonPrivateUserContentControllers.add(userContentController);
    else
        m_allPrivateUserContentControllers.add(userContentController);

    if (!m_allUserContentControllers.add(userContentController))
        return;

    for (Ref context : m_extensionContexts) {
        if (!context->hasAccessToPrivateData() && forPrivateBrowsing == ForPrivateBrowsing::Yes)
            continue;

        context->addInjectedContent(userContentController);
        context->addDeclarativeNetRequestRules(userContentController);
    }
}

void WebExtensionController::removeUserContentController(WebUserContentControllerProxy& userContentController)
{
    // Only remove the user content controller if no other pages use the same one.
    for (auto& knownPage : m_pages) {
        if (knownPage.userContentController() == userContentController)
            return;
    }

    for (Ref context : m_extensionContexts) {
        context->removeInjectedContent(userContentController);
        userContentController.removeContentRuleList(context->uniqueIdentifier());
    }

    m_allNonPrivateUserContentControllers.remove(userContentController);
    m_allPrivateUserContentControllers.remove(userContentController);
    m_allUserContentControllers.remove(userContentController);
}

RefPtr<WebsiteDataStore> WebExtensionController::websiteDataStore(std::optional<PAL::SessionID> sessionID) const
{
    Ref configuration = m_configuration;
    if (!sessionID || configuration->defaultWebsiteDataStore().sessionID() == sessionID.value())
        return configuration->defaultWebsiteDataStore();

    for (auto& dataStore : allWebsiteDataStores()) {
        if (dataStore.sessionID() == sessionID.value())
            return &dataStore;
    }

    return nullptr;
}

void WebExtensionController::addWebsiteDataStore(WebsiteDataStore& dataStore)
{
    m_websiteDataStores.add(dataStore);

    if (!m_cookieStoreObserver)
        m_cookieStoreObserver = HTTPCookieStoreObserver::create(*this);

    protect(dataStore.cookieStore())->registerObserver(*protect(m_cookieStoreObserver));
}

void WebExtensionController::removeWebsiteDataStore(WebsiteDataStore& dataStore)
{
    // Only remove the data store if no other pages use the same one.
    for (auto& knownPage : m_pages) {
        if (knownPage.websiteDataStore() == dataStore)
            return;
    }

    m_websiteDataStores.remove(dataStore);

    if (RefPtr observer = m_cookieStoreObserver)
        protect(dataStore.cookieStore())->unregisterObserver(*observer);

    if (m_websiteDataStores.isEmptyIgnoringNullReferences())
        m_cookieStoreObserver = nullptr;
}

// MARK: webNavigation

void WebExtensionController::didStartProvisionalLoadForFrame(WebPageProxyIdentifier pageID, const WebExtensionFrameParameters& frameParameters, WallTime timestamp)
{
    for (Ref context : m_extensionContexts)
        context->didStartProvisionalLoadForFrame(pageID, frameParameters, timestamp);
}

void WebExtensionController::didCommitLoadForFrame(WebPageProxyIdentifier pageID, const WebExtensionFrameParameters& frameParameters, WallTime timestamp)
{
    for (Ref context : m_extensionContexts)
        context->didCommitLoadForFrame(pageID, frameParameters, timestamp);
}

void WebExtensionController::didFinishLoadForFrame(WebPageProxyIdentifier pageID, const WebExtensionFrameParameters& frameParameters, WallTime timestamp)
{
    for (Ref context : m_extensionContexts)
        context->didFinishLoadForFrame(pageID, frameParameters, timestamp);
}

void WebExtensionController::didFailLoadForFrame(WebPageProxyIdentifier pageID, const WebExtensionFrameParameters& frameParameters, WallTime timestamp)
{
    for (Ref context : m_extensionContexts)
        context->didFailLoadForFrame(pageID, frameParameters, timestamp);
}

void WebExtensionController::cookiesDidChange(API::HTTPCookieStore& cookieStore)
{
    // FIXME: <https://webkit.org/b/267514> Add support for changeInfo.

    for (Ref context : m_extensionContexts)
        context->cookiesDidChange(cookieStore);
}

RefPtr<WebExtensionContext> WebExtensionController::extensionContext(const WebExtension& extension) const
{
    for (auto& context : m_extensionContexts) {
        if (context->extension() == extension)
            return context.ptr();
    }

    return nullptr;
}

RefPtr<WebExtensionContext> WebExtensionController::extensionContext(const UniqueIdentifier& uniqueIdentifier) const
{
    for (auto& context : m_extensionContexts) {
        if (context->uniqueIdentifier() == uniqueIdentifier)
            return context.ptr();
    }

    return nullptr;
}

RefPtr<WebExtensionContext> WebExtensionController::extensionContext(const URL& url) const
{
    return m_extensionContextBaseURLMap.get(url.protocolHostAndPort());
}

WebExtensionController::WebExtensionSet WebExtensionController::extensions() const
{
    WebExtensionSet extensions;
    extensions.reserveInitialCapacity(m_extensionContexts.size());
    for (Ref context : m_extensionContexts)
        extensions.addVoid(context->extension());
    return extensions;
}

String WebExtensionController::storageDirectory(WebExtensionContext& extensionContext) const
{
    if (m_configuration->storageIsPersistent() && extensionContext.hasCustomUniqueIdentifier())
        return FileSystem::pathByAppendingComponent(m_configuration->storageDirectory(), extensionContext.uniqueIdentifier());
    return nullString();
}

String WebExtensionController::storageDirectory(const String& uniqueIdentifier) const
{
    return FileSystem::pathByAppendingComponent(m_configuration->storageDirectory(), uniqueIdentifier);
}

String WebExtensionController::stateFilePath(const String& uniqueIdentifier) const
{
    return FileSystem::pathByAppendingComponent(storageDirectory(uniqueIdentifier), WebExtensionContext::plistFileName());
}

HashSet<String> WebExtensionController::activeExtensionURLs() const
{
    HashSet<String> origins;

    for (auto& context : m_extensionContexts)
        origins.add(context->baseURL().protocolHostAndPort().convertToASCIILowercase());

    if (m_configuration->storageIsPersistent()) {
        for (auto& uniqueIdentifier : FileSystem::listDirectory(m_configuration->storageDirectory())) {
            if (uniqueIdentifier == "StaleExtensionOriginsCleared"_s)
                continue;
            URL lastSeenBaseURL;
            if (WebExtensionContext::readLastBaseURLFromState(stateFilePath(uniqueIdentifier), lastSeenBaseURL))
                origins.add(lastSeenBaseURL.protocolHostAndPort().convertToASCIILowercase());
        }
    }

    return origins;
}

RefPtr<WebExtensionStorageSQLiteStore> WebExtensionController::sqliteStore(const String& storageDirectory, WebExtensionDataType type, RefPtr<WebExtensionContext> extensionContext)
{
    if (type == WebExtensionDataType::Session) {
        if (extensionContext)
            return extensionContext->storageForType(WebExtensionDataType::Session);
        return nullptr;
    }

    auto uniqueIdentifier = FileSystem::lastComponentOfPathIgnoringTrailingSlash(storageDirectory);
    return WebExtensionStorageSQLiteStore::create(uniqueIdentifier, type, storageDirectory, WebExtensionStorageSQLiteStore::UsesInMemoryDatabase::No);
}

void WebExtensionController::calculateStorageSize(WebExtensionStorageSQLiteStore& storage, WebExtensionDataType type, CompletionHandler<void(Expected<size_t, WebExtensionError>&&)>&& completionHandler)
{
    storage.getStorageSizeForKeys({ }, [completionHandler = WTF::move(completionHandler)](size_t storageSize, const String& errorMessage) mutable {
        // FIXME: <https://webkit.org/b/269100> Add storage size of window.localStorage, window.sessionStorage and indexedDB.
        if (!errorMessage.isEmpty())
            completionHandler(makeUnexpected(errorMessage));
        else
            completionHandler(storageSize);
    });
}

void WebExtensionController::removeStorage(WebExtensionStorageSQLiteStore& storage, WebExtensionDataType type, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    storage.deleteDatabase([completionHandler = WTF::move(completionHandler)](const String& errorMessage) mutable {
        // FIXME: <https://webkit.org/b/269100> Remove window.localStorage, window.sessionStorage, indexedDB.
        if (!errorMessage.isEmpty())
            completionHandler(makeUnexpected(errorMessage));
        else
            completionHandler({ });
    });
}

bool WebExtensionController::markDidRemoveStaleExtensionWebsiteData()
{
    if (m_didRemoveStaleExtensionWebsiteData)
        return false;

    m_didRemoveStaleExtensionWebsiteData = true;
    return true;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
