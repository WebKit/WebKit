/*
 * Copyright (C) 2017 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
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
#include "WebKitAutomationSession.h"

#include "APIAutomationSessionClient.h"
#include "WebKitApplicationInfo.h"
#include "WebKitAutomationSessionPrivate.h"
#include "WebKitWebContextPrivate.h"
#include "WebKitWebViewPrivate.h"
#include <glib/gi18n-lib.h>
#include <wtf/glib/WTFGType.h>
#include <wtf/text/CString.h>

using namespace WebKit;

/**
 * SECTION: WebKitAutomationSession
 * @Short_description: Automation Session
 * @Title: WebKitAutomationSession
 *
 * WebKitAutomationSession represents an automation session of a WebKitWebContext.
 * When a new session is requested, a WebKitAutomationSession is created and the signal
 * WebKitWebContext::automation-started is emitted with the WebKitAutomationSession as
 * argument. Then, the automation client can request the session to create a new
 * #WebKitWebView to interact with it. When this happens the signal #WebKitAutomationSession::create-web-view
 * is emitted.
 *
 * Since: 2.18
 */

enum {
    PROP_0,

    PROP_ID
};

enum {
    CREATE_WEB_VIEW,

    LAST_SIGNAL
};

struct _WebKitAutomationSessionPrivate {
    RefPtr<WebAutomationSession> session;
    WebKitApplicationInfo* applicationInfo;
    WebKitWebContext* webContext;
    CString id;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitAutomationSession, webkit_automation_session, G_TYPE_OBJECT)

class AutomationSessionClient final : public API::AutomationSessionClient {
public:
    explicit AutomationSessionClient(WebKitAutomationSession* session)
        : m_session(session)
    {
    }

private:
    String sessionIdentifier() const override
    {
        return String::fromUTF8(m_session->priv->id.data());
    }

    void didDisconnectFromRemote(WebAutomationSession&) override
    {
#if ENABLE(REMOTE_INSPECTOR)
        webkitWebContextWillCloseAutomationSession(m_session->priv->webContext);
#endif
    }

    void requestNewPageWithOptions(WebAutomationSession&, API::AutomationSessionBrowsingContextOptions, CompletionHandler<void(WebPageProxy*)>&& completionHandler) override
    {
        WebKitWebView* webView = nullptr;
        g_signal_emit(m_session, signals[CREATE_WEB_VIEW], 0, &webView);
        if (!webView || !webkit_web_view_is_controlled_by_automation(webView))
            completionHandler(nullptr);
        else
            completionHandler(&webkitWebViewGetPage(webView));
    }

    void requestMaximizeWindowOfPage(WebAutomationSession&, WebPageProxy& page, CompletionHandler<void()>&& completionHandler) override
    {
        if (auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page))
            webkitWebViewMaximizeWindow(webView, WTFMove(completionHandler));
        else
            completionHandler();
    }

    void requestHideWindowOfPage(WebAutomationSession&, WebPageProxy& page, CompletionHandler<void()>&& completionHandler) override
    {
        if (auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page))
            webkitWebViewMinimizeWindow(webView, WTFMove(completionHandler));
        else
            completionHandler();
    }

    void requestRestoreWindowOfPage(WebAutomationSession&, WebPageProxy& page, CompletionHandler<void()>&& completionHandler) override
    {
        if (auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page))
            webkitWebViewRestoreWindow(webView, WTFMove(completionHandler));
        else
            completionHandler();
    }

    bool isShowingJavaScriptDialogOnPage(WebAutomationSession&, WebPageProxy& page) override
    {
        auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page);
        if (!webView)
            return false;
        return webkitWebViewIsShowingScriptDialog(webView);
    }

    void dismissCurrentJavaScriptDialogOnPage(WebAutomationSession&, WebPageProxy& page) override
    {
        auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page);
        if (!webView)
            return;
        webkitWebViewDismissCurrentScriptDialog(webView);
    }

    void acceptCurrentJavaScriptDialogOnPage(WebAutomationSession&, WebPageProxy& page) override
    {
        auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page);
        if (!webView)
            return;
        webkitWebViewAcceptCurrentScriptDialog(webView);
    }

    String messageOfCurrentJavaScriptDialogOnPage(WebAutomationSession&, WebPageProxy& page) override
    {
        auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page);
        if (!webView)
            return { };
        return webkitWebViewGetCurrentScriptDialogMessage(webView);
    }

    void setUserInputForCurrentJavaScriptPromptOnPage(WebAutomationSession&, WebPageProxy& page, const String& userInput) override
    {
        auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page);
        if (!webView)
            return;
        webkitWebViewSetCurrentScriptDialogUserInput(webView, userInput);
    }

    Optional<API::AutomationSessionClient::JavaScriptDialogType> typeOfCurrentJavaScriptDialogOnPage(WebAutomationSession&, WebPageProxy& page) override
    {
        auto* webView = webkitWebContextGetWebViewForPage(m_session->priv->webContext, &page);
        if (!webView)
            return WTF::nullopt;
        auto dialogType = webkitWebViewGetCurrentScriptDialogType(webView);
        if (!dialogType)
            return WTF::nullopt;
        switch (dialogType.value()) {
        case WEBKIT_SCRIPT_DIALOG_ALERT:
            return API::AutomationSessionClient::JavaScriptDialogType::Alert;
        case WEBKIT_SCRIPT_DIALOG_CONFIRM:
            return API::AutomationSessionClient::JavaScriptDialogType::Confirm;
        case WEBKIT_SCRIPT_DIALOG_PROMPT:
            return API::AutomationSessionClient::JavaScriptDialogType::Prompt;
        case WEBKIT_SCRIPT_DIALOG_BEFORE_UNLOAD_CONFIRM:
            return API::AutomationSessionClient::JavaScriptDialogType::BeforeUnloadConfirm;
        }

        ASSERT_NOT_REACHED();
        return WTF::nullopt;
    }

    WebKitAutomationSession* m_session;
};

static void webkitAutomationSessionGetProperty(GObject* object, guint propID, GValue* value, GParamSpec* paramSpec)
{
    WebKitAutomationSession* session = WEBKIT_AUTOMATION_SESSION(object);

    switch (propID) {
    case PROP_ID:
        g_value_set_string(value, session->priv->id.data());
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkitAutomationSessionSetProperty(GObject* object, guint propID, const GValue* value, GParamSpec* paramSpec)
{
    WebKitAutomationSession* session = WEBKIT_AUTOMATION_SESSION(object);

    switch (propID) {
    case PROP_ID:
        session->priv->id = g_value_get_string(value);
        break;
    default:
        G_OBJECT_WARN_INVALID_PROPERTY_ID(object, propID, paramSpec);
    }
}

static void webkitAutomationSessionConstructed(GObject* object)
{
    WebKitAutomationSession* session = WEBKIT_AUTOMATION_SESSION(object);

    G_OBJECT_CLASS(webkit_automation_session_parent_class)->constructed(object);

    session->priv->session = adoptRef(new WebAutomationSession());
    session->priv->session->setSessionIdentifier(String::fromUTF8(session->priv->id.data()));
    session->priv->session->setClient(std::make_unique<AutomationSessionClient>(session));
}

static void webkitAutomationSessionDispose(GObject* object)
{
    WebKitAutomationSession* session = WEBKIT_AUTOMATION_SESSION(object);

    session->priv->session->setClient(nullptr);

    if (session->priv->applicationInfo) {
        webkit_application_info_unref(session->priv->applicationInfo);
        session->priv->applicationInfo = nullptr;
    }

    G_OBJECT_CLASS(webkit_automation_session_parent_class)->dispose(object);
}

static void webkit_automation_session_class_init(WebKitAutomationSessionClass* sessionClass)
{
    GObjectClass* gObjectClass = G_OBJECT_CLASS(sessionClass);
    gObjectClass->get_property = webkitAutomationSessionGetProperty;
    gObjectClass->set_property = webkitAutomationSessionSetProperty;
    gObjectClass->constructed = webkitAutomationSessionConstructed;
    gObjectClass->dispose = webkitAutomationSessionDispose;

    /**
     * WebKitAutomationSession:id:
     *
     * The session unique identifier.
     *
     * Since: 2.18
     */
    g_object_class_install_property(
        gObjectClass,
        PROP_ID,
        g_param_spec_string(
            "id",
            _("Identifier"),
            _("The automation session identifier"),
            nullptr,
            static_cast<GParamFlags>(WEBKIT_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY)));

    /**
     * WebKitAutomationSession::create-web-view:
     * @session: a #WebKitAutomationSession
     *
     * This signal is emitted when the automation client requests a new
     * browsing context to interact with it. The callback handler should
     * return a #WebKitWebView created with #WebKitWebView:is-controlled-by-automation
     * construct property enabled. The returned #WebKitWebView could be an existing
     * web view or a new one created and added to a new tab or window.
     *
     * Returns: (transfer none): a #WebKitWebView widget.
     *
     * Since: 2.18
     */
    signals[CREATE_WEB_VIEW] = g_signal_new(
        "create-web-view",
        G_TYPE_FROM_CLASS(sessionClass),
        G_SIGNAL_RUN_LAST,
        0,
        nullptr, nullptr,
        g_cclosure_marshal_generic,
        WEBKIT_TYPE_WEB_VIEW, 0,
        G_TYPE_NONE);
}

#if ENABLE(REMOTE_INSPECTOR)
WebKitAutomationSession* webkitAutomationSessionCreate(WebKitWebContext* webContext, const char* sessionID, const Inspector::RemoteInspector::Client::SessionCapabilities& capabilities)
{
    auto* session = WEBKIT_AUTOMATION_SESSION(g_object_new(WEBKIT_TYPE_AUTOMATION_SESSION, "id", sessionID, nullptr));
    session->priv->webContext = webContext;
    if (capabilities.acceptInsecureCertificates)
        webkit_web_context_set_tls_errors_policy(webContext, WEBKIT_TLS_ERRORS_POLICY_IGNORE);
    for (auto& certificate : capabilities.certificates) {
        GRefPtr<GTlsCertificate> tlsCertificate = adoptGRef(g_tls_certificate_new_from_file(certificate.second.utf8().data(), nullptr));
        if (tlsCertificate)
            webkit_web_context_allow_tls_certificate_for_host(webContext, tlsCertificate.get(), certificate.first.utf8().data());
    }
    return session;
}
#endif

WebAutomationSession& webkitAutomationSessionGetSession(WebKitAutomationSession* session)
{
    return *session->priv->session;
}

String webkitAutomationSessionGetBrowserName(WebKitAutomationSession* session)
{
    if (session->priv->applicationInfo)
        return String::fromUTF8(webkit_application_info_get_name(session->priv->applicationInfo));

    return g_get_prgname();
}

String webkitAutomationSessionGetBrowserVersion(WebKitAutomationSession* session)
{
    if (!session->priv->applicationInfo)
        return { };

    guint64 major, minor, micro;
    webkit_application_info_get_version(session->priv->applicationInfo, &major, &minor, &micro);

    if (!micro && !minor)
        return String::number(major);

    if (!micro)
        return makeString(String::number(major), ".", String::number(minor));

    return makeString(String::number(major), ".", String::number(minor), ".", String::number(micro));
}

/**
 * webkit_automation_session_get_id:
 * @session: a #WebKitAutomationSession
 *
 * Get the unique identifier of a #WebKitAutomationSession
 *
 * Returns: the unique identifier of @session
 *
 * Since: 2.18
 */
const char* webkit_automation_session_get_id(WebKitAutomationSession* session)
{
    g_return_val_if_fail(WEBKIT_IS_AUTOMATION_SESSION(session), nullptr);
    return session->priv->id.data();
}

/**
 * webkit_automation_session_set_application_info:
 * @session: a #WebKitAutomationSession
 * @info: a #WebKitApplicationInfo
 *
 * Set the application information to @session. This information will be used by the driver service
 * to match the requested capabilities with the actual application information. If this information
 * is not provided to the session when a new automation session is requested, the creation might fail
 * if the client requested a specific browser name or version. This will not have any effect when called
 * after the automation session has been fully created, so this must be called in the callback of
 * #WebKitWebContext::automation-started signal.
 *
 * Since: 2.18
 */
void webkit_automation_session_set_application_info(WebKitAutomationSession* session, WebKitApplicationInfo* info)
{
    g_return_if_fail(WEBKIT_IS_AUTOMATION_SESSION(session));
    g_return_if_fail(info);

    if (session->priv->applicationInfo == info)
        return;

    if (session->priv->applicationInfo)
        webkit_application_info_unref(session->priv->applicationInfo);
    session->priv->applicationInfo = webkit_application_info_ref(info);
}

/**
 * webkit_automation_session_get_application_info:
 * @session: a #WebKitAutomationSession
 *
 * Get the #WebKitAutomationSession previously set with webkit_automation_session_set_application_info().
 *
 * Returns: (transfer none): the #WebKitAutomationSession of @session, or %NULL if no one has been set.
 *
 * Since: 2.18
 */
WebKitApplicationInfo* webkit_automation_session_get_application_info(WebKitAutomationSession* session)
{
    g_return_val_if_fail(WEBKIT_IS_AUTOMATION_SESSION(session), nullptr);

    return session->priv->applicationInfo;
}
