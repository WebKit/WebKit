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

#include <wtf/CompletionHandler.h>
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS LAContext;

namespace WebKit {

// Local authenticators normally doesn't need to establish connections
// between the platform and themselves as they are attached.
// However, such abstraction is still provided to isolate operations
// that are not allowed in auto test environment such that some mocking
// mechnism can override them.
class LocalConnection {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(LocalConnection);
public:
    enum class UserConsent {
        No,
        Yes
    };

    using AttestationCallback = CompletionHandler<void(SecKeyRef, NSArray *, NSError *)>;
    using UserConsentCallback = CompletionHandler<void(UserConsent)>;
    using UserConsentContextCallback = CompletionHandler<void(UserConsent, LAContext *)>;

    LocalConnection() = default;
    virtual ~LocalConnection() = default;

    // Overrided by MockLocalConnection.
    virtual void getUserConsent(const String& reason, UserConsentCallback&&) const;
    virtual void getUserConsent(const String& reason, SecAccessControlRef, UserConsentContextCallback&&) const;
    virtual void getAttestation(const String& rpId, const String& username, const Vector<uint8_t>& hash, AttestationCallback&&) const;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN)
