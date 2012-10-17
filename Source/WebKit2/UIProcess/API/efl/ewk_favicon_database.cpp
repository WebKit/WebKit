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
#include "WKURL.h"
#include "WebIconDatabase.h"
#include "WebURL.h"
#include "ewk_favicon_database_private.h"
#include <WebCore/CairoUtilitiesEfl.h>
#include <WebCore/RefPtrCairo.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using namespace WebKit;

static void didChangeIconForPageURL(WKIconDatabaseRef iconDatabase, WKURLRef pageURL, const void* clientInfo);
static void iconDataReadyForPageURL(WKIconDatabaseRef iconDatabase, WKURLRef pageURL, const void* clientInfo);

_Ewk_Favicon_Database::_Ewk_Favicon_Database(WKIconDatabaseRef iconDatabaseRef)
    :  wkIconDatabase(iconDatabaseRef)
{
    WKIconDatabaseClient iconDatabaseClient;
    memset(&iconDatabaseClient, 0, sizeof(WKIconDatabaseClient));
    iconDatabaseClient.version = kWKIconDatabaseClientCurrentVersion;
    iconDatabaseClient.clientInfo = this;
    iconDatabaseClient.didChangeIconForPageURL = didChangeIconForPageURL;
    iconDatabaseClient.iconDataReadyForPageURL = iconDataReadyForPageURL;
    WKIconDatabaseSetIconDatabaseClient(wkIconDatabase.get(), &iconDatabaseClient);
}

static void didChangeIconForPageURL(WKIconDatabaseRef, WKURLRef pageURLRef, const void* clientInfo)
{
    const Ewk_Favicon_Database* ewkIconDatabase = static_cast<const Ewk_Favicon_Database*>(clientInfo);

    if (ewkIconDatabase->changeListeners.isEmpty())
        return;

    CString pageURL = toImpl(pageURLRef)->string().utf8();

    ChangeListenerMap::const_iterator it = ewkIconDatabase->changeListeners.begin();
    ChangeListenerMap::const_iterator end = ewkIconDatabase->changeListeners.end();
    for (; it != end; ++it)
        it->value.callback(pageURL.data(), it->value.userData);
}

static cairo_surface_t* getIconSurfaceSynchronously(WebIconDatabase* webIconDatabase, const String& pageURL)
{
    webIconDatabase->retainIconForPageURL(pageURL);

    WebCore::NativeImagePtr icon = webIconDatabase->nativeImageForPageURL(pageURL);
    if (!icon) {
        webIconDatabase->releaseIconForPageURL(pageURL);
        return 0;
    }

    return icon->surface();
}

static void iconDataReadyForPageURL(WKIconDatabaseRef, WKURLRef pageURL, const void* clientInfo)
{
    Ewk_Favicon_Database* ewkIconDatabase = const_cast<Ewk_Favicon_Database*>(static_cast<const Ewk_Favicon_Database*>(clientInfo));

    String urlString = toImpl(pageURL)->string();
    if (!ewkIconDatabase->iconRequests.contains(urlString))
        return;

    WebIconDatabase* webIconDatabase = toImpl(ewkIconDatabase->wkIconDatabase.get());
    RefPtr<cairo_surface_t> surface = getIconSurfaceSynchronously(webIconDatabase, urlString);

    PendingIconRequestVector requestsForURL = ewkIconDatabase->iconRequests.take(urlString);
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

    String iconURL;
    toImpl(ewkIconDatabase->wkIconDatabase.get())->synchronousIconURLForPageURL(pageURL, iconURL);

    return eina_stringshare_add(iconURL.utf8().data());
}

struct AsyncIconRequestResponse {
    String pageURL;
    RefPtr<cairo_surface_t> surface;
    IconRequestCallbackData callbackData;

    AsyncIconRequestResponse(const String& _pageURL, PassRefPtr<cairo_surface_t> _surface, const IconRequestCallbackData& _callbackData)
        : pageURL(_pageURL)
        , surface(_surface)
        , callbackData(_callbackData)
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

Eina_Bool ewk_favicon_database_async_icon_get(Ewk_Favicon_Database* ewkIconDatabase, const char* page_url, Evas* evas, Ewk_Favicon_Database_Async_Icon_Get_Cb callback, void* userData)
{
    EINA_SAFETY_ON_NULL_RETURN_VAL(ewkIconDatabase, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(page_url, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(evas, false);
    EINA_SAFETY_ON_NULL_RETURN_VAL(callback, false);

    WebIconDatabase* webIconDatabase = toImpl(ewkIconDatabase->wkIconDatabase.get());

    // We ask for the icon directly. If we don't get the icon data now,
    // we'll be notified later (even if the database is still importing icons).
    RefPtr<cairo_surface_t> surface = getIconSurfaceSynchronously(webIconDatabase, page_url);

    // If there's no valid icon, but there's an iconURL registered,
    // or it's still not registered but the import process hasn't
    // finished yet, we need to wait for iconDataReadyForPageURL to be
    // called before making and informed decision.
    String iconURLForPageURL;
    webIconDatabase->synchronousIconURLForPageURL(page_url, iconURLForPageURL);
    if (!surface && (!iconURLForPageURL.isEmpty() || !webIconDatabase->isUrlImportCompleted())) {
        PendingIconRequestVector requests = ewkIconDatabase->iconRequests.get(page_url);
        requests.append(IconRequestCallbackData(callback, userData, evas));
        ewkIconDatabase->iconRequests.set(page_url, requests);
        return true;
    }

    // Respond when idle.
    AsyncIconRequestResponse* response = new AsyncIconRequestResponse(page_url, surface.release(), IconRequestCallbackData(callback, userData, evas));
    ecore_idler_add(respond_icon_request_idle, response);

    return true;
}

void ewk_favicon_database_icon_change_callback_add(Ewk_Favicon_Database* ewkIconDatabase, Ewk_Favicon_Database_Icon_Change_Cb callback, void* userData)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkIconDatabase);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    if (ewkIconDatabase->changeListeners.contains(callback))
        return;

    ewkIconDatabase->changeListeners.add(callback, IconChangeCallbackData(callback, userData));
}

void ewk_favicon_database_icon_change_callback_del(Ewk_Favicon_Database* ewkIconDatabase, Ewk_Favicon_Database_Icon_Change_Cb callback)
{
    EINA_SAFETY_ON_NULL_RETURN(ewkIconDatabase);
    EINA_SAFETY_ON_NULL_RETURN(callback);

    ewkIconDatabase->changeListeners.remove(callback);
}
