/*
 * Copyright (C) 2022-2024 Apple Inc. All rights reserved.
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

#if !__has_feature(objc_arc)
#error This file requires ARC. Add the "-fobjc-arc" compiler flag for this file.
#endif

#import "config.h"
#import "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#import "APIWebsitePolicies.h"
#import "CocoaHelpers.h"
#import "ContextMenuContextData.h"
#import "Logging.h"
#import "SandboxUtilities.h"
#import "WKFeature.h"
#import "WKPreferences.h"
#import "WKPreferencesPrivate.h"
#import "WKWebViewConfigurationPrivate.h"
#import "WKWebsiteDataStoreInternal.h"
#import "WebExtensionContext.h"
#import "WebExtensionContextMessages.h"
#import "WebExtensionContextParameters.h"
#import "WebExtensionContextProxyMessages.h"
#import "WebExtensionControllerMessages.h"
#import "WebExtensionControllerProxyMessages.h"
#import "WebExtensionDataRecord.h"
#import "WebExtensionEventListenerType.h"
#import "WebExtensionStorageSQLiteStore.h"
#import "WebPageProxy.h"
#import "WebProcessPool.h"
#import "_WKFeatureInternal.h"
#import <WebCore/ContentRuleListResults.h>
#import <wtf/BlockPtr.h>
#import <wtf/CallbackAggregator.h>
#import <wtf/FileSystem.h>
#import <wtf/HashMap.h>
#import <wtf/HashSet.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/text/WTFString.h>

#if PLATFORM(IOS_FAMILY)
#import "FoundationSPI.h"
#endif

static constexpr Seconds purgeMatchedRulesInterval = 5_min;

static NSString * const WebExtensionUniqueIdentifierKey = @"uniqueIdentifier";
static NSString * const WebExtensionLocalStorageWasDeletedNotification = @"WebExtensionLocalStorageWasDeleted";

using namespace WebKit;

@interface _WKWebExtensionControllerHelper : NSObject {
    WeakPtr<WebKit::WebExtensionController> _webExtensionController;
}

- (instancetype)initWithWebExtensionController:(WebKit::WebExtensionController&)controller;

@end

@implementation _WKWebExtensionControllerHelper

- (instancetype)initWithWebExtensionController:(WebKit::WebExtensionController&)controller
{
    if (!(self = [super init]))
        return nil;

    _webExtensionController = controller;

    [NSDistributedNotificationCenter.defaultCenter addObserver:self selector:@selector(_didDeleteLocalStorage:) name:WebExtensionLocalStorageWasDeletedNotification object:nil];

    return self;
}

- (void)_didDeleteLocalStorage:(NSNotification *)notification
{
    NSString *uniqueIdentifier = objectForKey<NSString>(notification.userInfo, WebExtensionUniqueIdentifierKey);
    if (!uniqueIdentifier)
        return;

    RefPtr webExtensionController = _webExtensionController.get();
    if (!webExtensionController)
        return;

    if (RefPtr context = webExtensionController->extensionContext(uniqueIdentifier))
        context->invalidateStorage();
}

@end

namespace WebKit {

void WebExtensionController::initializePlatform()
{
    ASSERT(!m_webExtensionControllerHelper);
    m_webExtensionControllerHelper = [[_WKWebExtensionControllerHelper alloc] initWithWebExtensionController:*this];
}

String WebExtensionController::storageDirectory(WebExtensionContext& extensionContext) const
{
    if (m_configuration->storageIsPersistent() && extensionContext.hasCustomUniqueIdentifier())
        return FileSystem::pathByAppendingComponent(m_configuration->storageDirectory(), extensionContext.uniqueIdentifier());
    return nullString();
}

void WebExtensionController::getDataRecords(OptionSet<WebExtensionDataType> dataTypes, CompletionHandler<void(Vector<Ref<WebExtensionDataRecord>>)>&& completionHandler)
{
    if (!m_configuration->storageIsPersistent() || dataTypes.isEmpty()) {
        completionHandler({ });
        return;
    }

    Ref recordHolder = WebExtensionDataRecordHolder::create();
    Ref aggregator = MainRunLoopCallbackAggregator::create([recordHolder, completionHandler = WTFMove(completionHandler)]() mutable {
        Vector<Ref<WebExtensionDataRecord>> records;
        for (auto& entry : recordHolder->recordsMap)
            records.append(entry.value);

        completionHandler(records);
    });

    auto uniqueIdentifiers = FileSystem::listDirectory(m_configuration->storageDirectory());
    for (auto& uniqueIdentifier : uniqueIdentifiers) {
        String displayName;
        if (!WebExtensionContext::readDisplayNameFromState(stateFilePath(uniqueIdentifier), displayName)) {
            RELEASE_LOG_ERROR(Extensions, "Failed to read extension display name from State.plist for extension: %{private}@", uniqueIdentifier.createNSString().get());
            continue;
        }

        for (auto dataType : dataTypes) {
            Ref record = recordHolder->recordsMap.ensure(uniqueIdentifier, [&] {
                return WebExtensionDataRecord::create(displayName, uniqueIdentifier);
            }).iterator->value;

            RefPtr storage = sqliteStore(storageDirectory(uniqueIdentifier), dataType, this->extensionContext(uniqueIdentifier));
            if (!storage) {
                RELEASE_LOG_ERROR(Extensions, "Failed to create sqlite store for extension: %{private}@", uniqueIdentifier.createNSString().get());
                record->addError(@"Unable to calculate extension storage", dataType);
                continue;
            }

            calculateStorageSize(storage, dataType, makeBlockPtr([recordHolder, aggregator, uniqueIdentifier, displayName, dataType, record = Ref { record }](Expected<size_t, WebExtensionError>&& result) mutable {
                if (!result)
                    record->addError(result.error().createNSString().get(), dataType);
                else
                    record->setSizeOfType(dataType, result.value());
            }));
        }
    }
}

void WebExtensionController::getDataRecord(OptionSet<WebExtensionDataType> dataTypes, WebExtensionContext& extensionContext, CompletionHandler<void(RefPtr<WebExtensionDataRecord>)>&& completionHandler)
{
    if (!m_configuration->storageIsPersistent() || dataTypes.isEmpty()) {
        completionHandler(nullptr);
        return;
    }

    String matchingUniqueIdentifier;
    String displayName;

    auto uniqueIdentifiers = FileSystem::listDirectory(m_configuration->storageDirectory());
    for (auto& uniqueIdentifier : uniqueIdentifiers) {
        if (uniqueIdentifier == extensionContext.uniqueIdentifier() && WebExtensionContext::readDisplayNameFromState(stateFilePath(uniqueIdentifier), displayName)) {
            matchingUniqueIdentifier = uniqueIdentifier;
            break;
        }
    }

    if (!matchingUniqueIdentifier) {
        completionHandler(nullptr);
        return;
    }

    Ref recordHolder = WebExtensionDataRecordHolder::create();
    Ref aggregator = MainRunLoopCallbackAggregator::create([recordHolder, completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(recordHolder->recordsMap.takeFirst());
    });

    for (auto dataType : dataTypes) {
        Ref record = recordHolder->recordsMap.ensure(matchingUniqueIdentifier, [&] {
            return WebExtensionDataRecord::create(displayName, matchingUniqueIdentifier);
        }).iterator->value;

        RefPtr storage = sqliteStore(storageDirectory(matchingUniqueIdentifier), dataType, this->extensionContext(matchingUniqueIdentifier));
        if (!storage) {
            RELEASE_LOG_ERROR(Extensions, "Failed to create sqlite store for extension: %{private}@", matchingUniqueIdentifier.createNSString().get());
            record->addError(@"Unable to calculcate extension storage", dataType);
            continue;
        }

        calculateStorageSize(storage, dataType, makeBlockPtr([recordHolder, aggregator, matchingUniqueIdentifier, displayName, dataType, record = Ref { record }](Expected<size_t, WebExtensionError>&& result) mutable {
            if (!result)
                record->addError(result.error().createNSString().get(), dataType);
            else
                record->setSizeOfType(dataType, result.value());
        }));
    }
}

void WebExtensionController::removeData(OptionSet<WebExtensionDataType> dataTypes, const Vector<Ref<WebExtensionDataRecord>>& records, CompletionHandler<void()>&& completionHandler)
{
    if (!m_configuration->storageIsPersistent() || dataTypes.isEmpty() || records.isEmpty()) {
        completionHandler();
        return;
    }

    Ref aggregator = MainRunLoopCallbackAggregator::create([completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler();
    });

    for (Ref record : records) {
        auto uniqueIdentifier = record.get().uniqueIdentifier();
        for (auto dataType : dataTypes) {
            RefPtr extensionContext = this->extensionContext(uniqueIdentifier);
            RefPtr storage = sqliteStore(storageDirectory(uniqueIdentifier), dataType, extensionContext);
            if (!storage) {
                RELEASE_LOG_ERROR(Extensions, "Failed to create sqlite store for extension: %{private}@", uniqueIdentifier.createNSString().get());
                record->addError(@"Unable to delete extension storage", dataType);
                continue;
            }

            removeStorage(storage, dataType, makeBlockPtr([aggregator, uniqueIdentifier, dataType, record = Ref { record }](Expected<void, WebExtensionError>&& result) mutable {
                if (!result)
                    record->addError(result.error().createNSString().get(), dataType);
                else
                    [NSDistributedNotificationCenter.defaultCenter postNotificationName:WebExtensionLocalStorageWasDeletedNotification object:nil userInfo:@{ WebExtensionUniqueIdentifierKey: uniqueIdentifier.createNSString().get() }];
            }));
        }
    }
}

void WebExtensionController::calculateStorageSize(RefPtr<WebExtensionStorageSQLiteStore> storage, WebExtensionDataType type, CompletionHandler<void(Expected<size_t, WebExtensionError>&&)>&& completionHandler)
{
    if (!storage)
        return;

    storage->getStorageSizeForKeys({ }, [completionHandler = WTFMove(completionHandler)](size_t storageSize, const String& errorMessage) mutable {
        // FIXME: <https://webkit.org/b/269100> Add storage size of window.localStorage, window.sessionStorage and indexedDB.
        if (!errorMessage.isEmpty())
            completionHandler(makeUnexpected(errorMessage));
        else
            completionHandler(storageSize);
    });
}

void WebExtensionController::removeStorage(RefPtr<WebExtensionStorageSQLiteStore> storage, WebExtensionDataType type, CompletionHandler<void(Expected<void, WebExtensionError>&&)>&& completionHandler)
{
    storage->deleteDatabase([completionHandler = WTFMove(completionHandler)](const String& errorMessage) mutable {
        // FIXME: <https://webkit.org/b/269100> Remove window.localStorage, window.sessionStorage, indexedDB.
        if (!errorMessage.isEmpty())
            completionHandler(makeUnexpected(errorMessage));
        else
            completionHandler({ });
    });
}

bool WebExtensionController::load(WebExtensionContext& extensionContext, NSError **outError)
{
    if (outError)
        *outError = nil;

    if (!m_extensionContexts.add(extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded");
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::AlreadyLoaded);
        return false;
    }

    if (!m_extensionContextBaseURLMap.add(extensionContext.baseURL().protocolHostAndPort(), extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context already loaded with same base URL: %{private}@", extensionContext.baseURL().createNSURL().get());
        m_extensionContexts.remove(extensionContext);
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::BaseURLAlreadyInUse);
        return false;
    }

    for (Ref processPool : m_processPools) {
        processPool->addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.identifier(), extensionContext);
        processPool->addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.privilegedIdentifier(), extensionContext);
    }

    auto scheme = extensionContext.baseURL().protocol().toString();
    m_registeredSchemeHandlers.ensure(scheme, [&]() {
        Ref handler = WebExtensionURLSchemeHandler::create(*this);

        for (Ref page : m_pages)
            page->setURLSchemeHandlerForScheme(handler.copyRef(), scheme);

        return handler;
    });

    auto extensionDirectory = storageDirectory(extensionContext);
    if (!!extensionDirectory && !FileSystem::makeAllDirectories(extensionDirectory))
        RELEASE_LOG_ERROR(Extensions, "Failed to create directory: %{private}@", extensionDirectory.createNSString().get());

    if (!extensionContext.load(*this, extensionDirectory, outError)) {
        m_extensionContexts.remove(extensionContext);
        m_extensionContextBaseURLMap.remove(extensionContext.baseURL().protocolHostAndPort());

        for (Ref processPool : m_processPools) {
            processPool->removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.identifier());
            processPool->removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.privilegedIdentifier());
        }

        return false;
    }

    return true;
}

bool WebExtensionController::unload(WebExtensionContext& extensionContext, NSError **outError)
{
    if (outError)
        *outError = nil;

    Ref protectedExtensionContext = extensionContext;

    if (!m_extensionContexts.remove(extensionContext)) {
        RELEASE_LOG_ERROR(Extensions, "Extension context not loaded");
        if (outError)
            *outError = extensionContext.createError(WebExtensionContext::Error::NotLoaded);
        return false;
    }

    bool result = m_extensionContextBaseURLMap.remove(extensionContext.baseURL().protocolHostAndPort());
    UNUSED_VARIABLE(result);
    ASSERT(result);

    sendToAllProcesses(Messages::WebExtensionControllerProxy::Unload(extensionContext.identifier()), identifier());

    for (Ref processPool : m_processPools) {
        processPool->removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.identifier());
        processPool->removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), extensionContext.privilegedIdentifier());
    }

    if (!extensionContext.unload(outError))
        return false;

    return true;
}

void WebExtensionController::unloadAll()
{
    auto contextsCopy = m_extensionContexts;
    for (Ref context : contextsCopy)
        unload(context, nullptr);
}

void WebExtensionController::dispatchDidLoad(WebExtensionContext& context)
{
    sendToAllProcesses(Messages::WebExtensionControllerProxy::Load(context.parameters(WebExtensionContext::IncludePrivilegedIdentifier::No)), identifier());
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
    addUserContentController(controller, dataStore->isPersistent() ? ForPrivateBrowsing::No : ForPrivateBrowsing::Yes);
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

    for (Ref context : m_extensionContexts) {
        processPool.addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context->identifier(), context);
        processPool.addMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context->privilegedIdentifier(), context);
    }
}

void WebExtensionController::removeProcessPool(WebProcessPool& processPool)
{
    // Only remove the message receiver and process pool if no other pages use the same process pool.
    for (Ref knownPage : m_pages) {
        if (knownPage->configuration().processPool() == processPool)
            return;
    }

    processPool.removeMessageReceiver(Messages::WebExtensionController::messageReceiverName(), identifier());

    for (Ref context : m_extensionContexts) {
        processPool.removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context->identifier());
        processPool.removeMessageReceiver(Messages::WebExtensionContext::messageReceiverName(), context->privilegedIdentifier());
    }

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
    }
}

void WebExtensionController::removeUserContentController(WebUserContentControllerProxy& userContentController)
{
    // Only remove the user content controller if no other pages use the same one.
    for (Ref knownPage : m_pages) {
        if (knownPage->userContentController() == userContentController)
            return;
    }

    for (Ref context : m_extensionContexts)
        context->removeInjectedContent(userContentController);

    m_allNonPrivateUserContentControllers.remove(userContentController);
    m_allPrivateUserContentControllers.remove(userContentController);
    m_allUserContentControllers.remove(userContentController);
}

WebsiteDataStore* WebExtensionController::websiteDataStore(std::optional<PAL::SessionID> sessionID) const
{
    Ref configuration = m_configuration;
    if (!sessionID || configuration->defaultWebsiteDataStore().sessionID() == sessionID.value())
        return &configuration->defaultWebsiteDataStore();

    for (Ref dataStore : allWebsiteDataStores()) {
        if (dataStore->sessionID() == sessionID.value())
            return dataStore.ptr();
    }

    return nullptr;
}

void WebExtensionController::addWebsiteDataStore(WebsiteDataStore& dataStore)
{
    m_websiteDataStores.add(dataStore);

    if (!m_cookieStoreObserver)
        m_cookieStoreObserver = HTTPCookieStoreObserver::create(*this);

    dataStore.protectedCookieStore()->registerObserver(*protectedCookieStoreObserver());
}

void WebExtensionController::removeWebsiteDataStore(WebsiteDataStore& dataStore)
{
    // Only remove the data store if no other pages use the same one.
    for (Ref knownPage : m_pages) {
        if (knownPage->websiteDataStore() == dataStore)
            return;
    }

    m_websiteDataStores.remove(dataStore);

    if (RefPtr observer = m_cookieStoreObserver)
        dataStore.protectedCookieStore()->unregisterObserver(*observer);

    if (m_websiteDataStores.isEmptyIgnoringNullReferences())
        m_cookieStoreObserver = nullptr;
}

void WebExtensionController::cookiesDidChange(API::HTTPCookieStore& cookieStore)
{
    // FIXME: <https://webkit.org/b/267514> Add support for changeInfo.

    for (Ref context : m_extensionContexts)
        context->cookiesDidChange(cookieStore);
}

bool WebExtensionController::isFeatureEnabled(const String& featureName) const
{
    WKPreferences *preferences = protectedConfiguration()->webViewConfiguration().preferences;

    auto *cocoaFeatureName = featureName.createNSString().get();
    for (_WKFeature *feature in WKPreferences._features) {
        if ([feature.key isEqualToString:cocoaFeatureName])
            return [preferences _isEnabledForFeature:feature];
    }

    return false;
}

RefPtr<WebExtensionContext> WebExtensionController::extensionContext(const WebExtension& extension) const
{
    for (Ref context : m_extensionContexts) {
        if (context->extension() == extension)
            return context.ptr();
    }

    return nullptr;
}

RefPtr<WebExtensionContext> WebExtensionController::extensionContext(const UniqueIdentifier& uniqueIdentifier) const
{
    for (Ref context : m_extensionContexts) {
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

String WebExtensionController::stateFilePath(const String& uniqueIdentifier) const
{
    return FileSystem::pathByAppendingComponent(storageDirectory(uniqueIdentifier), WebExtensionContext::plistFileName());
}

String WebExtensionController::storageDirectory(const String& uniqueIdentifier) const
{
    return FileSystem::pathByAppendingComponent(m_configuration->storageDirectory(), uniqueIdentifier);
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

#if PLATFORM(MAC)
void WebExtensionController::addItemsToContextMenu(WebPageProxy& page, const ContextMenuContextData& contextData, NSMenu *menu)
{
    [menu addItem:NSMenuItem.separatorItem];

    for (Ref context : m_extensionContexts)
        context->addItemsToContextMenu(page, contextData, menu);
}
#endif

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

// MARK: declarativeNetRequest

void WebExtensionController::handleContentRuleListNotification(WebPageProxyIdentifier pageID, URL& url, WebCore::ContentRuleListResults& results)
{
    bool savedMatchedRule = false;

    for (const auto& result : results.results) {
        auto contentRuleListIdentifier = result.first;
        for (Ref context : m_extensionContexts) {
            if (context->uniqueIdentifier() != contentRuleListIdentifier)
                continue;

            RefPtr tab = context->getTab(pageID);
            if (!tab)
                break;

            savedMatchedRule |= context->handleContentRuleListNotificationForTab(*tab, url, result.second);

            break;
        }
    }

    if (!savedMatchedRule || m_purgeOldMatchedRulesTimer)
        return;

    m_purgeOldMatchedRulesTimer = makeUnique<RunLoop::Timer>(RunLoop::mainSingleton(), this, &WebExtensionController::purgeOldMatchedRules);
    m_purgeOldMatchedRulesTimer->startRepeating(purgeMatchedRulesInterval);
}

void WebExtensionController::purgeOldMatchedRules()
{
    WallTime earliestDateToKeep = WallTime::now() - purgeMatchedRulesInterval;

    bool stillHaveRules = false;
    for (Ref context : m_extensionContexts)
        stillHaveRules |= context->purgeMatchedRulesFromBefore(earliestDateToKeep);

    if (!stillHaveRules)
        m_purgeOldMatchedRulesTimer = nullptr;
}

void WebExtensionController::updateWebsitePoliciesForNavigation(API::WebsitePolicies& websitePolicies, API::NavigationAction&)
{
    auto actionPatterns = websitePolicies.activeContentRuleListActionPatterns();

    for (Ref context : m_extensionContexts) {
        if (!context->hasPermission(WKWebExtensionPermissionDeclarativeNetRequestWithHostAccess))
            continue;

        Vector<String> patterns;
        for (Ref pattern : context->currentPermissionMatchPatterns())
            patterns.appendVector(pattern->expandedStrings());

        actionPatterns.set(context->uniqueIdentifier(), WTFMove(patterns));
    }

    websitePolicies.setActiveContentRuleListActionPatterns(WTFMove(actionPatterns));
}

void WebExtensionController::resourceLoadDidSendRequest(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceRequest& request)
{
    for (Ref context : m_extensionContexts)
        context->resourceLoadDidSendRequest(pageID, loadInfo, request);
}

void WebExtensionController::resourceLoadDidPerformHTTPRedirection(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceResponse& response, const WebCore::ResourceRequest& request)
{
    for (Ref context : m_extensionContexts)
        context->resourceLoadDidPerformHTTPRedirection(pageID, loadInfo, response, request);
}

void WebExtensionController::resourceLoadDidReceiveChallenge(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::AuthenticationChallenge& challenge)
{
    for (Ref context : m_extensionContexts)
        context->resourceLoadDidReceiveChallenge(pageID, loadInfo, challenge);
}

void WebExtensionController::resourceLoadDidReceiveResponse(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceResponse& response)
{
    for (Ref context : m_extensionContexts)
        context->resourceLoadDidReceiveResponse(pageID, loadInfo, response);
}

void WebExtensionController::resourceLoadDidCompleteWithError(WebPageProxyIdentifier pageID, const ResourceLoadInfo& loadInfo, const WebCore::ResourceResponse& response, const WebCore::ResourceError& error)
{
    for (Ref context : m_extensionContexts)
        context->resourceLoadDidCompleteWithError(pageID, loadInfo, response, error);
}

// MARK: Inspector

#if ENABLE(INSPECTOR_EXTENSIONS)
void WebExtensionController::inspectorWillOpen(WebInspectorUIProxy& inspector, WebPageProxy& inspectedPage)
{
    for (Ref context : m_extensionContexts)
        context->inspectorWillOpen(inspector, inspectedPage);
}

void WebExtensionController::inspectorWillClose(WebInspectorUIProxy& inspector, WebPageProxy& inspectedPage)
{
    for (Ref context : m_extensionContexts)
        context->inspectorWillClose(inspector, inspectedPage);
}
#endif // ENABLE(INSPECTOR_EXTENSIONS)

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
