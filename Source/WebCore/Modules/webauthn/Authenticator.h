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

#include "ExceptionOr.h"
#include "PublicKeyCredentialCreationOptions.h"
#include <wtf/Forward.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

// FIXME: Consider moving all static methods from PublicKeyCredential to here and making this
// as an authenticator manager that controls all authenticator activities, mostly likely asnyc
// for attestations.
class Authenticator {
    WTF_MAKE_NONCOPYABLE(Authenticator);
    friend class NeverDestroyed<Authenticator>;
public:
    static Authenticator& singleton();

    // Omit requireResidentKey, requireUserPresence, and requireUserVerification as we always provide resident keys and require user verification.
    ExceptionOr<Vector<uint8_t>> makeCredential(const Vector<uint8_t>& hash, const PublicKeyCredentialCreationOptions::RpEntity&, const PublicKeyCredentialCreationOptions::UserEntity&, const Vector<PublicKeyCredentialCreationOptions::Parameters>&, const Vector<PublicKeyCredentialDescriptor>& excludeCredentialIds) const;

#if !COMPILER(MSVC)
private:
#endif
    Authenticator() = default;
};

} // namespace WebCore
