/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef CredentialCocoa_h
#define CredentialCocoa_h

#include "CredentialBase.h"
#include <Security/SecBase.h>
#include <wtf/RetainPtr.h>

// FIXME: Remove this macro once it is not used in WebKit.
#define CERTIFICATE_CREDENTIALS_SUPPORTED 1

namespace WebCore {

enum CredentialType {
    CredentialTypePassword,
    CredentialTypeClientCertificate
};

class Credential : public CredentialBase {
public:
    Credential()
        : CredentialBase()
        , m_type(CredentialTypePassword)
    {
    }

    Credential(const String& user, const String& password, CredentialPersistence persistence)
        : CredentialBase(user, password, persistence)
        , m_type(CredentialTypePassword)
    {
    }

    Credential(const Credential& original, CredentialPersistence persistence)
        : CredentialBase(original, persistence)
        , m_type(original.m_type)
    {
    }

    Credential(SecIdentityRef, CFArrayRef certificates, CredentialPersistence);

    bool isEmpty() const;

    SecIdentityRef identity() const;
    CFArrayRef certificates() const;
    CredentialType type() const;

    static bool platformCompare(const Credential&, const Credential&);

private:
    RetainPtr<SecIdentityRef> m_identity;
    RetainPtr<CFArrayRef> m_certificates;
    CredentialType m_type;
};

} // namespace WebCore

#endif // CredentialCocoa_h
