/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
#include "Authenticator.h"

#if ENABLE(WEB_AUTHN)

#include <AuthenticatorAttestationResponse.h>
#include <wtf/CurrentTime.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

Authenticator& Authenticator::singleton()
{
    static NeverDestroyed<Authenticator> authenticator;
    return authenticator;
}

ExceptionOr<Vector<uint8_t>> Authenticator::makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions::RpEntity&, const PublicKeyCredentialCreationOptions::UserEntity& user, const Vector<PublicKeyCredentialCreationOptions::Parameters>&, const Vector<PublicKeyCredentialDescriptor>&) const
{
    // The followings is just a dummy implementaion to support initial development.
    // User cancellation is effecively NotAllowedError.
    if (user.displayName == "John")
        return Exception { NotAllowedError };

    // Fill all parts before CredentialID with 0x00
    Vector<uint8_t> attestationObject(43, 0x00);
    // Fill length of CredentialID: 1 Byte
    attestationObject.append(0x00);
    attestationObject.append(0x01);
    // Fill CredentialID: 255
    attestationObject.append(0xff);
    // Append clientDataJsonHash
    attestationObject.appendVector(hash);
    return WTFMove(attestationObject);
}

ExceptionOr<Authenticator::AssertionReturnBundle> Authenticator::getAssertion(const String&, const Vector<uint8_t>& hash, const Vector<PublicKeyCredentialDescriptor>& allowCredentialIds) const
{
    // The followings is just a dummy implementaion to support initial development.
    // User cancellation is effecively NotAllowedError.
    if (!allowCredentialIds.isEmpty())
        return Exception { NotAllowedError };

    // FIXME: Delay processing for 0.1 seconds to simulate a timeout condition. This code will be removed
    // when the full test infrastructure is set up.
    WTF::sleep(0.1);

    // Fill all parts with hash
    return AssertionReturnBundle(ArrayBuffer::create(hash.data(), hash.size()), ArrayBuffer::create(hash.data(), hash.size()), ArrayBuffer::create(hash.data(), hash.size()), ArrayBuffer::create(hash.data(), hash.size()));
}

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
