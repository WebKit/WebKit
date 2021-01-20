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
#include "WebKitCredential.h"

#include "WebCredential.h"
#include "WebKitCredentialPrivate.h"
#include <wtf/text/CString.h>

using namespace WebKit;

struct _WebKitCredential {
    _WebKitCredential(const WebCore::Credential& coreCredential)
        : credential(coreCredential)
    {
    }

    WebCore::Credential credential;
    CString username;
    CString password;
};

G_DEFINE_BOXED_TYPE(WebKitCredential, webkit_credential, webkit_credential_copy, webkit_credential_free)

static inline WebKitCredentialPersistence toWebKitCredentialPersistence(WebCore::CredentialPersistence corePersistence)
{
    switch (corePersistence) {
    case WebCore::CredentialPersistenceNone:
        return WEBKIT_CREDENTIAL_PERSISTENCE_NONE;
    case WebCore::CredentialPersistenceForSession:
        return WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION;
    case WebCore::CredentialPersistencePermanent:
        return WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT;
    default:
        ASSERT_NOT_REACHED();
        return WEBKIT_CREDENTIAL_PERSISTENCE_NONE;
    }
}

static inline WebCore::CredentialPersistence toWebCoreCredentialPersistence(WebKitCredentialPersistence kitPersistence)
{
    switch (kitPersistence) {
    case WEBKIT_CREDENTIAL_PERSISTENCE_NONE:
        return WebCore::CredentialPersistenceNone;
    case WEBKIT_CREDENTIAL_PERSISTENCE_FOR_SESSION:
        return WebCore::CredentialPersistenceForSession;
    case WEBKIT_CREDENTIAL_PERSISTENCE_PERMANENT:
        return WebCore::CredentialPersistencePermanent;
    default:
        ASSERT_NOT_REACHED();
        return WebCore::CredentialPersistenceNone;
    }
}

WebKitCredential* webkitCredentialCreate(const WebCore::Credential& coreCredential)
{
    WebKitCredential* credential = static_cast<WebKitCredential*>(fastMalloc(sizeof(WebKitCredential)));
    new (credential) WebKitCredential(coreCredential);
    return credential;
}

const WebCore::Credential& webkitCredentialGetCredential(WebKitCredential* credential)
{
    ASSERT(credential);
    return credential->credential;
}

/**
 * webkit_credential_new:
 * @username: The username for the new credential
 * @password: The password for the new credential
 * @persistence: The #WebKitCredentialPersistence of the new credential
 *
 * Create a new credential from the provided username, password and persistence mode.
 *
 * Returns: (transfer full): A #WebKitCredential.
 *
 * Since: 2.2
 */
WebKitCredential* webkit_credential_new(const gchar* username, const gchar* password, WebKitCredentialPersistence persistence)
{
    g_return_val_if_fail(username, 0);
    g_return_val_if_fail(password, 0);

    return webkitCredentialCreate(WebCore::Credential(String::fromUTF8(username), String::fromUTF8(password), toWebCoreCredentialPersistence(persistence)));
}

/**
 * webkit_credential_copy:
 * @credential: a #WebKitCredential
 *
 * Make a copy of the #WebKitCredential.
 *
 * Returns: (transfer full): A copy of passed in #WebKitCredential
 *
 * Since: 2.2
 */
WebKitCredential* webkit_credential_copy(WebKitCredential* credential)
{
    g_return_val_if_fail(credential, 0);

    return webkitCredentialCreate(credential->credential);
}

/**
 * webkit_credential_free:
 * @credential: A #WebKitCredential
 *
 * Free the #WebKitCredential.
 *
 * Since: 2.2
 */
void webkit_credential_free(WebKitCredential* credential)
{
    g_return_if_fail(credential);

    credential->~WebKitCredential();
    fastFree(credential);
}

/**
 * webkit_credential_get_username:
 * @credential: a #WebKitCredential
 *
 * Get the username currently held by this #WebKitCredential.
 *
 * Returns: The username stored in the #WebKitCredential.
 *
 * Since: 2.2
 */
const gchar* webkit_credential_get_username(WebKitCredential* credential)
{
    g_return_val_if_fail(credential, 0);

    if (credential->username.isNull())
        credential->username = credential->credential.user().utf8();
    return credential->username.data();
}

/**
 * webkit_credential_get_password:
 * @credential: a #WebKitCredential
 *
 * Get the password currently held by this #WebKitCredential.
 *
 * Returns: The password stored in the #WebKitCredential.
 *
 * Since: 2.2
 */
const gchar* webkit_credential_get_password(WebKitCredential* credential)
{
    g_return_val_if_fail(credential, 0);

    if (credential->password.isNull())
        credential->password = credential->credential.password().utf8();
    return credential->password.data();
}

/**
 * webkit_credential_has_password:
 * @credential: a #WebKitCredential
 *
 * Determine whether this credential has a password stored.
 *
 * Returns: %TRUE if the credential has a password or %FALSE otherwise.
 *
 * Since: 2.2
 */
gboolean webkit_credential_has_password(WebKitCredential* credential)
{
    g_return_val_if_fail(credential, FALSE);

    return credential->credential.hasPassword();
}

/**
 * webkit_credential_get_persistence:
 * @credential: a #WebKitCredential
 *
 * Get the persistence mode currently held by this #WebKitCredential.
 *
 * Returns: The #WebKitCredentialPersistence stored in the #WebKitCredential.
 *
 * Since: 2.2
 */
WebKitCredentialPersistence webkit_credential_get_persistence(WebKitCredential* credential)
{
    g_return_val_if_fail(credential, WEBKIT_CREDENTIAL_PERSISTENCE_NONE);

    return toWebKitCredentialPersistence(credential->credential.persistence());
}
