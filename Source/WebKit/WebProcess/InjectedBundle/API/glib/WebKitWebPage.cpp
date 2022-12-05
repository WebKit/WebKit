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

#include "APIString.h"
#include "InjectedBundle.h"
#include "WebContextMenuItem.h"
#include "WebKitConsoleMessage.h"
#include "WebKitContextMenuPrivate.h"
#include "WebKitDOMDocumentPrivate.h"
#include "WebKitDOMElementPrivate.h"
#include "WebKitDOMNodePrivate.h"
#include "WebKitFramePrivate.h"
#include "WebKitPrivate.h"
#include "WebKitScriptWorldPrivate.h"
#include "WebKitURIRequestPrivate.h"
#include "WebKitURIResponsePrivate.h"
#include "WebKitUserMessagePrivate.h"
#include "WebKitWebEditorPrivate.h"
#include "WebKitWebFormManagerPrivate.h"
#include "WebKitWebHitTestResultPrivate.h"
#include "WebKitWebPagePrivate.h"
#include "WebKitWebProcessEnumTypes.h"
#include "WebPageProxyMessages.h"
#include "WebProcess.h"
#include <WebCore/ContextMenuContext.h>
#include <WebCore/Document.h>
#include <WebCore/DocumentLoader.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameDestructionObserver.h>
#include <WebCore/FrameLoader.h>
#include <WebCore/FrameView.h>
#include <WebCore/HTMLFormElement.h>
#include <glib/gi18n-lib.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

/**
 * WebKitWebPage:
 *
 * A loaded web page.
 */

enum {
    DOCUMENT_LOADED,
    SEND_REQUEST,
    CONTEXT_MENU,
    CONSOLE_MESSAGE_SENT,
    FORM_CONTROLS_ASSOCIATED,
    FORM_CONTROLS_ASSOCIATED_FOR_FRAME,
    WILL_SUBMIT_FORM,
    USER_MESSAGE_RECEIVED,

    LAST_SIGNAL
};

enum {
    PROP_0,
    PROP_URI,
    N_PROPERTIES,
};

static GParamSpec* sObjProperties[N_PROPERTIES] = { nullptr, };

struct _WebKitWebPagePrivate {
    WebPage* webPage;

    CString uri;

    GRefPtr<WebKitWebEditor> webEditor;
    HashMap<WebKitScriptWorld*, GRefPtr<WebKitWebFormManager>> formManagerMap;
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
    g_object_notify_by_pspec(G_OBJECT(webPage), sObjProperties[PROP_URI]);
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

    void globalObjectIsAvailableForFrame(WebPage&, WebFrame& frame, DOMWrapperWorld& world) override
    {
        // Force the creation of the JavaScript context for existing WebKitScriptWorlds to
        // ensure WebKitScriptWorld::window-object-cleared signal is emitted.
        auto injectedWorld = InjectedBundleScriptWorld::getOrCreate(world);
        if (webkitScriptWorldGet(injectedWorld.ptr()))
            frame.jsContextForWorld(injectedWorld.ptr());
    }

    void serviceWorkerGlobalObjectIsAvailableForFrame(WebPage&, WebFrame&, DOMWrapperWorld&) override
    {
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
    void willSendRequestForFrame(WebPage& page, WebFrame&, WebCore::ResourceLoaderIdentifier identifier, ResourceRequest& resourceRequest, const ResourceResponse& redirectResourceResponse) override
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
    bool getCustomMenuFromDefaultItems(WebPage&, const WebCore::HitTestResult& hitTestResult, const Vector<WebCore::ContextMenuItem>& defaultMenu, Vector<WebContextMenuItemData>& newMenu, const WebCore::ContextMenuContext& context, RefPtr<API::Object>& userData) override
    {
        if (context.type() != ContextMenuContext::Type::ContextMenu)
            return false;

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

class PageFormClient final : public API::InjectedBundle::FormClient {
public:
    explicit PageFormClient(WebKitWebPage* webPage)
        : m_webPage(webPage)
    {
    }

    void willSubmitForm(WebPage*, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values, RefPtr<API::Object>&) override
    {
        if (!formElement)
            return;

        if (!m_webPage->priv->formManagerMap.isEmpty()) {
            auto* wkFrame = webkitFrameGetOrCreate(sourceFrame);
            auto* wkTargetFrame = webkitFrameGetOrCreate(frame);
            for (const auto& it : m_webPage->priv->formManagerMap)
                webkitWebFormManagerWillSubmitForm(it.value.get(), webkitFrameGetJSCValueForElementInWorld(wkFrame, *formElement, it.key), wkFrame, wkTargetFrame);
        } else
            fireFormSubmissionEvent(WEBKIT_FORM_SUBMISSION_WILL_COMPLETE, formElement, frame, sourceFrame, values);
    }

    void willSendSubmitEvent(WebPage*, HTMLFormElement* formElement, WebFrame* frame, WebFrame* sourceFrame, const Vector<std::pair<String, String>>& values) override
    {
        if (!formElement)
            return;

        if (!m_webPage->priv->formManagerMap.isEmpty()) {
            auto* wkFrame = webkitFrameGetOrCreate(sourceFrame);
            auto* wkTargetFrame = webkitFrameGetOrCreate(frame);
            for (const auto& it : m_webPage->priv->formManagerMap)
                webkitWebFormManagerWillSendSubmitEvent(it.value.get(), webkitFrameGetJSCValueForElementInWorld(wkFrame, *formElement, it.key), wkFrame, wkTargetFrame);
        } else
            fireFormSubmissionEvent(WEBKIT_FORM_SUBMISSION_WILL_SEND_DOM_EVENT, formElement, frame, sourceFrame, values);
    }

    void didAssociateFormControls(WebPage*, const Vector<RefPtr<Element>>& elements, WebFrame* frame) override
    {
        auto* wkFrame = webkitFrameGetOrCreate(frame);
        if (!m_webPage->priv->formManagerMap.isEmpty()) {
            for (const auto& it : m_webPage->priv->formManagerMap)
                webkitWebFormManagerDidAssociateFormControls(it.value.get(), wkFrame, webkitFrameGetJSCValuesForElementsInWorld(wkFrame, elements, it.key));
            return;
        }

        GRefPtr<GPtrArray> formElements = adoptGRef(g_ptr_array_sized_new(elements.size()));
        for (size_t i = 0; i < elements.size(); ++i)
            g_ptr_array_add(formElements.get(), WebKit::kit(elements[i].get()));

        g_signal_emit(m_webPage, signals[FORM_CONTROLS_ASSOCIATED], 0, formElements.get());
        g_signal_emit(m_webPage, signals[FORM_CONTROLS_ASSOCIATED_FOR_FRAME], 0, formElements.get(), wkFrame);
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

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        g_signal_emit(m_webPage, signals[WILL_SUBMIT_FORM], 0, WEBKIT_DOM_ELEMENT(WebKit::kit(static_cast<Node*>(formElement))), step, webkitSourceFrame, webkitTargetFrame, textFieldNames.get(), textFieldValues.get());
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    WebKitWebPage* m_webPage;
};

static void worldDestroyed(WebKitWebPage* webPage, WebKitScriptWorld* world)
{
    webPage->priv->formManagerMap.remove(world);
}

static void webkitWebPageDispose(GObject* object)
{
    auto* priv = WEBKIT_WEB_PAGE(object)->priv;
    for (auto* world : priv->formManagerMap.keys())
        g_object_weak_unref(G_OBJECT(world), reinterpret_cast<GWeakNotify>(worldDestroyed), object);
    priv->formManagerMap.clear();

    G_OBJECT_CLASS(webkit_web_page_parent_class)->dispose(object);
}

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
    gObjectClass->dispose = webkitWebPageDispose;
    gObjectClass->get_property = webkitWebPageGetProperty;

    /**
     * WebKitWebPage:uri:
     *
     * The current active URI of the #WebKitWebPage.
     */
    sObjProperties[PROP_URI] =
        g_param_spec_string(
            "uri",
            _("URI"),
            _("The current active URI of the web page"),
            0,
            WEBKIT_PARAM_READABLE);

    g_object_class_install_properties(gObjectClass, N_PROPERTIES, sObjProperties);

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

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
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
     * This signal is now deprecated and it's never emitted.
     *
     * Since: 2.12
     *
     * Deprecated: 2.40
     */
    signals[CONSOLE_MESSAGE_SENT] = g_signal_new(
        "console-message-sent",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0, 0, nullptr,
        g_cclosure_marshal_VOID__BOXED,
        G_TYPE_NONE, 1,
        WEBKIT_TYPE_CONSOLE_MESSAGE | G_SIGNAL_TYPE_STATIC_SCOPE);
ALLOW_DEPRECATED_DECLARATIONS_END

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
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
     *
     * Deprecated: 2.40: Use #WebKitWebFormManager::form-controls-associated instead.
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
     *
     * Deprecated: 2.40: Use #WebKitWebFormManager::will-send-submit-event and #WebKitWebFormManager::will-submit-form instead.
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
ALLOW_DEPRECATED_DECLARATIONS_END

    /**
     * WebKitWebPage::user-message-received:
     * @web_page: the #WebKitWebPage on which the signal is emitted
     * @message: the #WebKitUserMessage received
     *
     * This signal is emitted when a #WebKitUserMessage is received from the
     * #WebKitWebView corresponding to @web_page. You can reply to the message
     * using webkit_user_message_send_reply().
     *
     * You can handle the user message asynchronously by calling g_object_ref() on
     * @message and returning %TRUE. If the last reference of @message is removed
     * and the message has been replied, the operation in the #WebKitWebView will
     * finish with error %WEBKIT_USER_MESSAGE_UNHANDLED_MESSAGE.
     *
     * Returns: %TRUE if the message was handled, or %FALSE otherwise.
     *
     * Since: 2.28
     */
    signals[USER_MESSAGE_RECEIVED] = g_signal_new(
        "user-message-received",
        G_TYPE_FROM_CLASS(klass),
        G_SIGNAL_RUN_LAST,
        0,
        g_signal_accumulator_true_handled, nullptr,
        g_cclosure_marshal_generic,
        G_TYPE_BOOLEAN, 1,
        WEBKIT_TYPE_USER_MESSAGE);
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
    webPage->setInjectedBundleFormClient(makeUnique<PageFormClient>(page));

    return page;
}

void webkitWebPageDidReceiveUserMessage(WebKitWebPage* webPage, UserMessage&& message, CompletionHandler<void(UserMessage&&)>&& completionHandler)
{
    // Sink the floating ref.
    GRefPtr<WebKitUserMessage> userMessage = webkitUserMessageCreate(WTFMove(message), WTFMove(completionHandler));
    gboolean returnValue;
    g_signal_emit(webPage, signals[USER_MESSAGE_RECEIVED], 0, userMessage.get(), &returnValue);
}

/**
 * webkit_web_page_get_dom_document:
 * @web_page: a #WebKitWebPage
 *
 * Get the #WebKitDOMDocument currently loaded in @web_page
 *
 * Returns: (transfer none): the #WebKitDOMDocument currently loaded, or %NULL
 *    if no document is currently loaded.
 *
 * Deprecated: 2.40. Use JavaScriptCore API instead.
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

    return webPage->priv->webPage->identifier().toUInt64();
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

    return webkitFrameGetOrCreate(&webPage->priv->webPage->mainWebFrame());
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

/**
 * webkit_web_page_send_message_to_view:
 * @web_page: a #WebKitWebPage
 * @message: a #WebKitUserMessage
 * @cancellable: (nullable): a #GCancellable or %NULL to ignore
 * @callback: (scope async): (nullable): A #GAsyncReadyCallback to call when the request is satisfied or %NULL
 * @user_data: (closure): the data to pass to callback function
 *
 * Send @message to the #WebKitWebView corresponding to @web_page. If @message is floating, it's consumed.
 *
 * If you don't expect any reply, or you simply want to ignore it, you can pass %NULL as @callback.
 * When the operation is finished, @callback will be called. You can then call
 * webkit_web_page_send_message_to_view_finish() to get the message reply.
 *
 * Since: 2.28
 */
void webkit_web_page_send_message_to_view(WebKitWebPage* webPage, WebKitUserMessage* message, GCancellable* cancellable, GAsyncReadyCallback callback, gpointer userData)
{
    g_return_if_fail(WEBKIT_IS_WEB_PAGE(webPage));
    g_return_if_fail(WEBKIT_IS_USER_MESSAGE(message));

    // We sink the reference in case of being floating.
    GRefPtr<WebKitUserMessage> adoptedMessage = message;
    if (!callback) {
        webPage->priv->webPage->send(Messages::WebPageProxy::SendMessageToWebView(webkitUserMessageGetMessage(message)));
        return;
    }

    GRefPtr<GTask> task = adoptGRef(g_task_new(webPage, cancellable, callback, userData));
    CompletionHandler<void(UserMessage&&)> completionHandler = [task = WTFMove(task)](UserMessage&& replyMessage) {
        switch (replyMessage.type) {
        case UserMessage::Type::Null:
            g_task_return_new_error(task.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED, _("Operation was cancelled"));
            break;
        case UserMessage::Type::Message:
            g_task_return_pointer(task.get(), g_object_ref_sink(webkitUserMessageCreate(WTFMove(replyMessage))), static_cast<GDestroyNotify>(g_object_unref));
            break;
        case UserMessage::Type::Error:
            g_task_return_new_error(task.get(), WEBKIT_USER_MESSAGE_ERROR, replyMessage.errorCode, _("Message %s was not handled"), replyMessage.name.data());
            break;
        }
    };
    webPage->priv->webPage->sendWithAsyncReply(Messages::WebPageProxy::SendMessageToWebViewWithReply(webkitUserMessageGetMessage(message)), WTFMove(completionHandler));
}

/**
 * webkit_web_page_send_message_to_view_finish:
 * @web_page: a #WebKitWebPage
 * @result: a #GAsyncResult
 * @error: return location for error or %NULL to ignor
 *
 * Finish an asynchronous operation started with webkit_web_page_send_message_to_view().
 *
 * Returns: (transfer full): a #WebKitUserMessage with the reply or %NULL in case of error.
 *
 * Since: 2.28
 */
WebKitUserMessage* webkit_web_page_send_message_to_view_finish(WebKitWebPage* webPage, GAsyncResult* result, GError** error)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), nullptr);
    g_return_val_if_fail(g_task_is_valid(result, webPage), nullptr);

    return WEBKIT_USER_MESSAGE(g_task_propagate_pointer(G_TASK(result), error));
}

/**
 * webkit_web_page_get_form_manager:
 * @web_page: a #WebKitWebPage
 * @world: (nullable): a #WebKitScriptWorld
 *
 * Get the #WebKitWebFormManager of @web_page in @world.
 *
 * Returns: (transfer none): a #WebKitWebFormManager
 *
 * Since: 2.40
 */
WebKitWebFormManager* webkit_web_page_get_form_manager(WebKitWebPage* webPage, WebKitScriptWorld* world)
{
    g_return_val_if_fail(WEBKIT_IS_WEB_PAGE(webPage), nullptr);
    g_return_val_if_fail(!world || WEBKIT_IS_SCRIPT_WORLD(world), nullptr);

    if (!world)
        world = webkit_script_world_get_default();

    auto addResult = webPage->priv->formManagerMap.ensure(world, [] {
        return adoptGRef(webkitWebFormManagerCreate());
    });
    if (addResult.isNewEntry)
        g_object_weak_ref(G_OBJECT(world), reinterpret_cast<GWeakNotify>(worldDestroyed), webPage);

    return addResult.iterator->value.get();
}
