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

#pragma once

#if ENABLE(WEB_AUTHN)

#include "ExceptionOr.h"
#include "PublicKeyCredentialCreationOptions.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

// FIXME(181946): Consider moving all static methods from PublicKeyCredential to here and making this
// as an authenticator manager that controls all authenticator activities, mostly likely asnyc
// for attestations.
class Authenticator {
    WTF_MAKE_NONCOPYABLE(Authenticator);
    friend class NeverDestroyed<Authenticator>;
public:
    // FIXME(181946): After moving all static methods from PublicKeyCredential to here, we will probably
    // return PublicKeyCredential directly and get rid of the following return type.
    struct AssertionReturnBundle {
        AssertionReturnBundle(Ref<ArrayBuffer>&& id, Ref<ArrayBuffer>&& data, Ref<ArrayBuffer>&& sig, Ref<ArrayBuffer>&& handle)
            : credentialID(WTFMove(id))
            , authenticatorData(WTFMove(data))
            , signature(WTFMove(sig))
            , userHandle(WTFMove(handle))
        {
        }

        Ref<ArrayBuffer> credentialID;
        Ref<ArrayBuffer> authenticatorData;
        Ref<ArrayBuffer> signature;
        Ref<ArrayBuffer> userHandle;
    };

    static Authenticator& singleton();

    // Omit requireResidentKey, requireUserPresence, and requireUserVerification as we always provide resident keys and require user verification.
    ExceptionOr<Vector<uint8_t>> makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions::RpEntity&, const PublicKeyCredentialCreationOptions::UserEntity&, const Vector<PublicKeyCredentialCreationOptions::Parameters>&, const Vector<PublicKeyCredentialDescriptor>& excludeCredentialIds) const;
    ExceptionOr<AssertionReturnBundle> getAssertion(const String& rpId, const Vector<uint8_t>& hash, const Vector<PublicKeyCredentialDescriptor>& allowCredentialIds) const;

private:
    Authenticator() = default;
};

} // namespace WebCore

#endif // ENABLE(WEB_AUTHN)
