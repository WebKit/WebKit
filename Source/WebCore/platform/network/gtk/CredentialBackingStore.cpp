/*
 * Copyright (C) 2012 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY IGALIA S.L. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "CredentialBackingStore.h"

#define SECRET_WITH_UNSTABLE 1
#define SECRET_API_SUBJECT_TO_CHANGE 1
#include "AuthenticationChallenge.h"
#include "GRefPtrGtk.h"
#include <glib/gi18n-lib.h>
#include <libsecret/secret.h>
#include <libsoup/soup.h>
#include <wtf/gobject/GOwnPtr.h>
#include <wtf/text/CString.h>

namespace WebCore {

CredentialBackingStore& credentialBackingStore()
{
    DEFINE_STATIC_LOCAL(CredentialBackingStore, backingStore, ());
    return backingStore;
}

static GRefPtr<GHashTable> createAttributeHashTableFromChallenge(const AuthenticationChallenge& challenge, const Credential& credential = Credential())
{
    SoupURI* uri = soup_message_get_uri(challenge.soupMessage());
    GRefPtr<GHashTable> attributes = adoptGRef(secret_attributes_build(
        SECRET_SCHEMA_COMPAT_NETWORK,
        "domain", soup_auth_get_realm(challenge.soupAuth()),
        "server", uri->host,
        "protocol", uri->scheme,
        "authtype", soup_auth_get_scheme_name(challenge.soupAuth()),
        "port", uri->port,
        NULL));
    if (credential.isEmpty())
        return attributes;

    g_hash_table_insert(attributes.get(), g_strdup("user"), g_strdup(credential.user().utf8().data()));
    return attributes;
}

Credential CredentialBackingStore::credentialForChallenge(const AuthenticationChallenge& challenge)
{
    GOwnPtr<GList> elements(secret_service_search_sync(
        0, // The default SecretService.
        SECRET_SCHEMA_COMPAT_NETWORK,
        createAttributeHashTableFromChallenge(challenge).get(),
        static_cast<SecretSearchFlags>(SECRET_SEARCH_UNLOCK | SECRET_SEARCH_LOAD_SECRETS), // The default behavior is to only return the most recent item.
        0, // cancellable
        0)); // error
    if (!elements || !elements->data)
        return Credential();

    GRefPtr<SecretItem> secretItem = adoptGRef(static_cast<SecretItem*>(elements->data));
    GRefPtr<GHashTable> attributes = adoptGRef(secret_item_get_attributes(secretItem.get()));
    String user = String::fromUTF8(static_cast<const char*>(g_hash_table_lookup(attributes.get(), "user")));
    if (user.isEmpty())
        return Credential();

    size_t length;
    GRefPtr<SecretValue> secretValue = adoptGRef(secret_item_get_secret(secretItem.get()));
    const char* passwordData = secret_value_get(secretValue.get(), &length);
    String password = String::fromUTF8(passwordData, length);

    return Credential(user, password, CredentialPersistencePermanent);
}

void CredentialBackingStore::storeCredentialsForChallenge(const AuthenticationChallenge& challenge, const Credential& credential)
{
    CString utf8Password = credential.password().utf8();
    GRefPtr<SecretValue> newSecretValue = adoptGRef(secret_value_new(utf8Password.data(), utf8Password.length(), "text/plain"));

    secret_service_store(
        0, // The default SecretService.
        SECRET_SCHEMA_COMPAT_NETWORK,
        createAttributeHashTableFromChallenge(challenge, credential).get(),
        SECRET_COLLECTION_DEFAULT,
        _("WebKitGTK+ password"),
        newSecretValue.get(),
        0, // cancellable
        0, // callback
        0); // data
}

} // namespace WebCore
