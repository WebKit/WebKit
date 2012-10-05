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
#include "ewk_view.h"

#include "WKFrame.h"
#include "ewk_back_forward_list_private.h"
#include "ewk_intent.h"
#include "ewk_intent_private.h"
#include "ewk_intent_service.h"
#include "ewk_intent_service_private.h"
#include "ewk_view_loader_client_private.h"
#include "ewk_view_private.h"
#include "ewk_web_error_private.h"
#include <wtf/text/CString.h>

using namespace WebKit;

static void didReceiveTitleForFrame(WKPageRef, WKStringRef title, WKFrameRef frame, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    ewk_view_title_changed(ewkView, toImpl(title)->string().utf8().data());
}

#if ENABLE(WEB_INTENTS)
static void didReceiveIntentForFrame(WKPageRef, WKFrameRef, WKIntentDataRef intent, WKTypeRef, const void* clientInfo)
{
    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    Ewk_Intent* ewkIntent = ewk_intent_new(intent);
    ewk_view_intent_request_new(ewkView, ewkIntent);
    ewk_intent_unref(ewkIntent);
}
#endif

#if ENABLE(WEB_INTENTS_TAG)
static void registerIntentServiceForFrame(WKPageRef, WKFrameRef, WKIntentServiceInfoRef serviceInfo, WKTypeRef, const void *clientInfo)
{
    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    Ewk_Intent_Service* ewkIntentService = ewk_intent_service_new(serviceInfo);
    ewk_view_intent_service_register(ewkView, ewkIntentService);
    ewk_intent_service_unref(ewkIntentService);
}
#endif

static void didChangeProgress(WKPageRef page, const void* clientInfo)
{
    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    ewk_view_load_progress_changed(ewkView, WKPageGetEstimatedProgress(page));
}

static void didFinishLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef /*userData*/, const void *clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    ewk_view_load_finished(ewkView);
}

static void didFailLoadWithErrorForFrame(WKPageRef, WKFrameRef frame, WKErrorRef error, WKTypeRef, const void *clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    Ewk_Web_Error* ewkError = ewk_web_error_new(error);
    ewk_view_load_error(ewkView, ewkError);
    ewk_view_load_finished(ewkView);
    ewk_web_error_free(ewkError);
}

static void didStartProvisionalLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef /*userData*/, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    ewk_view_load_provisional_started(ewkView);
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKPageRef, WKFrameRef frame, WKTypeRef /*userData*/, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    ewk_view_load_provisional_redirect(ewkView);
}

static void didFailProvisionalLoadWithErrorForFrame(WKPageRef, WKFrameRef frame, WKErrorRef error, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    Ewk_Web_Error* ewkError = ewk_web_error_new(error);
    ewk_view_load_provisional_failed(ewkView, ewkError);
    ewk_web_error_free(ewkError);
}

static void didChangeBackForwardList(WKPageRef, WKBackForwardListItemRef addedItem, WKArrayRef removedItems, const void* clientInfo)
{
    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    ASSERT(ewkView);

    ewk_back_forward_list_changed(ewk_view_back_forward_list_get(ewkView), addedItem, removedItems);
}

static void didSameDocumentNavigationForFrame(WKPageRef, WKFrameRef frame, WKSameDocumentNavigationType, WKTypeRef, const void* clientInfo)
{
    if (!WKFrameIsMainFrame(frame))
        return;

    Evas_Object* ewkView = static_cast<Evas_Object*>(const_cast<void*>(clientInfo));
    ewk_view_uri_update(ewkView);
}

void ewk_view_loader_client_attach(WKPageRef pageRef, Evas_Object* ewkView)
{
    WKPageLoaderClient loadClient;
    memset(&loadClient, 0, sizeof(WKPageLoaderClient));
    loadClient.version = kWKPageLoaderClientCurrentVersion;
    loadClient.clientInfo = ewkView;
    loadClient.didReceiveTitleForFrame = didReceiveTitleForFrame;
#if ENABLE(WEB_INTENTS)
    loadClient.didReceiveIntentForFrame = didReceiveIntentForFrame;
#endif
#if ENABLE(WEB_INTENTS_TAG)
    loadClient.registerIntentServiceForFrame = registerIntentServiceForFrame;
#endif
    loadClient.didStartProgress = didChangeProgress;
    loadClient.didChangeProgress = didChangeProgress;
    loadClient.didFinishProgress = didChangeProgress;
    loadClient.didFinishLoadForFrame = didFinishLoadForFrame;
    loadClient.didFailLoadWithErrorForFrame = didFailLoadWithErrorForFrame;
    loadClient.didStartProvisionalLoadForFrame = didStartProvisionalLoadForFrame;
    loadClient.didReceiveServerRedirectForProvisionalLoadForFrame = didReceiveServerRedirectForProvisionalLoadForFrame;
    loadClient.didFailProvisionalLoadWithErrorForFrame = didFailProvisionalLoadWithErrorForFrame;
    loadClient.didChangeBackForwardList = didChangeBackForwardList;
    loadClient.didSameDocumentNavigationForFrame = didSameDocumentNavigationForFrame;
    WKPageSetPageLoaderClient(pageRef, &loadClient);
}
