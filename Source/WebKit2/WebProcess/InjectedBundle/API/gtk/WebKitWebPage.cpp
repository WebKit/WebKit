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

#include "APIArray.h"
#include "APIDictionary.h"
#include "ImageOptions.h"
#include "InjectedBundle.h"
#include "WKBundleAPICast.h"
#include "WKBundleFrame.h"
#include "WebContextMenuItem.h"
#include "WebImage.h"
#include "WebKitConsoleMessagePrivate.h"
#include "WebKitContextMenuPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitFramePrivate.h"
#include "WebKitMarshal.h"
#include "WebKitPrivate.h"
#include "WebKitScriptWorldPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebEditorPrivate.h"
#include "WebKitWebHitTestResultPrivate.h"
#include "WebKitWebPagePrivate.h"
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserver.h>
#include <WebCore/FrameView.h>
#include <WebCore/MainFrame.h>
#include <glib/gi18n-lib.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebKit;
using namespace WebCore;

enum {
    DOCUMENT_LOADED,
    SEND_REQUEST,
    CONTEXT_MENU,
    CONSOLE_MESSAGE_SENT,

    LAST_SIGNAL
};

enum {
    PROP_0,

    PROP_URI
};

struct _WebKitWebPagePrivate {
    WebPage* webPage;

    CString uri;

    GRefPtr<WebKitWebEditor> webEditor;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitWebPage, webkit_web_page, G_TYPE_OBJECT)

static void webFrameDestroyed(WebFrame*);

class WebKitFrameWrapper final: public FrameDestructionObserver {
public:
    WebKitFrameWrapper(WebFrame& webFrame)
        : FrameDestructionObserver(webFrame.coreFrame())
        , m_webkitFrame(adoptGRef(webkitFrameCreate(&webFrame)))
    {
    }

    WebKitFrame* webkitFrame() const { return m_webkitFrame.get(); }

private:
    virtual void frameDestroyed() override
    {
        FrameDestructionObserver::frameDestroyed();
        webFrameDestroyed(webkitFrameGetWebFrame(m_webkitFrame.get()));
    }

    GRefPtr<WebKitFrame> m_webkitFrame;
};

typedef HashMap<WebFrame*, std::unique_ptr<WebKitFrameWrapper>> WebFrameMap;

static WebFrameMap& webFrameMap()
{
    static NeverDestroyed<WebFrameMap> map;
    return map;
}

static WebKitFrame* webkitFrameGetOrCreate(WebFrame* webFrame)
{
    auto wrapperPtr = webFrameMap().get(webFrame);
    if (wrapperPtr)
        return wrapperPtr->webkitFrame();

    std::unique_ptr<WebKitFrameWrapper> wrapper = std::make_unique<WebKitFrameWrapper>(*webFrame);
    wrapperPtr = wrapper.get();
    webFrameMap().set(webFrame, WTFMove(wrapper));
    return wrapperPtr->webkitFrame();
}

static void webFrameDestroyed(WebFrame* webFrame)
{
    webFrameMap().remove(webFrame);
}

static CString getProvisionalURLForFrame(WebFrame* webFrame)
{
    DocumentLoader* documentLoader = webFrame->coreFrame()->loader().provisionalDocumentLoader();
    if (!documentLoader->unreachableURL().isEmpty())
        return documentLoader->unreachableURL().string().utf8();

    return documentLoader->url().string().utf8();
}

static void webkitWebPageSetURI(WebKitWebPage* webPage, const CString& uri)
{
    if (webPage->priv->uri == uri)
        return;

    webPage->priv->uri = uri;
    g_object_notify(G_OBJECT(webPage), "uri");
}

static void webkitWebPageDidSendConsoleMessage(WebKitWebPage* webPage, MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
{
    WebKitConsoleMessage consoleMessage(source, level, message, lineNumber, sourceID);
    g_signal_emit(webPage, signals[CONSOLE_MESSAGE_SENT], 0, &consoleMessage);
}

static void didStartProvisionalLoadForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    webkitWebPageSetURI(WEBKIT_WEB_PAGE(clientInfo), getProvisionalURLForFrame(toImpl(frame)));
}

static void didReceiveServerRedirectForProvisionalLoadForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKTypeRef* /* userData */, const void *clientInfo)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    webkitWebPageSetURI(WEBKIT_WEB_PAGE(clientInfo), getProvisionalURLForFrame(toImpl(frame)));
}

static void didSameDocumentNavigationForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKSameDocumentNavigationType, WKTypeRef* /* userData */, const void *clientInfo)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    webkitWebPageSetURI(WEBKIT_WEB_PAGE(clientInfo), toImpl(frame)->coreFrame()->document()->url().string().utf8());
}

static void didFinishDocumentLoadForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKTypeRef*, const void *clientInfo)
{
    if (!WKBundleFrameIsMainFrame(frame))
        return;

    g_signal_emit(WEBKIT_WEB_PAGE(clientInfo), signals[DOCUMENT_LOADED], 0);
}

static void didClearWindowObjectForFrame(WKBundlePageRef, WKBundleFrameRef frame, WKBundleScriptWorldRef wkWorld, const void* clientInfo)
{
    if (WebKitScriptWorld* world = webkitScriptWorldGet(toImpl(wkWorld)))
        webkitScriptWorldWindowObjectCleared(world, WEBKIT_WEB_PAGE(clientInfo), webkitFrameGetOrCreate(toImpl(frame)));
}

static void didInitiateLoadForResource(WKBundlePageRef page, WKBundleFrameRef frame, uint64_t identifier, WKURLRequestRef request, bool /* pageLoadIsProvisional */, const void*)
{
    API::Dictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Frame"), toImpl(frame));
    message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
    message.set(String::fromUTF8("Request"), toImpl(request));
    WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidInitiateLoadForResource"), API::Dictionary::create(WTFMove(message)).ptr());
}

static WKURLRequestRef willSendRequestForFrame(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKURLRequestRef wkRequest, WKURLResponseRef wkRedirectResponse, const void* clientInfo)
{
    GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(toImpl(wkRequest)->resourceRequest()));
    const ResourceResponse& redirectResourceResponse = toImpl(wkRedirectResponse)->resourceResponse();
    GRefPtr<WebKitURIResponse> redirectResponse = !redirectResourceResponse.isNull() ? adoptGRef(webkitURIResponseCreateForResourceResponse(redirectResourceResponse)) : nullptr;

    gboolean returnValue;
    g_signal_emit(WEBKIT_WEB_PAGE(clientInfo), signals[SEND_REQUEST], 0, request.get(), redirectResponse.get(), &returnValue);
    if (returnValue)
        return 0;

    ResourceRequest resourceRequest;
    webkitURIRequestGetResourceRequest(request.get(), resourceRequest);
    resourceRequest.setInitiatingPageID(toImpl(page)->pageID());
    Ref<API::URLRequest> newRequest = API::URLRequest::create(resourceRequest);

    API::Dictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
    message.set(String::fromUTF8("Request"), newRequest.ptr());
    if (!redirectResourceResponse.isNull())
        message.set(String::fromUTF8("RedirectResponse"), toImpl(wkRedirectResponse));
    WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidSendRequestForResource"), API::Dictionary::create(WTFMove(message)).ptr());

    return toAPI(&newRequest.leakRef());
}

static void didReceiveResponseForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKURLResponseRef response, const void* clientInfo)
{
    API::Dictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
    message.set(String::fromUTF8("Response"), toImpl(response));
    WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidReceiveResponseForResource"), API::Dictionary::create(WTFMove(message)).ptr());

    // Post on the console as well to be consistent with the inspector.
    const ResourceResponse& resourceResponse = toImpl(response)->resourceResponse();
    if (resourceResponse.httpStatusCode() >= 400) {
        StringBuilder errorMessage;
        errorMessage.appendLiteral("Failed to load resource: the server responded with a status of ");
        errorMessage.appendNumber(resourceResponse.httpStatusCode());
        errorMessage.appendLiteral(" (");
        errorMessage.append(resourceResponse.httpStatusText());
        errorMessage.append(')');
        webkitWebPageDidSendConsoleMessage(WEBKIT_WEB_PAGE(clientInfo), MessageSource::Network, MessageLevel::Error, errorMessage.toString(), 0, resourceResponse.url().string());
    }
}

static void didReceiveContentLengthForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, uint64_t length, const void*)
{
    API::Dictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
    message.set(String::fromUTF8("ContentLength"), API::UInt64::create(length));
    WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidReceiveContentLengthForResource"), API::Dictionary::create(WTFMove(message)).ptr());
}

static void didFinishLoadForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, const void*)
{
    API::Dictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
    WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidFinishLoadForResource"), API::Dictionary::create(WTFMove(message)).ptr());
}

static void didFailLoadForResource(WKBundlePageRef page, WKBundleFrameRef, uint64_t identifier, WKErrorRef error, const void* clientInfo)
{
    API::Dictionary::MapType message;
    message.set(String::fromUTF8("Page"), toImpl(page));
    message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
    message.set(String::fromUTF8("Error"), toImpl(error));
    WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidFailLoadForResource"), API::Dictionary::create(WTFMove(message)).ptr());

    // Post on the console as well to be consistent with the inspector.
    const ResourceError& resourceError = toImpl(error)->platformError();
    if (!resourceError.isCancellation()) {
        StringBuilder errorMessage;
        errorMessage.appendLiteral("Failed to load resource");
        if (!resourceError.localizedDescription().isEmpty()) {
            errorMessage.appendLiteral(": ");
            errorMessage.append(resourceError.localizedDescription());
        }
        webkitWebPageDidSendConsoleMessage(WEBKIT_WEB_PAGE(clientInfo), MessageSource::Network, MessageLevel::Error, errorMessage.toString(), 0, resourceError.failingURL());
    }
}

class PageContextMenuClient final : public API::InjectedBundle::PageContextMenuClient {
public:
    explicit PageContextMenuClient(WebKitWebPage* webPage)
        : m_webPage(webPage)
    {
    }

private:
    bool getCustomMenuFromDefaultItems(WebPage&, const WebCore::HitTestResult& hitTestResult, const Vector<WebCore::ContextMenuItem>& defaultMenu, Vector<WebContextMenuItemData>& newMenu, RefPtr<API::Object>& userData) override
    {
        GRefPtr<WebKitContextMenu> contextMenu = adoptGRef(webkitContextMenuCreate(kitItems(defaultMenu)));
        GRefPtr<WebKitWebHitTestResult> webHitTestResult = adoptGRef(webkitWebHitTestResultCreate(hitTestResult));
        gboolean returnValue;
        g_signal_emit(m_webPage, signals[CONTEXT_MENU], 0, contextMenu.get(), webHitTestResult.get(), &returnValue);
        if (GVariant* variant = webkit_context_menu_get_user_data(contextMenu.get())) {
            GUniquePtr<gchar> dataString(g_variant_print(variant, TRUE));
            userData = API::String::create(String::fromUTF8(dataString.get()));
        }

        if (!returnValue)
            return false;

        webkitContextMenuPopulate(contextMenu.get(), newMenu);
        return true;
    }

    WebKitWebPage* m_webPage;
};

class PageUIClient final : public API::InjectedBundle::PageUIClient {
public:
    explicit PageUIClient(WebKitWebPage* webPage)
        : m_webPage(webPage)
    {
    }

private:
    void willAddMessageToConsole(WebPage*, MessageSource source, MessageLevel level, const String& message, unsigned lineNumber, unsigned /*columnNumber*/, const String& sourceID) override
    {
        webkitWebPageDidSendConsoleMessage(m_webPage, source, level, message, lineNumber, sourceID);
    }

    WebKitWebPage* m_webPage;
};

static void webkitWebPageGetProperty(GObject* object, guint propId, GValue* value, GParamSpec* paramSpec)
{
    WebKitWebPage* webPage = WEBKIT_WEB_PAGE(object);

    switch (propId) {
    case PROP_URI:
        g_value_set_string(value, webkit_web_page_get_uri(webPage));
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propId, paramSpec);
    }
}

static void webkit_web_page_class_init(WebKitWebPageClass* klass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(klass);

    gObjectClass->get_property = webkitWebPageGetProperty;

    /**
     * WebKitWebPage:uri:
     *
     * The current active URI of the #WebKitWebPage.
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_URI,
        g_param_spec_string(
            "uri",
            _("URI"),
            _("The current active URI of the web page"),
            0,
            WEBKIT_PARAM_READABLE));

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
        g_cclosure_marshal_VOID__VOID,
        G_TYPE_NONE, 0);

    /**
     * WebKitWebPage::send-request:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @request: a #WebKitURIRequest
     * @redirected_response: a #WebKitURIResponse, or %NULL
     *
     * This signal is emitted when @request is about to be sent to
     * the server. This signal can be used to modify the #WebKitURIRequest
     * that will be sent to the server. You can also cancel the resource load
     * operation by connecting to this signal and returning %TRUE.
     *
     * In case of a server redirection this signal is
     * emitted again with the @request argument containing the new
     * request to be sent to the server due to the redirection and the
     * @redirected_response parameter containing the response
     * received by the server for the initial request.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to continue emission of the event.
     */
    signals[SEND_REQUEST] = g_signal_new(
        "send-request",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, 0,
        webkit_marshal_BOOLEAN__OBJECT_OBJECT,
        G_TYPE_BOOLEAN, 2,
        WEBKIT_TYPE_URI_REQUEST,
        WEBKIT_TYPE_URI_RESPONSE);

    /**
     * WebKitWebPage::context-menu:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @context_menu: the proposed #WebKitContextMenu
     * @hit_test_result: a #WebKitWebHitTestResult
     *
     * Emmited before a context menu is displayed in the UI Process to
     * give the application a chance to customize the proposed menu,
     * build its own context menu or pass user data to the UI Process.
     * This signal is useful when the information available in the UI Process
     * is not enough to build or customize the context menu, for example, to
     * add menu entries depending on the #WebKitDOMNode at the coordinates of the
     * @hit_test_result. Otherwise, it's recommened to use #WebKitWebView::context-menu
     * signal instead.
     *
     * Returns: %TRUE if the proposed @context_menu has been modified, or %FALSE otherwise.
     *
     * Since: 2.8
     */
    signals[CONTEXT_MENU] = g_signal_new(
        "context-menu",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, 0,
        webkit_marshal_BOOLEAN__OBJECT_OBJECT,
        G_TYPE_BOOLEAN, 2,
        WEBKIT_TYPE_CONTEXT_MENU,
        WEBKIT_TYPE_WEB_HIT_TEST_RESULT);

    /**
     * WebKitWebPage::console-message-sent:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @console_message: the #WebKitConsoleMessage
     *
     * Emmited when a message is sent to the console. This can be a message
     * produced by the use of JavaScript console API, a javascript exception,
     * a security error or other errors, warnings, debug or log messages.
     * The @console_message contains information of the message.
     *
     * Since: 2.12
     */
    signals[CONSOLE_MESSAGE_SENT] = g_signal_new(
        "console-message-sent",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, nullptr,
        g_cclosure_marshal_VOID__BOXED,
        G_TYPE_NONE, 1,
        WEBKIT_TYPE_CONSOLE_MESSAGE | G_SIGNAL_TYPE_STATIC_SCOPE);
}

WebPage* webkitWebPageGetPage(WebKitWebPage *webPage)
{
    return webPage->priv->webPage;
}

WebKitWebPage* webkitWebPageCreate(WebPage* webPage)
{
    WebKitWebPage* page = WEBKIT_WEB_PAGE(g_object_new(WEBKIT_TYPE_WEB_PAGE, NULL));
    page->priv->webPage = webPage;

    WKBundlePageLoaderClientV6 loaderClient = {
        {
            6, // version
            page, // clientInfo
        },
        didStartProvisionalLoadForFrame,
        didReceiveServerRedirectForProvisionalLoadForFrame,
        0, // didFailProvisionalLoadWithErrorForFrame
        0, // didCommitLoadForFrame
        didFinishDocumentLoadForFrame,
        0, // didFinishLoadForFrame
        0, // didFailLoadWithErrorForFrame
        didSameDocumentNavigationForFrame,
        0, // didReceiveTitleForFrame
        0, // didFirstLayoutForFrame
        0, // didFirstVisuallyNonEmptyLayoutForFrame
        0, // didRemoveFrameFromHierarchy,
        0, // didDisplayInsecureContentForFrame
        0, // didRunInsecureContentForFrame
        didClearWindowObjectForFrame,
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
        0, // didLayout
        0, // featuresUsedInPage
        0, // willLoadURLRequest
        0, // willLoadDataRequest
    };
    WKBundlePageSetPageLoaderClient(toAPI(webPage), &loaderClient.base);

    WKBundlePageResourceLoadClientV1 resourceLoadClient = {
        {
            1, // version
            page, // clientInfo
        },
        didInitiateLoadForResource,
        willSendRequestForFrame,
        didReceiveResponseForResource,
        didReceiveContentLengthForResource,
        didFinishLoadForResource,
        didFailLoadForResource,
        0, // shouldCacheResponse
        0 // shouldUseCredentialStorage
    };
    WKBundlePageSetResourceLoadClient(toAPI(webPage), &resourceLoadClient.base);

    webPage->setInjectedBundleContextMenuClient(std::make_unique<PageContextMenuClient>(page));
    webPage->setInjectedBundleUIClient(std::make_unique<PageUIClient>(page));

    return page;
}

void webkitWebPageDidReceiveMessage(WebKitWebPage* page, const String& messageName, API::Dictionary& message)
{
    if (messageName == String("GetSnapshot")) {
        SnapshotOptions snapshotOptions = static_cast<SnapshotOptions>(static_cast<API::UInt64*>(message.get("SnapshotOptions"))->value());
        uint64_t callbackID = static_cast<API::UInt64*>(message.get("CallbackID"))->value();
        SnapshotRegion region = static_cast<SnapshotRegion>(static_cast<API::UInt64*>(message.get("SnapshotRegion"))->value());
        bool transparentBackground = static_cast<API::Boolean*>(message.get("TransparentBackground"))->value();

        RefPtr<WebImage> snapshotImage;
        WebPage* webPage = page->priv->webPage;
        if (WebCore::FrameView* frameView = webPage->mainFrameView()) {
            WebCore::IntRect snapshotRect;
            switch (region) {
            case SnapshotRegionVisible:
                snapshotRect = frameView->visibleContentRect();
                break;
            case SnapshotRegionFullDocument:
                snapshotRect = WebCore::IntRect(WebCore::IntPoint(0, 0), frameView->contentsSize());
                break;
            default:
                ASSERT_NOT_REACHED();
            }
            if (!snapshotRect.isEmpty()) {
                Color savedBackgroundColor;
                if (transparentBackground) {
                    savedBackgroundColor = frameView->baseBackgroundColor();
                    frameView->setBaseBackgroundColor(Color::transparent);
                }
                snapshotImage = webPage->scaledSnapshotWithOptions(snapshotRect, 1, snapshotOptions | SnapshotOptionsShareable);
                if (transparentBackground)
                    frameView->setBaseBackgroundColor(savedBackgroundColor);
            }
        }

        API::Dictionary::MapType messageReply;
        messageReply.set("Page", webPage);
        messageReply.set("CallbackID", API::UInt64::create(callbackID));
        messageReply.set("Snapshot", snapshotImage);
        WebProcess::singleton().injectedBundle()->postMessage("WebPage.DidGetSnapshot", API::Dictionary::create(WTFMove(messageReply)).ptr());
    } else
        ASSERT_NOT_REACHED();
}

/**
 * webkit_web_page_get_dom_document:
 * @web_page: a #WebKitWebPage
 *
 * Get the #WebKitDOMDocument currently loaded in @web_page
 *
 * Returns: (transfer none): the #WebKitDOMDocument currently loaded, or %NULL
 *    if no document is currently loaded.
 */
WebKitDOMDocument* webkit_web_page_get_dom_document(WebKitWebPage* webPage)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), 0);

    MainFrame* coreFrame = webPage->priv->webPage->mainFrame();
    if (!coreFrame)
        return 0;

    return kit(coreFrame->document());
}

/**
 * webkit_web_page_get_id:
 * @web_page: a #WebKitWebPage
 *
 * Get the identifier of the #WebKitWebPage
 *
 * Returns: the identifier of @web_page
 */
guint64 webkit_web_page_get_id(WebKitWebPage* webPage)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), 0);

    return webPage->priv->webPage->pageID();
}

/**
 * webkit_web_page_get_uri:
 * @web_page: a #WebKitWebPage
 *
 * Returns the current active URI of @web_page.
 *
 * You can monitor the active URI by connecting to the notify::uri
 * signal of @web_page.
 *
 * Returns: the current active URI of @web_view or %NULL if nothing has been
 *    loaded yet.
 */
const gchar* webkit_web_page_get_uri(WebKitWebPage* webPage)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), 0);

    return webPage->priv->uri.data();
}

/**
 * webkit_web_page_get_main_frame:
 * @web_page: a #WebKitWebPage
 *
 * Returns the main frame of a #WebKitWebPage.
 *
 * Returns: (transfer none): the #WebKitFrame that is the main frame of @web_page
 *
 * Since: 2.2
 */
WebKitFrame* webkit_web_page_get_main_frame(WebKitWebPage* webPage)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), 0);

    return webkitFrameGetOrCreate(webPage->priv->webPage->mainWebFrame());
}

/**
 * webkit_web_page_get_editor:
 * @web_page: a #WebKitWebPage
 *
 * Gets the #WebKitWebEditor of a #WebKitWebPage.
 *
 * Returns: (transfer none): the #WebKitWebEditor
 *
 * Since: 2.10
 */
WebKitWebEditor* webkit_web_page_get_editor(WebKitWebPage* webPage)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), nullptr);

    if (!webPage->priv->webEditor)
        webPage->priv->webEditor = adoptGRef(webkitWebEditorCreate(webPage));

    return webPage->priv->webEditor.get();
}
