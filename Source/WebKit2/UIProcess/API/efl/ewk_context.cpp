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
#include "VibrationProvider.h"
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

typedef HashMap<WKContextRef, Ewk_Context*> ContextMap;

static inline ContextMap& contextMap()
{
    DEFINE_STATIC_LOCAL(ContextMap, map, ());
    return map;
}

Ewk_Context::Ewk_Context(WKContextRef context)
    : m_context(context)
    , m_storageManager(Ewk_Storage_Manager::create(WKContextGetKeyValueStorageManager(m_context.get())))
#if ENABLE(BATTERY_STATUS)
    , m_batteryProvider(BatteryProvider::create(context))
#endif
#if ENABLE(NETWORK_INFO)
    , m_networkInfoProvider(NetworkInfoProvider::create(context))
#endif
#if ENABLE(VIBRATION)
    , m_vibrationProvider(VibrationProvider::create(context))
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

Ewk_Context::~Ewk_Context()
{
    ASSERT(contextMap().get(m_context.get()) == this);
    contextMap().remove(m_context.get());
}

PassRefPtr<Ewk_Context> Ewk_Context::create(WKContextRef context)
{
    if (contextMap().contains(context))
        return contextMap().get(context); // Will be ref-ed automatically.

    return adoptRef(new Ewk_Context(context));
}

PassRefPtr<Ewk_Context> Ewk_Context::create()
{
    return create(adoptWK(WKContextCreate()).get());
}

PassRefPtr<Ewk_Context> Ewk_Context::create(const String& injectedBundlePath)
{   
    if (!fileExists(injectedBundlePath))
        return 0;

    WKRetainPtr<WKStringRef> injectedBundlePathWK = adoptWK(toCopiedAPI(injectedBundlePath));
    WKRetainPtr<WKContextRef> contextWK = adoptWK(WKContextCreateWithInjectedBundlePath(injectedBundlePathWK.get()));

    return create(contextWK.get());
}

PassRefPtr<Ewk_Context> Ewk_Context::defaultContext()
{
    static RefPtr<Ewk_Context> defaultInstance = create(adoptWK(WKContextCreate()).get());

    return defaultInstance;
}

Ewk_Cookie_Manager* Ewk_Context::cookieManager()
{
    if (!m_cookieManager)
        m_cookieManager = Ewk_Cookie_Manager::create(WKContextGetCookieManager(m_context.get()));

    return m_cookieManager.get();
}

Ewk_Favicon_Database* Ewk_Context::faviconDatabase()
{
    if (!m_faviconDatabase) {
        WKRetainPtr<WKIconDatabaseRef> iconDatabase = WKContextGetIconDatabase(m_context.get());
        // Set the database path if it is not open yet.
        if (!toImpl(iconDatabase.get())->isOpen()) {
            WebContext* webContext = toImpl(m_context.get());
            webContext->setIconDatabasePath(webContext->iconDatabasePath());
        }
        m_faviconDatabase = Ewk_Favicon_Database::create(iconDatabase.get());
    }

    return m_faviconDatabase.get();
}

Ewk_Storage_Manager* Ewk_Context::storageManager() const
{
    return m_storageManager.get();
}

RequestManagerClientEfl* Ewk_Context::requestManager()
{
    return m_requestManagerClient.get();
}

#if ENABLE(VIBRATION)
PassRefPtr<VibrationProvider> Ewk_Context::vibrationProvider()
{
    return m_vibrationProvider;
}
#endif

void Ewk_Context::addVisitedLink(const String& visitedURL)
{
    toImpl(m_context.get())->addVisitedLink(visitedURL);
}

void Ewk_Context::setCacheModel(Ewk_Cache_Model cacheModel)
{
    WKContextSetCacheModel(m_context.get(), static_cast<Ewk_Cache_Model>(cacheModel));
}

Ewk_Cache_Model Ewk_Context::cacheModel() const
{
    return static_cast<Ewk_Cache_Model>(WKContextGetCacheModel(m_context.get()));
}

Ewk_Context* ewk_context_ref(Ewk_Context* ewkContext)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, 0);

    ewkContext->ref();

    return ewkContext;
}

void ewk_context_unref(Ewk_Context* ewkContext)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkContext);

    ewkContext->deref();
}

Ewk_Cookie_Manager* ewk_context_cookie_manager_get(const Ewk_Context* ewkContext)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, 0);

    return const_cast<Ewk_Context*>(ewkContext)->cookieManager();
}

Ewk_Favicon_Database* ewk_context_favicon_database_get(const Ewk_Context* ewkContext)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, 0);

    return const_cast<Ewk_Context*>(ewkContext)->faviconDatabase();
}

Ewk_Storage_Manager* ewk_context_storage_manager_get(const Ewk_Context* ewkContext)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, 0);

    return ewkContext->storageManager();
}

WKContextRef Ewk_Context::wkContext()
{
    return m_context.get();
}

DownloadManagerEfl* Ewk_Context::downloadManager() const
{
    return m_downloadManager.get();
}

ContextHistoryClientEfl* Ewk_Context::historyClient()
{
    return m_historyClient.get();
}

Ewk_Context* ewk_context_default_get()
{
    return Ewk_Context::defaultContext().get();
}

Ewk_Context* ewk_context_new()
{
    return Ewk_Context::create().leakRef();
}

Ewk_Context* ewk_context_new_with_injected_bundle_path(const char* path)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(path, 0);

    return Ewk_Context::create(String::fromUTF8(path)).leakRef();
}

Eina_Bool ewk_context_url_scheme_register(Ewk_Context* ewkContext, const char* scheme, Ewk_Url_Scheme_Request_Cb callback, void* userData)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(scheme, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(callback, false);

    ewkContext->requestManager()->registerURLSchemeHandler(String::fromUTF8(scheme), callback, userData);

    return true;
}

void ewk_context_vibration_client_callbacks_set(Ewk_Context* ewkContext, Ewk_Vibration_Client_Vibrate_Cb vibrate, Ewk_Vibration_Client_Vibration_Cancel_Cb cancel, void* data)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkContext);

#if ENABLE(VIBRATION)
    ewkContext->vibrationProvider()->setVibrationClientCallbacks(vibrate, cancel, data);
#endif
}

void ewk_context_history_callbacks_set(Ewk_Context* ewkContext, Ewk_History_Navigation_Cb navigate, Ewk_History_Client_Redirection_Cb clientRedirect, Ewk_History_Server_Redirection_Cb serverRedirect, Ewk_History_Title_Update_Cb titleUpdate, Ewk_History_Populate_Visited_Links_Cb populateVisitedLinks, void* data)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkContext);

    ewkContext->historyClient()->setCallbacks(navigate, clientRedirect, serverRedirect, titleUpdate, populateVisitedLinks, data);
}

void ewk_context_visited_link_add(Ewk_Context* ewkContext, const char* visitedURL)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkContext);
    EINA_SAFETY_ON_NULL_RETURN(visitedURL);

    ewkContext->addVisitedLink(visitedURL);
}

// Ewk_Cache_Model enum validation
COMPILE_ASSERT_MATCHING_ENUM(EWK_CACHE_MODEL_DOCUMENT_VIEWER, kWKCacheModelDocumentViewer);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CACHE_MODEL_DOCUMENT_BROWSER, kWKCacheModelDocumentBrowser);
COMPILE_ASSERT_MATCHING_ENUM(EWK_CACHE_MODEL_PRIMARY_WEBBROWSER, kWKCacheModelPrimaryWebBrowser);

Eina_Bool ewk_context_cache_model_set(Ewk_Context* ewkContext, Ewk_Cache_Model cacheModel)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, false);

    ewkContext->setCacheModel(cacheModel);

    return true;
}

Ewk_Cache_Model ewk_context_cache_model_get(const Ewk_Context* ewkContext)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkContext, EWK_CACHE_MODEL_DOCUMENT_VIEWER);

    return ewkContext->cacheModel();
}

