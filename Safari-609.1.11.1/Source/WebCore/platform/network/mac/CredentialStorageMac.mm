/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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
#include "CredentialStorage.h"

#include "AuthenticationMac.h"
#include "Credential.h"
#include "ProtectionSpace.h"

namespace WebCore {

Credential CredentialStorage::getFromPersistentStorage(const ProtectionSpace& protectionSpace)
{
    NSURLCredential *credential = [[NSURLCredentialStorage sharedCredentialStorage] defaultCredentialForProtectionSpace:protectionSpace.nsSpace()];
    return credential ? Credential(credential) : Credential();
}

HashSet<SecurityOriginData> CredentialStorage::originsWithSessionCredentials()
{
    HashSet<SecurityOriginData> origins;
    auto allCredentials = [[NSURLCredentialStorage sharedCredentialStorage] allCredentials];
    for (NSURLProtectionSpace* key in allCredentials.keyEnumerator) {
        for (NSURLProtectionSpace* space in allCredentials) {
            auto credentials = allCredentials[space];
            for (NSString* user in credentials) {
                if (credentials[user].persistence == NSURLCredentialPersistenceForSession) {
                    origins.add(WebCore::SecurityOriginData { String(key.protocol), String(key.host), key.port });
                    break;
                }
            }
        }
    }
    return origins;
}

void CredentialStorage::removeSessionCredentialsWithOrigins(const Vector<SecurityOriginData>& origins)
{
    auto sharedStorage = [NSURLCredentialStorage sharedCredentialStorage];
    auto allCredentials = [sharedStorage allCredentials];
    for (auto& origin : origins) {
        for (NSURLProtectionSpace* space in allCredentials) {
            if (origin.protocol == String(space.protocol)
                && origin.host == String(space.host)
                && origin.port
                && *origin.port == space.port) {
                    auto credentials = allCredentials[space];
                    for (NSString* user in credentials) {
                        auto credential = credentials[user];
                        if (credential.persistence == NSURLCredentialPersistenceForSession)
                            [sharedStorage removeCredential:credential forProtectionSpace:space];
                }
            }
        }
    }
}

void CredentialStorage::clearSessionCredentials()
{
    auto sharedStorage = [NSURLCredentialStorage sharedCredentialStorage];
    auto allCredentials = [sharedStorage allCredentials];
    for (NSURLProtectionSpace* space in allCredentials.keyEnumerator) {
        auto credentials = allCredentials[space];
        for (NSString* user in credentials) {
            auto credential = credentials[user];
            if (credential.persistence == NSURLCredentialPersistenceForSession)
                [sharedStorage removeCredential:credential forProtectionSpace:space];
        }
    }
}

} // namespace WebCore
