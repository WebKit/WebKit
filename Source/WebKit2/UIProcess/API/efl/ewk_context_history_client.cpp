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
#include "ewk_context.h"

#include "DownloadProxy.h"
#include "WKAPICast.h"
#include "WKContext.h"
#include "WKEinaSharedString.h"
#include "WKString.h"
#include "ewk_context_history_client_private.h"
#include "ewk_context_private.h"
#include "ewk_navigation_data.h"
#include "ewk_navigation_data_private.h"
#include "ewk_url_response.h"
#include "ewk_url_response_private.h"
#include "ewk_view_private.h"

using namespace WebKit;

static inline const Ewk_Context_History_Client& getEwkHistoryClient(const void* clientInfo)
{
    ASSERT(clientInfo);
    return static_cast<const Ewk_Context*>(clientInfo)->historyClient();
}

static void didNavigateWithNavigationData(WKContextRef, WKPageRef page, WKNavigationDataRef navigationData, WKFrameRef, const void* clientInfo)
{
    const Ewk_Context_History_Client& historyClient = getEwkHistoryClient(clientInfo);

    if (!historyClient.navigate_func)
        return;

    RefPtr<Ewk_Navigation_Data> navigationDataEwk = Ewk_Navigation_Data::create(navigationData);
    historyClient.navigate_func(ewk_view_from_page_get(toImpl(page)), navigationDataEwk.get(), historyClient.user_data);
}

static void didPerformClientRedirect(WKContextRef, WKPageRef page, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef, const void* clientInfo)
{
    const Ewk_Context_History_Client& historyClient = getEwkHistoryClient(clientInfo);

    if (!historyClient.client_redirect_func)
        return;

    WKEinaSharedString sourceURLString(sourceURL);
    WKEinaSharedString destinationURLString(destinationURL);

    historyClient.client_redirect_func(ewk_view_from_page_get(toImpl(page)), sourceURLString, destinationURLString, historyClient.user_data);
}

static void didPerformServerRedirect(WKContextRef, WKPageRef page, WKURLRef sourceURL, WKURLRef destinationURL, WKFrameRef, const void* clientInfo)
{
    const Ewk_Context_History_Client& historyClient = getEwkHistoryClient(clientInfo);

    if (!historyClient.server_redirect_func)
        return;

    WKEinaSharedString sourceURLString(sourceURL);
    WKEinaSharedString destinationURLString(destinationURL);

    historyClient.server_redirect_func(ewk_view_from_page_get(toImpl(page)), sourceURLString, destinationURLString, historyClient.user_data);
}

static void didUpdateHistoryTitle(WKContextRef, WKPageRef page, WKStringRef title, WKURLRef URL, WKFrameRef, const void* clientInfo)
{
    const Ewk_Context_History_Client& historyClient = getEwkHistoryClient(clientInfo);

    if (!historyClient.title_update_func)
        return;

    WKEinaSharedString titleString(title);
    WKEinaSharedString stringURL(URL);

    historyClient.title_update_func(ewk_view_from_page_get(toImpl(page)), titleString, stringURL, historyClient.user_data);
}

static void populateVisitedLinks(WKContextRef, const void* clientInfo)
{
    const Ewk_Context_History_Client& historyClient = getEwkHistoryClient(clientInfo);

    if (!historyClient.populate_visited_links_func)
        return;

    historyClient.populate_visited_links_func(historyClient.user_data);
}

void ewk_context_history_client_attach(Ewk_Context* ewkContext)
{
    WKContextHistoryClient wkHistoryClient;
    memset(&wkHistoryClient, 0, sizeof(WKContextHistoryClient));

    wkHistoryClient.version = kWKContextHistoryClientCurrentVersion;
    wkHistoryClient.clientInfo = ewkContext;

    wkHistoryClient.didNavigateWithNavigationData = didNavigateWithNavigationData;
    wkHistoryClient.didPerformClientRedirect = didPerformClientRedirect;
    wkHistoryClient.didPerformServerRedirect = didPerformServerRedirect;
    wkHistoryClient.didUpdateHistoryTitle = didUpdateHistoryTitle;
    wkHistoryClient.populateVisitedLinks = populateVisitedLinks;

    WKContextSetHistoryClient(ewkContext->wkContext(), &wkHistoryClient);
}
