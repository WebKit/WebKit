/*
 * Copyright (C) 2013 Samsung Electronics Inc. All rights reserved.
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
#include "WebKitAuthenticationRequest.h"

#include "AuthenticationDecisionListener.h"
#include "WebCredential.h"
#include "WebKitAuthenticationRequestPrivate.h"
#include "WebKitCredentialPrivate.h"
#include "WebProtectionSpace.h"
#include <glib/gi18n-lib.h>
#include <wtf/text/CString.h>

using namespace WebKit;
using namespace WebCore;

/**
 * SECTION: WebKitAuthenticationRequest
 * @Short_description: Represents an authentication request
 * @Title: WebKitAuthenticationRequest
 * @See_also: #WebKitWebView
 *
 * Whenever a client attempts to load a page protected by HTTP
 * authentication, credentials will need to be provided to authorize access.
 * To allow the client to decide how it wishes to handle authentication,
 * WebKit will fire a #WebKitWebView::authenticate signal with a
 * WebKitAuthenticationRequest object to provide client side
 * authentication support. Credentials are exposed through the
 * #WebKitCredential object.
 *
 * In case the client application does not wish
 * to handle this signal WebKit will provide a default handler. To handle
 * authentication asynchronously, simply increase the reference count of the
 * WebKitAuthenticationRequest object.
 */

enum {
    CANCELLED,

    LAST_SIGNAL
};

struct _WebKitAuthenticationRequestPrivate {
    RefPtr<AuthenticationChallengeProxy> authenticationChallenge;
    bool privateBrowsingEnabled;
    bool handledRequest;
    CString host;
    CString realm;
};

static guint signals[LAST_SIGNAL] = { 0, };

WEBKIT_DEFINE_TYPE(WebKitAuthenticationRequest, webkit_authentication_request, G_TYPE_OBJECT)

static inline WebKitAuthenticationScheme toWebKitAuthenticationScheme(WebCore::ProtectionSpaceAuthenticationScheme coreScheme)
{
    switch (coreScheme) {
    case WebCore::ProtectionSpaceAuthenticationSchemeDefault:
        return WEBKIT_AUTHENTICATION_SCHEME_DEFAULT;
    case WebCore::ProtectionSpaceAuthenticationSchemeHTTPBasic:
        return WEBKIT_AUTHENTICATION_SCHEME_HTTP_BASIC;
    case WebCore::ProtectionSpaceAuthenticationSchemeHTTPDigest:
        return WEBKIT_AUTHENTICATION_SCHEME_HTTP_DIGEST;
    case WebCore::ProtectionSpaceAuthenticationSchemeHTMLForm:
        return WEBKIT_AUTHENTICATION_SCHEME_HTML_FORM;
    case WebCore::ProtectionSpaceAuthenticationSchemeNTLM:
        return WEBKIT_AUTHENTICATION_SCHEME_NTLM;
    case WebCore::ProtectionSpaceAuthenticationSchemeNegotiate:
        return WEBKIT_AUTHENTICATION_SCHEME_NEGOTIATE;
    case WebCore::ProtectionSpaceAuthenticationSchemeClientCertificateRequested:
        return WEBKIT_AUTHENTICATION_SCHEME_CLIENT_CERTIFICATE_REQUESTED;
    case WebCore::ProtectionSpaceAuthenticationSchemeServerTrustEvaluationRequested:
        return WEBKIT_AUTHENTICATION_SCHEME_SERVER_TRUST_EVALUATION_REQUESTED;
    case WebCore::ProtectionSpaceAuthenticationSchemeUnknown:
        return WEBKIT_AUTHENTICATION_SCHEME_UNKNOWN;
    default:
        ASSERT_NOT_REACHED();
        return WEBKIT_AUTHENTICATION_SCHEME_DEFAULT;
    }
}

static void webkitAuthenticationRequestDispose(GObject* object)
{
    WebKitAuthenticationRequest* request = WEBKIT_AUTHENTICATION_REQUEST(object);

    // Make sure the request is always handled before finalizing.
    if (!request->priv->handledRequest)
        webkit_authentication_request_cancel(request);

    G_OBJECT_CLASS(webkit_authentication_request_parent_class)->dispose(object);
}

static void webkit_authentication_request_class_init(WebKitAuthenticationRequestClass* requestClass)
{
    GObjectClass* objectClass = G_OBJECT_CLASS(requestClass);
    objectClass->dispose = webkitAuthenticationRequestDispose;

    /**
     * WebKitAuthenticationRequest::cancelled:
     * @request: the #WebKitAuthenticationRequest
     *
     * This signal is emitted when the user authentication request is
     * cancelled. It allows the application to dismiss its authentication
     * dialog in case of page load failure for example.
     *
     * Since: 2.2
     */
    signals[CANCELLED] =
        g_signal_new("cancelled",
            G_TYPE_FROM_CLASS(objectClass),
            G_SIGNAL_RUN_LAST,
            0, 0, 0,
            g_cclosure_marshal_VOID__VOID,
            G_TYPE_NONE, 0);
}

WebKitAuthenticationRequest* webkitAuthenticationRequestCreate(AuthenticationChallengeProxy* authenticationChallenge, bool privateBrowsingEnabled)
{
    WebKitAuthenticationRequest* request = WEBKIT_AUTHENTICATION_REQUEST(g_object_new(WEBKIT_TYPE_AUTHENTICATION_REQUEST, NULL));
    request->priv->authenticationChallenge = authenticationChallenge;
    request->priv->privateBrowsingEnabled = privateBrowsingEnabled;
    return request;
}

AuthenticationChallengeProxy* webkitAuthenticationRequestGetAuthenticationChallenge(WebKitAuthenticationRequest* request)
{
    return request->priv->authenticationChallenge.get();
}

/**
 * webkit_authentication_request_can_save_credentials:
 * @request: a #WebKitAuthenticationRequest
 *
 * Determine whether the authentication method associated with this
 * #WebKitAuthenticationRequest should allow the storage of credentials.
 * This will return %FALSE if webkit doesn't support credential storing
 * or if private browsing is enabled.
 *
 * Returns: %TRUE if webkit can store credentials or %FALSE otherwise.
 *
 * Since: 2.2
 */
gboolean webkit_authentication_request_can_save_credentials(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), FALSE);

#if ENABLE(CREDENTIAL_STORAGE)
    return !request->priv->privateBrowsingEnabled;
#else
    return FALSE;
#endif
}

/**
 * webkit_authentication_request_get_proposed_credential:
 * @request: a #WebKitAuthenticationRequest
 *
 * Get the #WebKitCredential of the proposed authentication challenge that was
 * stored from a previous session. The client can use this directly for
 * authentication or construct their own #WebKitCredential.
 *
 * Returns: (transfer full): A #WebKitCredential encapsulating credential details
 * or %NULL if there is no stored credential.
 *
 * Since: 2.2
 */
WebKitCredential* webkit_authentication_request_get_proposed_credential(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), 0);

    const WebCore::Credential& credential = request->priv->authenticationChallenge->proposedCredential()->core();
    if (credential.isEmpty())
        return 0;

    return webkitCredentialCreate(credential);
}

/**
 * webkit_authentication_request_get_host:
 * @request: a #WebKitAuthenticationRequest
 *
 * Get the host that this authentication challenge is applicable to.
 *
 * Returns: The host of @request.
 *
 * Since: 2.2
 */
const gchar* webkit_authentication_request_get_host(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), 0);

    if (request->priv->host.isNull())
        request->priv->host = request->priv->authenticationChallenge->protectionSpace()->host().utf8();
    return request->priv->host.data();
}

/**
 * webkit_authentication_request_get_port:
 * @request: a #WebKitAuthenticationRequest
 *
 * Get the port that this authentication challenge is applicable to.
 *
 * Returns: The port of @request.
 *
 * Since: 2.2
 */
guint webkit_authentication_request_get_port(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), 0);

    return request->priv->authenticationChallenge->protectionSpace()->port();
}

/**
 * webkit_authentication_request_get_realm:
 * @request: a #WebKitAuthenticationRequest
 *
 * Get the realm that this authentication challenge is applicable to.
 *
 * Returns: The realm of @request.
 *
 * Since: 2.2
 */
const gchar* webkit_authentication_request_get_realm(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), 0);

    if (request->priv->realm.isNull())
        request->priv->realm = request->priv->authenticationChallenge->protectionSpace()->realm().utf8();
    return request->priv->realm.data();
}

/**
 * webkit_authentication_request_get_scheme:
 * @request: a #WebKitAuthenticationRequest
 *
 * Get the authentication scheme of the authentication challenge.
 *
 * Returns: The #WebKitAuthenticationScheme of @request.
 *
 * Since: 2.2
 */
WebKitAuthenticationScheme webkit_authentication_request_get_scheme(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), WEBKIT_AUTHENTICATION_SCHEME_UNKNOWN);

    return toWebKitAuthenticationScheme(request->priv->authenticationChallenge->protectionSpace()->authenticationScheme());
}

/**
 * webkit_authentication_request_is_for_proxy:
 * @request: a #WebKitAuthenticationRequest
 *
 * Determine whether the authentication challenge is associated with a proxy server rather than an "origin" server.
 *
 * Returns: %TRUE if authentication is for a proxy or %FALSE otherwise.
 *
 * Since: 2.2
 */
gboolean webkit_authentication_request_is_for_proxy(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), FALSE);

    return request->priv->authenticationChallenge->protectionSpace()->isProxy();
}

/**
 * webkit_authentication_request_is_retry:
 * @request: a #WebKitAuthenticationRequest
 *
 * Determine whether this this is a first attempt or a retry for this authentication challenge.
 *
 * Returns: %TRUE if authentication attempt is a retry or %FALSE otherwise.
 *
 * Since: 2.2
 */
gboolean webkit_authentication_request_is_retry(WebKitAuthenticationRequest* request)
{
    g_return_val_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request), 0);

    return request->priv->authenticationChallenge->previousFailureCount() ? TRUE : FALSE;
}

/**
 * webkit_authentication_request_authenticate:
 * @request: a #WebKitAuthenticationRequest
 * @credential: (transfer none) (allow-none): A #WebKitCredential, or %NULL
 *
 * Authenticate the #WebKitAuthenticationRequest using the #WebKitCredential
 * supplied. To continue without credentials, pass %NULL as @credential.
 *
 * Since: 2.2
 */
void webkit_authentication_request_authenticate(WebKitAuthenticationRequest* request, WebKitCredential* credential)
{
    g_return_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request));

    RefPtr<WebCredential> webCredential = credential ? WebCredential::create(webkitCredentialGetCredential(credential)) : 0;
    request->priv->authenticationChallenge->listener()->useCredential(webCredential.get());
    request->priv->handledRequest = true;
}

/**
 * webkit_authentication_request_cancel:
 * @request: a #WebKitAuthenticationRequest
 *
 * Cancel the authentication challenge. This will also cancel the page loading and result in a
 * #WebKitWebView::load-failed signal with a #WebKitNetworkError of type %WEBKIT_NETWORK_ERROR_CANCELLED being emitted.
 *
 * Since: 2.2
 */
void webkit_authentication_request_cancel(WebKitAuthenticationRequest* request)
{
    g_return_if_fail(WEBKIT_IS_AUTHENTICATION_REQUEST(request));

    request->priv->authenticationChallenge->listener()->cancel();

    g_signal_emit(request, signals[CANCELLED], 0);
}
