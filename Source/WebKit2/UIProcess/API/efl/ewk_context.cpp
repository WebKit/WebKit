/*
 * Copyright (C) 2012 Samsung Electronics
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this program; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"
#include "ewk_context.h"

#include "BatteryProvider.h"
#include "ContextHistoryClientEfl.h"
#include "DownloadManagerEfl.h"
#include "NetworkInfoProvider.h"
#include "RequestManagerClientEfl.h"
#include "WKAPICast.h"
#include "WKContextPrivate.h"
#include "WKContextSoup.h"
#include "WKNumber.h"
#include "WKString.h"
#include "WebContext.h"
#include "WebIconDatabase.h"
#include "ewk_application_cache_manager_private.h"
#include "ewk_context_private.h"
#include "ewk_cookie_manager_private.h"
#include "ewk_database_manager_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_private.h"
#include "ewk_storage_manager_private.h"
#include "ewk_url_scheme_request_private.h"
#include <JavaScriptCore/JSContextRef.h>
#include <WebCore/FileSystem.h>
#include <WebCore/IconDatabase.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/WTFString.h>

#if ENABLE(SPELLCHECK)
#include "TextCheckerClientEfl.h"
#endif

using namespace WebCore;
using namespace WebKit;

typedef HashMap<WKContextRef, EwkContext*> ContextMap;

static inline ContextMap& contextMap()
{
    static NeverDestroyed<ContextMap> map;
    return map;
}

EwkContext::EwkContext(WKContextRef context)
    : m_context(context)
    , m_databaseManager(std::make_unique<EwkDatabaseManager>(WKContextGetDatabaseManager(context)))
    , m_storageManager(std::make_unique<EwkStorageManager>(WKContextGetKeyValueStorageManager(context)))
#if ENABLE(BATTERY_STATUS)
    , m_batteryProvider(BatteryProvider::create(context))
#endif
#if ENABLE(NETWORK_INFO)
    , m_networkInfoProvider(NetworkInfoProvider::create(context))
#endif
    , m_downloadManager(std::make_unique<DownloadManagerEfl>(context))
    , m_requestManagerClient(std::make_unique<RequestManagerClientEfl>(context))
    , m_historyClient(std::make_unique<ContextHistoryClientEfl>(context))
    , m_jsGlobalContext(nullptr)
{
    ContextMap::AddResult result = contextMap().add(context, this);
    ASSERT_UNUSED(result, result.isNewEntry);

#if ENABLE(MEMORY_SAMPLER)
    static bool initializeMemorySampler = false;
    static const char environmentVariable[] = "SAMPLE_MEMORY";

    if (!initializeMemorySampler && getenv(environmentVariable)) {
        WKContextStartMemorySampler(context, adoptWK(WKDoubleCreate(0.0)).get());
        initializeMemorySampler = true;
    }
#endif

#if ENABLE(SPELLCHECK)
    // Load the default dictionary to show context menu spellchecking items
    // independently of checking spelling while typing setting.
    TextCheckerClientEfl::instance().ensureSpellCheckingLanguage();
#endif

    m_callbackForMessageFromInjectedBundle.callback = nullptr;
    m_callbackForMessageFromInjectedBundle.userData = nullptr;
}

EwkContext::~EwkContext()
{
    ASSERT(contextMap().get(m_context.get()) == this);

    if (m_jsGlobalContext)
        JSGlobalContextRelease(m_jsGlobalContext);

    contextMap().remove(m_context.get());
}

PassRefPtr<EwkContext> EwkContext::findOrCreateWrapper(WKContextRef context)
{
    if (contextMap().contains(context))
        return contextMap().get(context);

    return adoptRef(new EwkContext(context));
}

PassRefPtr<EwkContext> EwkContext::create()
{
    return adoptRef(new EwkContext(adoptWK(WKContextCreate()).get()));
}

PassRefPtr<EwkContext> EwkContext::create(const String& injectedBundlePath)
{   
    if (!fileExists(injectedBundlePath))
        return 0;

    WKRetainPtr<WKStringRef> path = adoptWK(toCopiedAPI(injectedBundlePath));

    return adoptRef(new EwkContext(adoptWK(WKContextCreateWithInjectedBundlePath(path.get())).get()));
}

EwkContext* EwkContext::defaultContext()
{
    static EwkContext* defaultInstance = create().leakRef();

    return defaultInstance;
}

EwkApplicationCacheManager* EwkContext::applicationCacheManager()
{
    if (!m_applicationCacheManager)
        m_applicationCacheManager = std::make_unique<EwkApplicationCacheManager>(WKContextGetApplicationCacheManager(m_context.get()));

    return m_applicationCacheManager.get();
}

EwkCookieManager* EwkContext::cookieManager()
{
    if (!m_cookieManager)
        m_cookieManager = std::make_unique<EwkCookieManager>(WKContextGetCookieManager(m_context.get()));

    return m_cookieManager.get();
}

EwkDatabaseManager* EwkContext::databaseManager()
{
    return m_databaseManager.get();
}

void EwkContext::ensureFaviconDatabase()
{
    if (m_faviconDatabase)
        return;

    m_faviconDatabase = std::make_unique<EwkFaviconDatabase>(WKContextGetIconDatabase(m_context.get()));
}

bool EwkContext::setFaviconDatabaseDirectoryPath(const String& databaseDirectory)
{
    ensureFaviconDatabase();
    // FIXME: Hole in WK2 API layering must be fixed when C API is available.
    WebIconDatabase* iconDatabase = toImpl(WKContextGetIconDatabase(m_context.get()));

    // The database path is already open so its path was
    // already set.
    if (iconDatabase->isOpen())
        return false;

    // If databaseDirectory is empty, we use the default database path for the platform.
    String databasePath = databaseDirectory.isEmpty() ? toImpl(m_context.get())->iconDatabasePath() : pathByAppendingComponent(databaseDirectory, WebCore::IconDatabase::defaultDatabaseFilename());
    toImpl(m_context.get())->setIconDatabasePath(databasePath);

    return true;
}

EwkFaviconDatabase* EwkContext::faviconDatabase()
{
    ensureFaviconDatabase();
    ASSERT(m_faviconDatabase);

    return m_faviconDatabase.get();
}

EwkStorageManager* EwkContext::storageManager() const
{
    return m_storageManager.get();
}

RequestManagerClientEfl* EwkContext::requestManager()
{
    return m_requestManagerClient.get();
}

void EwkContext::addVisitedLink(const String& visitedURL)
{
    WKContextAddVisitedLink(m_context.get(), adoptWK(toCopiedAPI(visitedURL)).get());
}

// Ewk_Cache_Model enum validation
inline WKCacheModel toWKCacheModel(Ewk_Cache_Model cacheModel)
{
    switch (cacheModel) {
    case EWK_CACHE_MODEL_DOCUMENT_VIEWER:
        return kWKCacheModelDocumentViewer;
    case EWK_CACHE_MODEL_DOCUMENT_BROWSER:
        return kWKCacheModelDocumentBrowser;
    case EWK_CACHE_MODEL_PRIMARY_WEBBROWSER:
        return kWKCacheModelPrimaryWebBrowser;
    }
    ASSERT_NOT_REACHED();

    return kWKCacheModelDocumentViewer;
}

void EwkContext::setCacheModel(Ewk_Cache_Model cacheModel)
{
    WKContextSetCacheModel(m_context.get(), toWKCacheModel(cacheModel));
}

Ewk_Cache_Model EwkContext::cacheModel() const
{
    return static_cast<Ewk_Cache_Model>(WKContextGetCacheModel(m_context.get()));
}

inline WKProcessModel toWKProcessModel(Ewk_Process_Model processModel)
{
    switch (processModel) {
    case EWK_PROCESS_MODEL_SHARED_SECONDARY:
        return kWKProcessModelSharedSecondaryProcess;
    case EWK_PROCESS_MODEL_MULTIPLE_SECONDARY:
        return kWKProcessModelMultipleSecondaryProcesses;
    }
    ASSERT_NOT_REACHED();

    return kWKProcessModelSharedSecondaryProcess;
}

void EwkContext::setProcessModel(Ewk_Process_Model processModel)
{
    WKProcessModel newWKProcessModel = toWKProcessModel(processModel);

    if (WKContextGetProcessModel(m_context.get()) == newWKProcessModel)
        return;

    WKContextSetUsesNetworkProcess(m_context.get(), newWKProcessModel == kWKProcessModelMultipleSecondaryProcesses);
    WKContextSetProcessModel(m_context.get(), newWKProcessModel);
}

inline Ewk_Process_Model toEwkProcessModel(WKProcessModel processModel)
{
    switch (processModel) {
    case kWKProcessModelSharedSecondaryProcess:
        return EWK_PROCESS_MODEL_SHARED_SECONDARY;
    case kWKProcessModelMultipleSecondaryProcesses:
        return EWK_PROCESS_MODEL_MULTIPLE_SECONDARY;
    }
    ASSERT_NOT_REACHED();

    return EWK_PROCESS_MODEL_SHARED_SECONDARY;
}

Ewk_Process_Model EwkContext::processModel() const
{
    return toEwkProcessModel(WKContextGetProcessModel(m_context.get()));
}

#if ENABLE(NETSCAPE_PLUGIN_API)
void EwkContext::setAdditionalPluginPath(const String& path)
{
    // FIXME: Hole in WK2 API layering must be fixed when C API is available.
    toImpl(m_context.get())->setAdditionalPluginsDirectory(path);
}
#endif

void EwkContext::clearResourceCache()
{
    WKResourceCacheManagerClearCacheForAllOrigins(WKContextGetResourceCacheManager(m_context.get()), WKResourceCachesToClearAll);
}


JSGlobalContextRef EwkContext::jsGlobalContext()
{
    if (!m_jsGlobalContext)
        m_jsGlobalContext = JSGlobalContextCreate(0);

    return m_jsGlobalContext;
}

Ewk_Application_Cache_Manager* ewk_context_application_cache_manager_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, nullptr);

    return const_cast<EwkContext*>(impl)->applicationCacheManager();
}

Ewk_Cookie_Manager* ewk_context_cookie_manager_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, nullptr);

    return const_cast<EwkContext*>(impl)->cookieManager();
}

Ewk_Database_Manager* ewk_context_database_manager_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, nullptr);

    return const_cast<EwkContext*>(impl)->databaseManager();
}

Eina_Bool ewk_context_favicon_database_directory_set(Ewk_Context* ewkContext, const char* directoryPath)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl, false);

    return impl->setFaviconDatabaseDirectoryPath(String::fromUTF8(directoryPath));
}

Ewk_Favicon_Database* ewk_context_favicon_database_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, nullptr);

    return const_cast<EwkContext*>(impl)->faviconDatabase();
}

Ewk_Storage_Manager* ewk_context_storage_manager_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, nullptr);

    return impl->storageManager();
}

DownloadManagerEfl* EwkContext::downloadManager() const
{
    return m_downloadManager.get();
}

ContextHistoryClientEfl* EwkContext::historyClient()
{
    return m_historyClient.get();
}

static inline EwkContext* toEwkContext(const void* clientInfo)
{
    return static_cast<EwkContext*>(const_cast<void*>(clientInfo));
}

void EwkContext::didReceiveMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, const void* clientInfo)
{
    toEwkContext(clientInfo)->processReceivedMessageFromInjectedBundle(messageName, messageBody, nullptr);
}

void EwkContext::didReceiveSynchronousMessageFromInjectedBundle(WKContextRef, WKStringRef messageName, WKTypeRef messageBody, WKTypeRef* returnData, const void* clientInfo)
{
    toEwkContext(clientInfo)->processReceivedMessageFromInjectedBundle(messageName, messageBody, returnData);
}

void EwkContext::setMessageFromInjectedBundleCallback(Ewk_Context_Message_From_Injected_Bundle_Cb callback, void* userData)
{
    m_callbackForMessageFromInjectedBundle.userData = userData;

    if (m_callbackForMessageFromInjectedBundle.callback == callback)
        return;

    if (!m_callbackForMessageFromInjectedBundle.callback) {
        WKContextInjectedBundleClientV1 client;
        memset(&client, 0, sizeof(client));

        client.base.version = 1;
        client.base.clientInfo = this;
        client.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;
        client.didReceiveSynchronousMessageFromInjectedBundle = didReceiveSynchronousMessageFromInjectedBundle;

        WKContextSetInjectedBundleClient(m_context.get(), &client.base);
    } else if (!callback)
        WKContextSetInjectedBundleClient(m_context.get(), nullptr);

    m_callbackForMessageFromInjectedBundle.callback = callback;
}

void EwkContext::processReceivedMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody, WKTypeRef* returnData)
{
    if (!m_callbackForMessageFromInjectedBundle.callback)
        return;

    CString name = toImpl(messageName)->string().utf8();
    CString body;
    if (messageBody && WKStringGetTypeID() == WKGetTypeID(messageBody))
        body = toImpl(static_cast<WKStringRef>(messageBody))->string().utf8();

    if (returnData) {
        char* returnString = nullptr;
        m_callbackForMessageFromInjectedBundle.callback(name.data(), body.data(), &returnString, m_callbackForMessageFromInjectedBundle.userData);
        if (returnString) {
            *returnData = WKStringCreateWithUTF8CString(returnString);
            free(returnString);
        } else
            *returnData = WKStringCreateWithUTF8CString("");
    } else
        m_callbackForMessageFromInjectedBundle.callback(name.data(), body.data(), nullptr, m_callbackForMessageFromInjectedBundle.userData);
}

Ewk_TLS_Error_Policy EwkContext::ignoreTLSErrors() const
{
    return toImpl(m_context.get())->ignoreTLSErrors() ? EWK_TLS_ERROR_POLICY_IGNORE : EWK_TLS_ERROR_POLICY_FAIL;
}

void EwkContext::setIgnoreTLSErrors(Ewk_TLS_Error_Policy TLSErrorPolicy) const
{
    bool isNewPolicy = TLSErrorPolicy == EWK_TLS_ERROR_POLICY_IGNORE;
    if (toImpl(m_context.get())->ignoreTLSErrors() == isNewPolicy)
        return;

    toImpl(m_context.get())->setIgnoreTLSErrors(isNewPolicy);
}

Ewk_Context* ewk_context_default_get()
{
    return EwkContext::defaultContext();
}

Ewk_Context* ewk_context_new()
{
    return EwkContext::create().leakRef();
}

Ewk_Context* ewk_context_new_with_injected_bundle_path(const char* path)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(path, nullptr);

    return EwkContext::create(String::fromUTF8(path)).leakRef();
}

Eina_Bool ewk_context_url_scheme_register(Ewk_Context* ewkContext, const char* scheme, Ewk_Url_Scheme_Request_Cb callback, void* userData)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(scheme, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(callback, false);

    impl->requestManager()->registerURLSchemeHandler(String::fromUTF8(scheme), callback, userData);

    return true;
}

void ewk_context_history_callbacks_set(Ewk_Context* ewkContext, Ewk_History_Navigation_Cb navigate, Ewk_History_Client_Redirection_Cb clientRedirect, Ewk_History_Server_Redirection_Cb serverRedirect, Ewk_History_Title_Update_Cb titleUpdate, Ewk_History_Populate_Visited_Links_Cb populateVisitedLinks, void* data)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl);

    impl->historyClient()->setCallbacks(navigate, clientRedirect, serverRedirect, titleUpdate, populateVisitedLinks, data);
}

void ewk_context_visited_link_add(Ewk_Context* ewkContext, const char* visitedURL)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl);
    EINA_SAFETY_ON_NULL_RETURN(visitedURL);

    impl->addVisitedLink(visitedURL);
}

Eina_Bool ewk_context_cache_model_set(Ewk_Context* ewkContext, Ewk_Cache_Model cacheModel)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl, false);

    impl->setCacheModel(cacheModel);

    return true;
}

Ewk_Cache_Model ewk_context_cache_model_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, EWK_CACHE_MODEL_DOCUMENT_VIEWER);

    return impl->cacheModel();
}

Eina_Bool ewk_context_additional_plugin_path_set(Ewk_Context* ewkContext, const char* path)
{
#if ENABLE(NETSCAPE_PLUGIN_API)
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(path, false);

    impl->setAdditionalPluginPath(String::fromUTF8(path));
    return true;
#else
    UNUSED_PARAM(ewkContext);
    UNUSED_PARAM(path);
    return false;
#endif
}

void ewk_context_resource_cache_clear(Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl);

    impl->clearResourceCache();
}

void ewk_context_message_post_to_injected_bundle(Ewk_Context* ewkContext, const char* name, const char* body)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl);
    EINA_SAFETY_ON_NULL_RETURN(name);
    EINA_SAFETY_ON_NULL_RETURN(body);

    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString(name));
    WKRetainPtr<WKStringRef> messageBody(AdoptWK, WKStringCreateWithUTF8CString(body));
    WKContextPostMessageToInjectedBundle(impl->wkContext(), messageName.get(), messageBody.get());
}

void ewk_context_message_from_injected_bundle_callback_set(Ewk_Context* ewkContext, Ewk_Context_Message_From_Injected_Bundle_Cb callback, void* userData)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl);

    impl->setMessageFromInjectedBundleCallback(callback, userData);
}

Eina_Bool ewk_context_process_model_set(Ewk_Context* ewkContext, Ewk_Process_Model processModel)
{
#if ENABLE(NETWORK_PROCESS)
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl, false);

    impl->setProcessModel(processModel);

    return true;
#else
    UNUSED_PARAM(ewkContext);
    UNUSED_PARAM(processModel);
    return false;
#endif
}

Ewk_Process_Model ewk_context_process_model_get(const Ewk_Context* ewkContext)
{
#if ENABLE(NETWORK_PROCESS)
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, EWK_PROCESS_MODEL_SHARED_SECONDARY);

    return impl->processModel();
#else
    UNUSED_PARAM(ewkContext);
    return EWK_PROCESS_MODEL_SHARED_SECONDARY;
#endif
}

Ewk_TLS_Error_Policy ewk_context_tls_error_policy_get(const Ewk_Context* context)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, context, impl, EWK_TLS_ERROR_POLICY_FAIL);

    return impl->ignoreTLSErrors();
}

void ewk_context_tls_error_policy_set(Ewk_Context* context, Ewk_TLS_Error_Policy tls_error_policy)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, context, impl);
    impl->setIgnoreTLSErrors(tls_error_policy);
}
