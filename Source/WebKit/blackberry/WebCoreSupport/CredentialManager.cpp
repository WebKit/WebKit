/*
 * Copyright (C) 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"

#if ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
#include "CredentialManager.h"

#include "CredentialBackingStore.h"
#include "CredentialTransformData.h"
#include "HTMLFormElement.h"
#include "KURL.h"
#include "Logging.h"
#include "PageClientBlackBerry.h"
#include "WebString.h"

using BlackBerry::WebKit::WebString;

namespace WebCore {

CredentialManager& credentialManager()
{
    static CredentialManager *credentialManager = 0;
    if (!credentialManager)
        credentialManager = new CredentialManager();
    return *credentialManager;
}

void CredentialManager::autofillAuthenticationChallenge(const ProtectionSpace& protectionSpace, WebString& username, WebString& password)
{
    if (CredentialBackingStore::instance()->hasNeverRemember(protectionSpace))
        return;

    Credential savedCredential = CredentialBackingStore::instance()->getLogin(protectionSpace);
    username = savedCredential.user();
    password = savedCredential.password();
}

void CredentialManager::autofillPasswordForms(PassRefPtr<HTMLCollection> docForms)
{
    ASSERT(docForms);

    RefPtr<HTMLCollection> forms = docForms;
    size_t sourceLength = forms->length();
    for (size_t i = 0; i < sourceLength; ++i) {
        Node* node = forms->item(i);
        if (node && node->isHTMLElement()) {
            CredentialTransformData data(static_cast<HTMLFormElement*>(node));
            if (!data.isValid() || !CredentialBackingStore::instance()->hasLogin(data.url(), data.protectionSpace()))
                continue;
            Credential savedCredential = CredentialBackingStore::instance()->getLogin(data.url());
            data.setCredential(savedCredential);
        }
    }
}

void CredentialManager::saveCredentialIfConfirmed(PageClientBlackBerry* pageClient, const CredentialTransformData& data)
{
    ASSERT(pageClient);

    if (!data.isValid() || data.credential().isEmpty() || CredentialBackingStore::instance()->hasNeverRemember(data.protectionSpace()))
        return;

    Credential savedCredential = CredentialBackingStore::instance()->getLogin(data.url());
    if (savedCredential == data.credential())
        return;

    PageClientBlackBerry::SaveCredentialType type = pageClient->notifyShouldSaveCredential(savedCredential.isEmpty());
    if (type == PageClientBlackBerry::SaveCredentialYes) {
        if (savedCredential.isEmpty())
            CredentialBackingStore::instance()->addLogin(data.url(), data.protectionSpace(), data.credential());
        else
            CredentialBackingStore::instance()->updateLogin(data.url(), data.protectionSpace(), data.credential());
    } else if (type == PageClientBlackBerry::SaveCredentialNeverForThisSite) {
        CredentialBackingStore::instance()->addNeverRemember(data.url(), data.protectionSpace());
        CredentialBackingStore::instance()->removeLogin(data.url(), data.protectionSpace());
    }
}

void CredentialManager::clearCredentials()
{
    CredentialBackingStore::instance()->clearLogins();
}

void CredentialManager::clearNeverRememberSites()
{
    CredentialBackingStore::instance()->clearNeverRemember();
}

} // namespace WebCore

#endif // ENABLE(BLACKBERRY_CREDENTIAL_PERSIST)
