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
#include "NetworkInfoProvider.h"
#include "RequestManagerClientEfl.h"
#include "WKAPICast.h"
#include "WKContextSoup.h"
#include "WKNumber.h"
#include "WKRetainPtr.h"
#include "WKString.h"
#include "WebContext.h"
#include "WebIconDatabase.h"
#include "WebSoupRequestManagerProxy.h"
#include "ewk_context_private.h"
#include "ewk_cookie_manager_private.h"
#include "ewk_database_manager_private.h"
#include "ewk_favicon_database_private.h"
#include "ewk_private.h"
#include "ewk_storage_manager_private.h"
#include "ewk_url_scheme_request_private.h"
#include <WebCore/FileSystem.h>
#include <WebCore/IconDatabase.h>
#include <wtf/HashMap.h>
#include <wtf/text/WTFString.h>

#if ENABLE(SPELLCHECK)
#include "ewk_settings.h"
#include "ewk_text_checker_private.h"
#endif

using namespace WebCore;
using namespace WebKit;

typedef HashMap<WKContextRef, EwkContext*> ContextMap;

static inline ContextMap& contextMap()
{
    DEFINE_STATIC_LOCAL(ContextMap, map, ());
    return map;
}

EwkContext::EwkContext(WKContextRef context)
    : m_context(context)
    , m_databaseManager(Ewk_Database_Manager::create(WKContextGetDatabaseManager(m_context.get())))
    , m_storageManager(Ewk_Storage_Manager::create(WKContextGetKeyValueStorageManager(m_context.get())))
#if ENABLE(BATTERY_STATUS)
    , m_batteryProvider(BatteryProvider::create(context))
#endif
#if ENABLE(NETWORK_INFO)
    , m_networkInfoProvider(NetworkInfoProvider::create(context))
#endif
    , m_downloadManager(DownloadManagerEfl::create(this))
    , m_requestManagerClient(RequestManagerClientEfl::create(this))
    , m_historyClient(ContextHistoryClientEfl::create(context))
{
    ContextMap::AddResult result = contextMap().add(context, this);
    ASSERT_UNUSED(result, result.isNewEntry);

#if ENABLE(MEMORY_SAMPLER)
    static bool initializeMemorySampler = false;
    static const char environmentVariable[] = "SAMPLE_MEMORY";

    if (!initializeMemorySampler && getenv(environmentVariable)) {
        WKRetainPtr<WKDoubleRef> interval(AdoptWK, WKDoubleCreate(0.0));
        WKContextStartMemorySampler(context, interval.get());
        initializeMemorySampler = true;
    }
#endif

#if ENABLE(SPELLCHECK)
    Ewk_Text_Checker::initialize();
    if (ewk_settings_continuous_spell_checking_enabled_get()) {
        // Load the default language.
        ewk_settings_spell_checking_languages_set(0);
    }
#endif
}

EwkContext::~EwkContext()
{
    ASSERT(contextMap().get(m_context.get()) == this);
    contextMap().remove(m_context.get());
}

PassRefPtr<EwkContext> EwkContext::create(WKContextRef context)
{
    if (contextMap().contains(context))
        return contextMap().get(context); // Will be ref-ed automatically.

    return adoptRef(new EwkContext(context));
}

PassRefPtr<EwkContext> EwkContext::create()
{
    return create(adoptWK(WKContextCreate()).get());
}

PassRefPtr<EwkContext> EwkContext::create(const String& injectedBundlePath)
{   
    if (!fileExists(injectedBundlePath))
        return 0;

    WKRetainPtr<WKStringRef> injectedBundlePathWK = adoptWK(toCopiedAPI(injectedBundlePath));
    WKRetainPtr<WKContextRef> contextWK = adoptWK(WKContextCreateWithInjectedBundlePath(injectedBundlePathWK.get()));

    return create(contextWK.get());
}

PassRefPtr<EwkContext> EwkContext::defaultContext()
{
    static RefPtr<EwkContext> defaultInstance = create(adoptWK(WKContextCreate()).get());

    return defaultInstance;
}

Ewk_Cookie_Manager* EwkContext::cookieManager()
{
    if (!m_cookieManager)
        m_cookieManager = Ewk_Cookie_Manager::create(WKContextGetCookieManager(m_context.get()));

    return m_cookieManager.get();
}

Ewk_Database_Manager* EwkContext::databaseManager()
{
    return m_databaseManager.get();
}

void EwkContext::ensureFaviconDatabase()
{
    if (m_faviconDatabase)
        return;

    m_faviconDatabase = Ewk_Favicon_Database::create(toImpl(m_context.get())->iconDatabase());
}

bool EwkContext::setFaviconDatabaseDirectoryPath(const String& databaseDirectory)
{
    ensureFaviconDatabase();

    WebContext* webContext = toImpl(m_context.get());

    // The database path is already open so its path was
    // already set.
    if (webContext->iconDatabase()->isOpen())
        return false;

    // If databaseDirectory is empty, we use the default database path for the platform.
    String databasePath = databaseDirectory.isEmpty() ? webContext->iconDatabasePath() : pathByAppendingComponent(databaseDirectory, WebCore::IconDatabase::defaultDatabaseFilename());
    webContext->setIconDatabasePath(databasePath);

    return true;
}

Ewk_Favicon_Database* EwkContext::faviconDatabase()
{
    ensureFaviconDatabase();
    ASSERT(m_faviconDatabase);

    return m_faviconDatabase.get();
}

Ewk_Storage_Manager* EwkContext::storageManager() const
{
    return m_storageManager.get();
}

RequestManagerClientEfl* EwkContext::requestManager()
{
    return m_requestManagerClient.get();
}

void EwkContext::addVisitedLink(const String& visitedURL)
{
    toImpl(m_context.get())->addVisitedLink(visitedURL);
}

void EwkContext::setCacheModel(Ewk_Cache_Model cacheModel)
{
    WKContextSetCacheModel(m_context.get(), static_cast<Ewk_Cache_Model>(cacheModel));
}

Ewk_Cache_Model EwkContext::cacheModel() const
{
    return static_cast<Ewk_Cache_Model>(WKContextGetCacheModel(m_context.get()));
}

Ewk_Cookie_Manager* ewk_context_cookie_manager_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, 0);

    return const_cast<EwkContext*>(impl)->cookieManager();
}

Ewk_Database_Manager* ewk_context_database_manager_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, 0);

    return const_cast<EwkContext*>(impl)->databaseManager();
}

Eina_Bool ewk_context_favicon_database_directory_set(Ewk_Context* ewkContext, const char* directoryPath)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(EwkContext, ewkContext, impl, false);

    return impl->setFaviconDatabaseDirectoryPath(String::fromUTF8(directoryPath));
}

Ewk_Favicon_Database* ewk_context_favicon_database_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, 0);

    return const_cast<EwkContext*>(impl)->faviconDatabase();
}

Ewk_Storage_Manager* ewk_context_storage_manager_get(const Ewk_Context* ewkContext)
{
    EWK_OBJ_GET_IMPL_OR_RETURN(const EwkContext, ewkContext, impl, 0);

    return impl->storageManager();
}

WKContextRef EwkContext::wkContext()
{
    return m_context.get();
}

DownloadManagerEfl* EwkContext::downloadManager() const
{
    return m_downloadManager.get();
}

ContextHistoryClientEfl* EwkContext::historyClient()
{
    return m_historyClient.get();
}

Ewk_Context* ewk_context_default_get()
{
    return EwkContext::defaultContext().get();
}

Ewk_Context* ewk_context_new()
{
    return EwkContext::create().leakRef();
}

Ewk_Context* ewk_context_new_with_injected_bundle_path(const char* path)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(path, 0);

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

// Ewk_Cache_Model enum validation
COMPILE_ASSERT_MATCHING_ENUM(EWK_CACHE_MODEL_DOCUMENT_VIEWER, kWKCacheModelDocumentViewer);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CACHE_MODEL_DOCUMENT_BROWSER, kWKCacheModelDocumentBrowser);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CACHE_MODEL_PRIMARY_WEBBROWSER, kWKCacheModelPrimaryWebBrowser);

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

