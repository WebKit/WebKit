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
#include "RequestManagerClientEfl.h"
#include "WKAPICast.h"
#include "WKContextPrivate.h"
#include "WKContextSoup.h"
#include "WKNumber.h"
#include "WKString.h"
#include "WebIconDatabase.h"
#include "WebProcessPool.h"
#include "ewk_application_cache_manager_private.h"
#include "ewk_context_private.h"
#include "ewk_cookie_manager_private.h"
#include "ewk_database_manager_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_storage_manager_private.h"
#include "ewk_url_scheme_request_private.h"
#include <JavaScriptCore/JSContextRef.h>
#include <Shared/WebCertificateInfo.h>
#include <WebCore/CertificateInfo.h>
#include <WebCore/FileSystem.h>
#include <WebCore/IconDatabase.h>
#include <WebCore/Language.h>
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

EwkContext::EwkContext(WKContextRef context, const String& extensionsPath)
    : m_context(context)
    , m_databaseManager(std::make_unique<EwkDatabaseManager>())
    , m_storageManager(std::make_unique<EwkStorageManager>(WKContextGetKeyValueStorageManager(context)))
#if ENABLE(BATTERY_STATUS)
    , m_batteryProvider(BatteryProvider::create(context))
#endif
    , m_downloadManager(std::make_unique<DownloadManagerEfl>(context))
    , m_requestManagerClient(std::make_unique<RequestManagerClientEfl>(context))
    , m_historyClient(std::make_unique<ContextHistoryClientEfl>(context))
    , m_jsGlobalContext(nullptr)
    , m_extensionsPath(extensionsPath)
{
    // EwkContext make the context with the legacy options, it set the maximum process count to 1.
    // m_processCount also set to 1 to align with the ProcessPoolConfiguration.
    m_processCountLimit = 1;
    
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

    m_callbackForMessageFromExtension.callback = nullptr;
    m_callbackForMessageFromExtension.userData = nullptr;

    if (!extensionsPath.isEmpty() || isDefaultBundle()) {
        WKContextInjectedBundleClientV1 client;
        memset(&client, 0, sizeof(client));

        client.base.version = 1;
        client.base.clientInfo = this;
        client.didReceiveMessageFromInjectedBundle = didReceiveMessageFromInjectedBundle;
        client.getInjectedBundleInitializationUserData = getInjectedBundleInitializationUserData;

        WKContextSetInjectedBundleClient(m_context.get(), &client.base);
    }
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

static String bundlePathForExtension()
{
    String bundlePathForExtension = WebCore::pathByAppendingComponent(String::fromUTF8(TEST_LIB_DIR), EXTENSIONMANAGERNAME);
    if (WebCore::fileExists(bundlePathForExtension))
        return bundlePathForExtension;

    bundlePathForExtension = WebCore::pathByAppendingComponent(String::fromUTF8(EXTENSIONMANAGERDIR), EXTENSIONMANAGERNAME);
    if (WebCore::fileExists(bundlePathForExtension))
        return bundlePathForExtension;

    return emptyString();
}

Ref<EwkContext> EwkContext::create(const String& extensionsPath)
{   
    String bundlePath = bundlePathForExtension();
    if (bundlePath.isEmpty())
        return adoptRef(*new EwkContext(adoptWK(WKContextCreate()).get()));

    WKRetainPtr<WKStringRef> path = adoptWK(toCopiedAPI(bundlePath));

    return adoptRef(*new EwkContext(adoptWK(WKContextCreateWithInjectedBundlePath(path.get())).get(), extensionsPath));
}

EwkContext* EwkContext::defaultContext()
{
    static EwkContext* defaultInstance = &create().leakRef();

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

void EwkContext::setProcessCountLimit(unsigned count)
{
    if (m_processCountLimit == count)
        return;
    
    m_processCountLimit = count;
    WKContextSetMaximumNumberOfProcesses(m_context.get(), m_processCountLimit);
}

unsigned EwkContext::processCountLimit() const
{
    return m_processCountLimit;
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
    toEwkContext(clientInfo)->processReceivedMessageFromInjectedBundle(messageName, messageBody);
}

WKTypeRef EwkContext::getInjectedBundleInitializationUserData(WKContextRef, const void* clientInfo)
{
    return static_cast<WKTypeRef>(toCopiedAPI(toEwkContext(clientInfo)->extensionsPath()));
}

void EwkContext::setMessageFromExtensionCallback(Ewk_Context_Message_From_Extension_Cb callback, void* userData)
{
    m_callbackForMessageFromExtension.userData = userData;

    if (m_callbackForMessageFromExtension.callback == callback)
        return;

    m_callbackForMessageFromExtension.callback = callback;
}

void EwkContext::processReceivedMessageFromInjectedBundle(WKStringRef messageName, WKTypeRef messageBody)
{
    if (!m_callbackForMessageFromExtension.callback)
        return;

    CString name = toImpl(messageName)->string().utf8();
    Eina_Value* value = nullptr;
    if (messageBody && WKStringGetTypeID() == WKGetTypeID(messageBody)) {
        value = eina_value_new(EINA_VALUE_TYPE_STRING);
        CString body = toImpl(static_cast<WKStringRef>(messageBody))->string().utf8();
        eina_value_set(value, body.data());
    }

    m_callbackForMessageFromExtension.callback(name.data(), value, m_callbackForMessageFromExtension.userData);

    if (value)
        eina_value_free(value);
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

void EwkContext::allowSpecificHTTPSCertificateForHost(const String& pem, const String& host) const
{
    CString certificate = pem.ascii();

    GTlsCertificate* gTlsCertificate = g_tls_certificate_new_from_pem(
        certificate.data(), certificate.length(), nullptr);

    if (!gTlsCertificate)
        return;

    WebCore::CertificateInfo certificateInfo = WebCore::CertificateInfo(gTlsCertificate, G_TLS_CERTIFICATE_VALIDATE_ALL);

    RefPtr<WebCertificateInfo> webCertificateInfo = WebCertificateInfo::create(certificateInfo);

    toImpl(m_context.get())->allowSpecificHTTPSCertificateForHost(webCertificateInfo.get(), host);
}

bool EwkContext::isDefaultBundle() const
{
    return bundlePathForExtension() == toImpl(m_context.get())->injectedBundlePath();
}

Ewk_Context* ewk_context_default_get()
{
    return EwkContext::defaultContext();
}

Ewk_Context* ewk_context_new()
{
    return &EwkContext::create().leakRef();
}

Ewk_Context* ewk_context_new_with_extensions_path(const char* path)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(path, nullptr);

    return &EwkContext::create(String::fromUTF8(path)).leakRef();
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

void ewk_context_download_callbacks_set(Ewk_Context* ewkContext, Ewk_Download_Requested_Cb requested, Ewk_Download_Failed_Cb failed, Ewk_Download_Cancelled_Cb cancelled,  Ewk_Download_Finished_Cb finished, void* data)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl);

    impl->downloadManager()->setClientCallbacks(requested, failed, cancelled, finished, data);
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

Eina_Bool ewk_context_message_post_to_extensions(Ewk_Context* ewkContext, const char* name, const Eina_Value* body)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(name, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(body, false);

    const Eina_Value_Type* type = eina_value_type_get(body);
    if (type != EINA_VALUE_TYPE_STRINGSHARE && type != EINA_VALUE_TYPE_STRING)
        return false;

    const char* value;
    eina_value_get(body, &value);

    WKRetainPtr<WKTypeRef> messageBody = adoptWK(WKStringCreateWithUTF8CString(value));
    WKRetainPtr<WKStringRef> messageName(AdoptWK, WKStringCreateWithUTF8CString(name));
    WKContextPostMessageToInjectedBundle(impl->wkContext(), messageName.get(), messageBody.get());

    return true;
}

void ewk_context_message_from_extensions_callback_set(Ewk_Context* ewkContext, Ewk_Context_Message_From_Extension_Cb callback, void* userData)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl);

    impl->setMessageFromExtensionCallback(callback, userData);
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

void ewk_context_preferred_languages_set(Eina_List* languages)
{
    Vector<String> preferredLanguages;
    if (languages) {
        Eina_List* l;
        void* data;
        EINA_LIST_FOREACH(languages, l, data)
            preferredLanguages.append(String::fromUTF8(static_cast<char*>(data)).lower().replace("_", "-"));
    }

    WebCore::overrideUserPreferredLanguages(preferredLanguages);
    WebCore::languageDidChange();
}

void ewk_context_tls_certificate_for_host_allow(Ewk_Context* context, const char* pem, const char* host)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, context, impl);

    impl->allowSpecificHTTPSCertificateForHost(pem, host);
}

Eina_Bool ewk_context_web_process_count_limit_set(Ewk_Context* context, unsigned count)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, context, impl, false);

    impl->setProcessCountLimit(count);
    return true;
}

unsigned ewk_context_web_process_count_limit_get(const Ewk_Context* context)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, context, impl, 0);
    
    return impl->processCountLimit();
}
