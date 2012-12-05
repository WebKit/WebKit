/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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
#include "ewk_favicon_database.h"

#include "WKAPICast.h"
#include "WKIconDatabase.h"
#include "WKURL.h"
#include "WebIconDatabase.h"
#include "WebURL.h"
#include "ewk_favicon_database_private.h"
#include <WebCore/CairoUtilitiesEfl.h>
#include <WebCore/RefPtrCairo.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

EwkFaviconDatabase::EwkFaviconDatabase(WebIconDatabase* iconDatabase)
    : m_iconDatabase(iconDatabase)
{
    WKIconDatabaseClient iconDatabaseClient;
    memset(&iconDatabaseClient, 0, sizeof(WKIconDatabaseClient));
    iconDatabaseClient.version = kWKIconDatabaseClientCurrentVersion;
    iconDatabaseClient.clientInfo = this;
    iconDatabaseClient.didChangeIconForPageURL = didChangeIconForPageURL;
    iconDatabaseClient.iconDataReadyForPageURL = iconDataReadyForPageURL;
    WKIconDatabaseSetIconDatabaseClient(toAPI(m_iconDatabase.get()), &iconDatabaseClient);
}

EwkFaviconDatabase::~EwkFaviconDatabase()
{
    WKIconDatabaseSetIconDatabaseClient(toAPI(m_iconDatabase.get()), 0);
}

String EwkFaviconDatabase::iconURLForPageURL(const String& pageURL) const
{
    String iconURL;
    m_iconDatabase->synchronousIconURLForPageURL(pageURL, iconURL);

    return iconURL;
}

void EwkFaviconDatabase::watchChanges(const IconChangeCallbackData& callbackData)
{
    ASSERT(callbackData.callback);
    if (m_changeListeners.contains(callbackData.callback))
        return;

    m_changeListeners.add(callbackData.callback, callbackData);
}

void EwkFaviconDatabase::unwatchChanges(Ewk_Favicon_Database_Icon_Change_Cb callback)
{
    ASSERT(callback);
    m_changeListeners.remove(callback);
}

struct AsyncIconRequestResponse {
    String pageURL;
    RefPtr<cairo_surface_t> surface;
    IconRequestCallbackData callbackData;

    AsyncIconRequestResponse(const String& pageURL, PassRefPtr<cairo_surface_t> surface, const IconRequestCallbackData& callbackData)
        : pageURL(pageURL)
        , surface(surface)
        , callbackData(callbackData)
    { }
};

static Eina_Bool respond_icon_request_idle(void* data)
{
    AsyncIconRequestResponse* response = static_cast<AsyncIconRequestResponse*>(data);

    RefPtr<Evas_Object> icon = response->surface ? WebCore::evasObjectFromCairoImageSurface(response->callbackData.evas, response->surface.get()) : 0;
    response->callbackData.callback(response->pageURL.utf8().data(), icon.get(), response->callbackData.userData);

    delete response;

    return ECORE_CALLBACK_DONE;
}

void EwkFaviconDatabase::iconForPageURL(const String& pageURL, const IconRequestCallbackData& callbackData)
{
    // We ask for the icon directly. If we don't get the icon data now,
    // we'll be notified later (even if the database is still importing icons).
    RefPtr<cairo_surface_t> surface = getIconSurfaceSynchronously(pageURL);

    // If there's no valid icon, but there's an iconURL registered,
    // or it's still not registered but the import process hasn't
    // finished yet, we need to wait for iconDataReadyForPageURL to be
    // called before making and informed decision.
    String iconURL = iconURLForPageURL(pageURL);
    if (!surface && (!iconURL.isEmpty() || !m_iconDatabase->isUrlImportCompleted())) {
        PendingIconRequestVector requests = m_iconRequests.get(pageURL);
        requests.append(callbackData);
        m_iconRequests.set(pageURL, requests);
        return;
    }

    // Respond when idle.
    AsyncIconRequestResponse* response = new AsyncIconRequestResponse(pageURL, surface.release(), callbackData);
    ecore_idler_add(respond_icon_request_idle, response);
}

void EwkFaviconDatabase::didChangeIconForPageURL(WKIconDatabaseRef, WKURLRef pageURLRef, const void* clientInfo)
{
    const EwkFaviconDatabase* ewkIconDatabase = static_cast<const EwkFaviconDatabase*>(clientInfo);

    if (ewkIconDatabase->m_changeListeners.isEmpty())
        return;

    CString pageURL = toImpl(pageURLRef)->string().utf8();

    ChangeListenerMap::const_iterator it = ewkIconDatabase->m_changeListeners.begin();
    ChangeListenerMap::const_iterator end = ewkIconDatabase->m_changeListeners.end();
    for (; it != end; ++it)
        it->value.callback(pageURL.data(), it->value.userData);
}

PassRefPtr<cairo_surface_t> EwkFaviconDatabase::getIconSurfaceSynchronously(const String& pageURL) const
{
    m_iconDatabase->retainIconForPageURL(pageURL);

    WebCore::NativeImagePtr icon = m_iconDatabase->nativeImageForPageURL(pageURL);
    if (!icon) {
        m_iconDatabase->releaseIconForPageURL(pageURL);
        return 0;
    }

    RefPtr<cairo_surface_t> surface = icon->surface();

    return surface.release();
}

void EwkFaviconDatabase::iconDataReadyForPageURL(WKIconDatabaseRef, WKURLRef pageURL, const void* clientInfo)
{
    EwkFaviconDatabase* ewkIconDatabase = const_cast<EwkFaviconDatabase*>(static_cast<const EwkFaviconDatabase*>(clientInfo));

    String urlString = toImpl(pageURL)->string();
    if (!ewkIconDatabase->m_iconRequests.contains(urlString))
        return;

    RefPtr<cairo_surface_t> surface = ewkIconDatabase->getIconSurfaceSynchronously(urlString);

    PendingIconRequestVector requestsForURL = ewkIconDatabase->m_iconRequests.take(urlString);
    size_t requestCount = requestsForURL.size();
    for (size_t i = 0; i < requestCount; ++i) {
        const IconRequestCallbackData& callbackData = requestsForURL[i];
        RefPtr<Evas_Object> icon = surface ? WebCore::evasObjectFromCairoImageSurface(callbackData.evas, surface.get()) : 0;
        callbackData.callback(urlString.utf8().data(), icon.get(), callbackData.userData);
    }
}

const char* ewk_favicon_database_icon_url_get(Ewk_Favicon_Database* ewkIconDatabase, const char* pageURL)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkIconDatabase, 0);
    EINA_SAFETY_ON_NULL_RETURN_VAL(pageURL, 0);

    String iconURL = ewkIconDatabase->iconURLForPageURL(String::fromUTF8(pageURL));

    return eina_stringshare_add(iconURL.utf8().data());
}

Eina_Bool ewk_favicon_database_async_icon_get(Ewk_Favicon_Database* ewkIconDatabase, const char* page_url, Evas* evas, Ewk_Favicon_Database_Async_Icon_Get_Cb callback, void* userData)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkIconDatabase, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(page_url, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(evas, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(callback, false);

    ewkIconDatabase->iconForPageURL(String::fromUTF8(page_url), IconRequestCallbackData(callback, userData, evas));

    return true;
}

void ewk_favicon_database_icon_change_callback_add(Ewk_Favicon_Database* ewkIconDatabase, Ewk_Favicon_Database_Icon_Change_Cb callback, void* userData)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkIconDatabase);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    ewkIconDatabase->watchChanges(IconChangeCallbackData(callback, userData));
}

void ewk_favicon_database_icon_change_callback_del(Ewk_Favicon_Database* ewkIconDatabase, Ewk_Favicon_Database_Icon_Change_Cb callback)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkIconDatabase);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    ewkIconDatabase->unwatchChanges(callback);
}
