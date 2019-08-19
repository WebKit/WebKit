/*
 * Copyright (C) 2012, 2017 Igalia S.L.
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
#include "APIError.h"
#include "APINumber.h"
#include "APIString.h"
#include "APIURLRequest.h"
#include "APIURLResponse.h"
#include "ImageOptions.h"
#include "InjectedBundle.h"
#include "WebContextMenuItem.h"
#include "WebImage.h"
#include "WebKitConsoleMessagePrivate.h"
#include "WebKitContextMenuPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitFramePrivate.h"
#include "WebKitPrivate.h"
#include "WebKitScriptWorldPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitWebEditorPrivate.h"
#include "WebKitWebHitTestResultPrivate.h"
#include "WebKitWebPagePrivate.h"
#include "WebKitWebProcessEnumTypes.h"
#include "WebProcess.h"
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserver.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <glib/gi18n-lib.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringBuilder.h>

using namespace WebKit;
using namespace WebCore;

enum {
    DOCUMENT_LOADED,
    SEND_REQUEST,
    CONTEXT_MENU,
    CONSOLE_MESSAGE_SENT,
    FORM_CONTROLS_ASSOCIATED,
    FORM_CONTROLS_ASSOCIATED_FOR_FRAME,
    WILL_SUBMIT_FORM,

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
    WTF_MAKE_FAST_ALLOCATED;
public:
    WebKitFrameWrapper(WebFrame& webFrame)
        : FrameDestructionObserver(webFrame.coreFrame())
        , m_webkitFrame(adoptGRef(webkitFrameCreate(&webFrame)))
    {
    }

    WebKitFrame* webkitFrame() const { return m_webkitFrame.get(); }

private:
    void frameDestroyed() override
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

    std::unique_ptr<WebKitFrameWrapper> wrapper = makeUnique<WebKitFrameWrapper>(*webFrame);
    wrapperPtr = wrapper.get();
    webFrameMap().set(webFrame, WTFMove(wrapper));
    return wrapperPtr->webkitFrame();
}

static void webFrameDestroyed(WebFrame* webFrame)
{
    webFrameMap().remove(webFrame);
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

class PageLoaderClient final : public API::InjectedBundle::PageLoaderClient {
public:
    explicit PageLoaderClient(WebKitWebPage* webPage)
        : m_webPage(webPage)
    {
    }

private:
    static CString getDocumentLoaderURL(DocumentLoader* documentLoader)
    {
        ASSERT(documentLoader);
        if (!documentLoader->unreachableURL().isEmpty())
            return documentLoader->unreachableURL().string().utf8();

        return documentLoader->url().string().utf8();
    }

    void didStartProvisionalLoadForFrame(WebPage&, WebFrame& frame, RefPtr<API::Object>&) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebPageSetURI(m_webPage, getDocumentLoaderURL(frame.coreFrame()->loader().provisionalDocumentLoader()));
    }

    void didReceiveServerRedirectForProvisionalLoadForFrame(WebPage&, WebFrame& frame, RefPtr<API::Object>&) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebPageSetURI(m_webPage, getDocumentLoaderURL(frame.coreFrame()->loader().provisionalDocumentLoader()));
    }

    void didSameDocumentNavigationForFrame(WebPage&, WebFrame& frame, SameDocumentNavigationType, RefPtr<API::Object>&) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebPageSetURI(m_webPage, frame.coreFrame()->document()->url().string().utf8());
    }

    void didCommitLoadForFrame(WebPage&, WebFrame& frame, RefPtr<API::Object>&) override
    {
        if (!frame.isMainFrame())
            return;
        webkitWebPageSetURI(m_webPage, getDocumentLoaderURL(frame.coreFrame()->loader().documentLoader()));
    }

    void didFinishDocumentLoadForFrame(WebPage&, WebFrame& frame, RefPtr<API::Object>&) override
    {
        if (!frame.isMainFrame())
            return;
        g_signal_emit(m_webPage, signals[DOCUMENT_LOADED], 0);
    }

    void didClearWindowObjectForFrame(WebPage&, WebFrame& frame, DOMWrapperWorld& world) override
    {
        auto injectedWorld = InjectedBundleScriptWorld::getOrCreate(world);
        if (auto* wkWorld = webkitScriptWorldGet(injectedWorld.ptr()))
            webkitScriptWorldWindowObjectCleared(wkWorld, m_webPage, webkitFrameGetOrCreate(&frame));
    }

    WebKitWebPage* m_webPage;
};


class PageResourceLoadClient final : public API::InjectedBundle::ResourceLoadClient {
public:
    explicit PageResourceLoadClient(WebKitWebPage* webPage)
        : m_webPage(webPage)
    {
    }

private:
    void didInitiateLoadForResource(WebPage& page, WebFrame& frame, uint64_t identifier, const ResourceRequest& request, bool) override
    {
        API::Dictionary::MapType message;
        message.set(String::fromUTF8("Page"), &page);
        message.set(String::fromUTF8("Frame"), &frame);
        message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
        message.set(String::fromUTF8("Request"), API::URLRequest::create(request));
        WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidInitiateLoadForResource"), API::Dictionary::create(WTFMove(message)).ptr());
    }

    void willSendRequestForFrame(WebPage& page, WebFrame&, uint64_t identifier, ResourceRequest& resourceRequest, const ResourceResponse& redirectResourceResponse) override
    {
        GRefPtr<WebKitURIRequest> request = adoptGRef(webkitURIRequestCreateForResourceRequest(resourceRequest));
        GRefPtr<WebKitURIResponse> redirectResponse = !redirectResourceResponse.isNull() ? adoptGRef(webkitURIResponseCreateForResourceResponse(redirectResourceResponse)) : nullptr;

        gboolean returnValue;
        g_signal_emit(m_webPage, signals[SEND_REQUEST], 0, request.get(), redirectResponse.get(), &returnValue);
        if (returnValue) {
            resourceRequest = { };
            return;
        }

        webkitURIRequestGetResourceRequest(request.get(), resourceRequest);
        resourceRequest.setInitiatingPageID(page.pageID());

        API::Dictionary::MapType message;
        message.set(String::fromUTF8("Page"), &page);
        message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
        message.set(String::fromUTF8("Request"), API::URLRequest::create(resourceRequest));
        if (!redirectResourceResponse.isNull())
            message.set(String::fromUTF8("RedirectResponse"), API::URLResponse::create(redirectResourceResponse));
        WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidSendRequestForResource"), API::Dictionary::create(WTFMove(message)).ptr());
    }

    void didReceiveResponseForResource(WebPage& page, WebFrame&, uint64_t identifier, const ResourceResponse& response) override
    {
        API::Dictionary::MapType message;
        message.set(String::fromUTF8("Page"), &page);
        message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
        message.set(String::fromUTF8("Response"), API::URLResponse::create(response));
        WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidReceiveResponseForResource"), API::Dictionary::create(WTFMove(message)).ptr());

        // Post on the console as well to be consistent with the inspector.
        if (response.httpStatusCode() >= 400) {
            StringBuilder errorMessage;
            errorMessage.appendLiteral("Failed to load resource: the server responded with a status of ");
            errorMessage.appendNumber(response.httpStatusCode());
            errorMessage.appendLiteral(" (");
            errorMessage.append(response.httpStatusText());
            errorMessage.append(')');
            webkitWebPageDidSendConsoleMessage(m_webPage, MessageSource::Network, MessageLevel::Error, errorMessage.toString(), 0, response.url().string());
        }
    }

    void didReceiveContentLengthForResource(WebPage& page, WebFrame&, uint64_t identifier, uint64_t contentLength) override
    {
        API::Dictionary::MapType message;
        message.set(String::fromUTF8("Page"), &page);
        message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
        message.set(String::fromUTF8("ContentLength"), API::UInt64::create(contentLength));
        WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidReceiveContentLengthForResource"), API::Dictionary::create(WTFMove(message)).ptr());
    }

    void didFinishLoadForResource(WebPage& page, WebFrame&, uint64_t identifier) override
    {
        API::Dictionary::MapType message;
        message.set(String::fromUTF8("Page"), &page);
        message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
        WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidFinishLoadForResource"), API::Dictionary::create(WTFMove(message)).ptr());
    }

    void didFailLoadForResource(WebPage& page, WebFrame&, uint64_t identifier, const ResourceError& error) override
    {
        API::Dictionary::MapType message;
        message.set(String::fromUTF8("Page"), &page);
        message.set(String::fromUTF8("Identifier"), API::UInt64::create(identifier));
        message.set(String::fromUTF8("Error"), API::Error::create(error));
        WebProcess::singleton().injectedBundle()->postMessage(String::fromUTF8("WebPage.DidFailLoadForResource"), API::Dictionary::create(WTFMove(message)).ptr());

        // Post on the console as well to be consistent with the inspector.
        if (!error.isCancellation()) {
            StringBuilder errorMessage;
            errorMessage.appendLiteral("Failed to load resource");
            if (!error.localizedDescription().isEmpty()) {
                errorMessage.appendLiteral(": ");
                errorMessage.append(error.localizedDescription());
            }
            webkitWebPageDidSendConsoleMessage(m_webPage, MessageSource::Network, MessageLevel::Error, errorMessage.toString(), 0, error.failingURL());
        }
    }

    WebKitWebPage* m_webPage;
};

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

class PageFormClient final : public API::InjectedBundle::FormClient {
public:
    explicit PageFormClient(WebKitWebPage* webPage)
        : m_webPage(webPage)
    {
    }

    void willSubmitForm(WebPage*, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values, RefPtr<API::Object>&) override
    {
        fireFormSubmissionEvent(WEBKIT_FORM_SUBMISSION_WILL_COMPLETE, formElement, frame, sourceFrame, values);
    }

    void willSendSubmitEvent(WebPage*, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values) override
    {
        fireFormSubmissionEvent(WEBKIT_FORM_SUBMISSION_WILL_SEND_DOM_EVENT, formElement, frame, sourceFrame, values);
    }

    void didAssociateFormControls(WebPage*, const Vector<RefPtr<Element>>& elements, WebFrame* frame) override
    {
        GRefPtr<GPtrArray> formElements = adoptGRef(g_ptr_array_sized_new(elements.size()));
        for (size_t i = 0; i < elements.size(); ++i)
            g_ptr_array_add(formElements.get(), WebKit::kit(elements[i].get()));

        g_signal_emit(m_webPage, signals[FORM_CONTROLS_ASSOCIATED], 0, formElements.get());
        g_signal_emit(m_webPage, signals[FORM_CONTROLS_ASSOCIATED_FOR_FRAME], 0, formElements.get(), webkitFrameGetOrCreate(frame));
    }

    bool shouldNotifyOnFormChanges(WebPage*) override { return true; }

private:
    void fireFormSubmissionEvent(WebKitFormSubmissionStep step, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values)
    {
        WebKitFrame* webkitTargetFrame = webkitFrameGetOrCreate(frame);
        WebKitFrame* webkitSourceFrame = webkitFrameGetOrCreate(sourceFrame);

        GRefPtr<GPtrArray> textFieldNames = adoptGRef(g_ptr_array_new_full(values.size(), g_free));
        GRefPtr<GPtrArray> textFieldValues = adoptGRef(g_ptr_array_new_full(values.size(), g_free));
        for (auto& pair : values) {
            g_ptr_array_add(textFieldNames.get(), g_strdup(pair.first.utf8().data()));
            g_ptr_array_add(textFieldValues.get(), g_strdup(pair.second.utf8().data()));
        }

        g_signal_emit(m_webPage, signals[WILL_SUBMIT_FORM], 0, WEBKIT_DOM_ELEMENT(WebKit::kit(static_cast<Node*>(formElement))), step, webkitSourceFrame, webkitTargetFrame, textFieldNames.get(), textFieldValues.get());
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
     * Modifications to the #WebKitURIRequest and its associated
     * #SoupMessageHeaders will be taken into account when the request
     * is sent over the network.
     *
     * Returns: %TRUE to stop other handlers from being invoked for the event.
     *    %FALSE to continue emission of the event.
     */
    signals[SEND_REQUEST] = g_signal_new(
        "send-request",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 2,
        WEBKIT_TYPE_URI_REQUEST,
        WEBKIT_TYPE_URI_RESPONSE);

    /**
     * WebKitWebPage::context-menu:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @context_menu: the proposed #WebKitContextMenu
     * @hit_test_result: a #WebKitWebHitTestResult
     *
     * Emitted before a context menu is displayed in the UI Process to
     * give the application a chance to customize the proposed menu,
     * build its own context menu or pass user data to the UI Process.
     * This signal is useful when the information available in the UI Process
     * is not enough to build or customize the context menu, for example, to
     * add menu entries depending on the #WebKitDOMNode at the coordinates of the
     * @hit_test_result. Otherwise, it's recommended to use #WebKitWebView::context-menu
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
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 2,
        WEBKIT_TYPE_CONTEXT_MENU,
        WEBKIT_TYPE_WEB_HIT_TEST_RESULT);

    /**
     * WebKitWebPage::console-message-sent:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @console_message: the #WebKitConsoleMessage
     *
     * Emitted when a message is sent to the console. This can be a message
     * produced by the use of JavaScript console API, a JavaScript exception,
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

    /**
     * WebKitWebPage::form-controls-associated:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @elements: (element-type WebKitDOMElement) (transfer none): a #GPtrArray of
     *     #WebKitDOMElement with the list of forms in the page
     *
     * Emitted after form elements (or form associated elements) are associated to a particular web
     * page. This is useful to implement form auto filling for web pages where form fields are added
     * dynamically. This signal might be emitted multiple times for the same web page.
     *
     * Note that this signal could be also emitted when form controls are moved between forms. In
     * that case, the @elements array carries the list of those elements which have moved.
     *
     * Clients should take a reference to the members of the @elements array if it is desired to
     * keep them alive after the signal handler returns.
     *
     * Since: 2.16
     *
     * Deprecated: 2.26, use #WebKitWebPage::form-controls-associated-for-frame instead.
     */
    signals[FORM_CONTROLS_ASSOCIATED] = g_signal_new(
        "form-controls-associated",
        G_TYPE_FROM_CLASS(klass),
        static_cast<GSignalFlags>(G_SIGNAL_RUN_LAST | G_SIGNAL_DEPRECATED),
        0, 0, nullptr,
        g_cclosure_marshal_VOID__BOXED,
        G_TYPE_NONE, 1,
        G_TYPE_PTR_ARRAY);

    /**
     * WebKitWebPage::form-controls-associated-for-frame:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @elements: (element-type WebKitDOMElement) (transfer none): a #GPtrArray of
     *     #WebKitDOMElement with the list of forms in the page
     * @frame: the #WebKitFrame
     *
     * Emitted after form elements (or form associated elements) are associated to a particular web
     * page. This is useful to implement form auto filling for web pages where form fields are added
     * dynamically. This signal might be emitted multiple times for the same web page.
     *
     * Note that this signal could be also emitted when form controls are moved between forms. In
     * that case, the @elements array carries the list of those elements which have moved.
     *
     * Clients should take a reference to the members of the @elements array if it is desired to
     * keep them alive after the signal handler returns.
     *
     * Since: 2.26
     */
    signals[FORM_CONTROLS_ASSOCIATED_FOR_FRAME] = g_signal_new(
        "form-controls-associated-for-frame",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 2,
        G_TYPE_PTR_ARRAY,
        WEBKIT_TYPE_FRAME);

    /**
     * WebKitWebPage::will-submit-form:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @form: the #WebKitDOMElement to be submitted, which will always correspond to an HTMLFormElement
     * @step: a #WebKitFormSubmissionEventType indicating the current
     * stage of form submission
     * @source_frame: the #WebKitFrame containing the form to be
     * submitted
     * @target_frame: the #WebKitFrame containing the form's target,
     * which may be the same as @source_frame if no target was specified
     * @text_field_names: (element-type utf8) (transfer none): names of
     * the form's text fields
     * @text_field_values: (element-type utf8) (transfer none): values
     * of the form's text fields
     *
     * This signal is emitted to indicate various points during form
     * submission. @step indicates the current stage of form submission.
     *
     * If this signal is emitted with %WEBKIT_FORM_SUBMISSION_WILL_SEND_DOM_EVENT,
     * then the DOM submit event is about to be emitted. JavaScript code
     * may rely on the submit event to detect that the user has clicked
     * on a submit button, and to possibly cancel the form submission
     * before %WEBKIT_FORM_SUBMISSION_WILL_COMPLETE. However, beware
     * that, for historical reasons, the submit event is not emitted at
     * all if the form submission is triggered by JavaScript. For these
     * reasons, %WEBKIT_FORM_SUBMISSION_WILL_SEND_DOM_EVENT may not
     * be used to reliably detect whether a form will be submitted.
     * Instead, use it to detect if a user has clicked on a form's
     * submit button even if JavaScript later cancels the form
     * submission, or to read the values of the form's fields even if
     * JavaScript later clears certain fields before submitting. This
     * may be needed, for example, to implement a robust browser
     * password manager, as some misguided websites may use such
     * techniques to attempt to thwart password managers.
     *
     * If this signal is emitted with %WEBKIT_FORM_SUBMISSION_WILL_COMPLETE,
     * the form will imminently be submitted. It can no longer be
     * cancelled. This event always occurs immediately before a form is
     * submitted to its target, so use this event to reliably detect
     * when a form is submitted. This event occurs after
     * %WEBKIT_FORM_SUBMISSION_WILL_SEND_DOM_EVENT if that event is
     * emitted.
     *
     * Since: 2.20
     */
    signals[WILL_SUBMIT_FORM] = g_signal_new(
        "will-submit-form",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_NONE, 6,
        WEBKIT_DOM_TYPE_ELEMENT,
        WEBKIT_TYPE_FORM_SUBMISSION_STEP,
        WEBKIT_TYPE_FRAME,
        WEBKIT_TYPE_FRAME,
        G_TYPE_PTR_ARRAY,
        G_TYPE_PTR_ARRAY);
}

WebPage* webkitWebPageGetPage(WebKitWebPage *webPage)
{
    return webPage->priv->webPage;
}

WebKitWebPage* webkitWebPageCreate(WebPage* webPage)
{
    WebKitWebPage* page = WEBKIT_WEB_PAGE(g_object_new(WEBKIT_TYPE_WEB_PAGE, NULL));
    page->priv->webPage = webPage;

    webPage->setInjectedBundleResourceLoadClient(makeUnique<PageResourceLoadClient>(page));
    webPage->setInjectedBundlePageLoaderClient(makeUnique<PageLoaderClient>(page));
    webPage->setInjectedBundleContextMenuClient(makeUnique<PageContextMenuClient>(page));
    webPage->setInjectedBundleUIClient(makeUnique<PageUIClient>(page));
    webPage->setInjectedBundleFormClient(makeUnique<PageFormClient>(page));

    return page;
}

void webkitWebPageDidReceiveMessage(WebKitWebPage* page, const String& messageName, API::Dictionary& message)
{
#if PLATFORM(GTK)
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
#endif
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
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), nullptr);

    if (auto* coreFrame = webPage->priv->webPage->mainFrame())
        return kit(coreFrame->document());

    return nullptr;
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

    return webPage->priv->webPage->pageID().toUInt64();
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
