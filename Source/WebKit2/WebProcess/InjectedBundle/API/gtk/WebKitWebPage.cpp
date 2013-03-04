/*
 * Copyright (C) 2012 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2,1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "WebKitWebPage.h"

#include "ImmutableDictionary.h"
#include "InjectedBundle.h"
#include "WKBundleAPICast.h"
#include "WebFrame.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitPrivate.h"
#include "WebKitWebPagePrivate.h"
#include "WebProcess.h"
#include <WebCore/Frame.h>

using namespace WebKit;
using namespace WebCore;

enum {
    DOCUMENT_LOADED,

    LAST_SIGNAL
};

struct _WebKitWebPagePrivate {
    WebPage* webPage;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitWebPage, webkit_web_page, G_TYPE_OBJECT)

static void didFinishDocumentLoadForFrame(WKBundlePageRef, WKBundleFrameRef, WKTypeRef*, const void *clientInfo)
{
    g_signal_emit(WEBKIT_WEB_PAGE(clientInfo), signals[DOCUMENT_LOADED], 0);
}

static void didInitiateLoadForResource(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, WKURLRequestRef request, bool pageLoadIsProvisional, const void*)
{
    ImmutableDictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Frame"), toImpl(frame));
    message.set(String::fromUTF8("Identifier"), WebUInt64::create(identifier));
    message.set(String::fromUTF8("Request"), toImpl(request));
    WebProcess::shared().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidInitiateLoadForResource"), ImmutableDictionary::adopt(message).get());
}

static WKURLRequestRef willSendRequestForFrame(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKURLRequestRef request, WKURLResponseRef redirectResponse, const void*)
{
    ImmutableDictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), WebUInt64::create(identifier));
    message.set(String::fromUTF8("Request"), toImpl(request));
    if (!toImpl(redirectResponse)->resourceResponse().isNull())
        message.set(String::fromUTF8("RedirectResponse"), toImpl(redirectResponse));
    WebProcess::shared().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidSendRequestForResource"), ImmutableDictionary::adopt(message).get());

    WKRetain(request);
    return request;
}

static void didReceiveResponseForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKURLResponseRef response, const void*)
{
    ImmutableDictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), WebUInt64::create(identifier));
    message.set(String::fromUTF8("Response"), toImpl(response));
    WebProcess::shared().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidReceiveResponseForResource"), ImmutableDictionary::adopt(message).get());
}

static void didReceiveContentLengthForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, uint64_t length, const void*)
{
    ImmutableDictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), WebUInt64::create(identifier));
    message.set(String::fromUTF8("ContentLength"), WebUInt64::create(length));
    WebProcess::shared().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidReceiveContentLengthForResource"), ImmutableDictionary::adopt(message).get());
}

static void didFinishLoadForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, const void*)
{
    ImmutableDictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), WebUInt64::create(identifier));
    WebProcess::shared().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidFinishLoadForResource"), ImmutableDictionary::adopt(message).get());
}

static void didFailLoadForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKErrorRef error, const void*)
{
    ImmutableDictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), WebUInt64::create(identifier));
    message.set(String::fromUTF8("Error"), toImpl(error));
    WebProcess::shared().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidFailLoadForResource"), ImmutableDictionary::adopt(message).get());
}

static void webkit_web_page_class_init(WebKitWebPageClass* klass)
{
    /**
     * WebKitWebPage::document-loaded:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     *
     * This signal is emitted when the DOM document of a #WebKitWebPage has been
     * loaded.
     *
     * You can wait for this signal to get the DOM document with
     * webkit_web_page_get_dom_document().
     */
    signals[DOCUMENT_LOADED] = g_signal_new(
        "document-loaded",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, 0,
        g_cclosure_marshal_VOID__OBJECT,
        G_TYPE_NONE, 0);
}

WebKitWebPage* webkitWebPageCreate(WebPage* webPage)
{
    WebKitWebPage* page = WEBKIT_WEB_PAGE(g_object_new(WEBKIT_TYPE_WEB_PAGE, NULL));
    page->priv->webPage = webPage;

    WKBundlePageLoaderClient loaderClient = {
        kWKBundlePageResourceLoadClientCurrentVersion,
        page,
        0, // didStartProvisionalLoadForFrame
        0, // didReceiveServerRedirectForProvisionalLoadForFrame
        0, // didFailProvisionalLoadWithErrorForFrame
        0, // didCommitLoadForFrame
        didFinishDocumentLoadForFrame,
        0, // didFinishLoadForFrame,
        0, // didFailLoadWithErrorForFrame
        0, // didSameDocumentNavigationForFrame
        0, // didReceiveTitleForFrame
        0, // didFirstLayoutForFrame
        0, // didFirstVisuallyNonEmptyLayoutForFrame
        0, // didRemoveFrameFromHierarchy
        0, // didDisplayInsecureContentForFrame
        0, // didRunInsecureContentForFrame
        0, // didClearWindowObjectForFrame
        0, // didCancelClientRedirectForFrame
        0, // willPerformClientRedirectForFrame
        0, // didHandleOnloadEventsForFrame
        0, // didLayoutForFrame
        0, // didNewFirstVisuallyNonEmptyLayout
        0, // didDetectXSSForFrame
        0, // shouldGoToBackForwardListItem
        0, // globalObjectIsAvailableForFrame
        0, // willDisconnectDOMWindowExtensionFromGlobalObject
        0, // didReconnectDOMWindowExtensionToGlobalObject
        0, // willDestroyGlobalObjectForDOMWindowExtension
        0, // didFinishProgress
        0, // shouldForceUniversalAccessFromLocalURL
        0, // didReceiveIntentForFrame_unavailable
        0, // registerIntentServiceForFrame_unavailable
        0 // didLayout
    };
    WKBundlePageSetPageLoaderClient(toAPI(webPage), &loaderClient);

    WKBundlePageResourceLoadClient resourceLoadClient = {
        kWKBundlePageResourceLoadClientCurrentVersion,
        page,
        didInitiateLoadForResource,
        willSendRequestForFrame,
        didReceiveResponseForResource,
        didReceiveContentLengthForResource,
        didFinishLoadForResource,
        didFailLoadForResource,
        0, // shouldCacheResponse
        0 // shouldUseCredentialStorage
    };
    WKBundlePageSetResourceLoadClient(toAPI(webPage), &resourceLoadClient);

    return page;
}

/**
 * webkit_web_page_get_dom_document:
 * @web_page: a #WebKitWebPage
 *
 * Get the #WebKitDOMDocument currently loaded in @web_page
 *
 * Returns: the #WebKitDOMDocument currently loaded, or %NULL
 *    if no document is currently loaded.
 */
WebKitDOMDocument* webkit_web_page_get_dom_document(WebKitWebPage* webPage)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), 0);

    Frame* coreFrame = webPage->priv->webPage->mainFrame();
    if (!coreFrame)
        return 0;

    return kit(coreFrame->document());
}
