/*
 * Copyright (C) 2009, 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "CredentialCocoa.h"

namespace WebCore {

Credential::Credential(SecIdentityRef identity, CFArrayRef certificates, CredentialPersistence persistence)
    : CredentialBase(emptyString(), emptyString(), persistence)
    , m_identity(identity)
    , m_certificates(certificates)
    , m_type(CredentialTypeClientCertificate)
{
}

bool Credential::isEmpty() const
{
    if (m_type == CredentialTypeClientCertificate && (m_identity || m_certificates))
        return false;

    return CredentialBase::isEmpty();
}

SecIdentityRef Credential::identity() const
{
    return m_identity.get();
}
    
CFArrayRef Credential::certificates() const
{
    return m_certificates.get();
}
    
CredentialType Credential::type() const
{
    return m_type;
}

bool Credential::platformCompare(const Credential& a, const Credential& b)
{
    if (a.type() != CredentialTypeClientCertificate || b.type() != CredentialTypeClientCertificate)
        return a.type() == b.type();

    // FIXME: Is pointer comparison of the identity and certificates properties sufficient?
    if (a.identity() != b.identity())
        return false;
    if (a.certificates() != b.certificates())
        return false;

    return true;
}

} // namespace WebCore
