/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include "CoreIPCDate.h"
#include "CoreIPCNumber.h"
#include "CoreIPCSecTrust.h"
#include "CoreIPCString.h"

#include <wtf/ArgumentCoder.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS NSURLCredential;

namespace WebKit {

#if HAVE(WK_SECURE_CODING_NSURLCREDENTIAL)

enum class CoreIPCNSURLCredentialPersistence : uint8_t {
    None = 1,
    Session,
    Permanent,
    Synchronizable
};

enum class CoreIPCNSURLCredentialType : uint8_t {
    Password,
    ServerTrust,
    KerberosTicket,
    XMobileMeAuthToken,
    OAuth2
};

struct CoreIPCNSURLCredentialData {
    using Flags = std::pair<CoreIPCString, CoreIPCString>;
    using Attributes = std::pair<CoreIPCString, std::variant<CoreIPCNumber, CoreIPCString, CoreIPCDate>>;

    CoreIPCNSURLCredentialPersistence persistence { CoreIPCNSURLCredentialPersistence::None };
    CoreIPCNSURLCredentialType type { CoreIPCNSURLCredentialType::Password };
    std::optional<CoreIPCString> user;
    std::optional<CoreIPCString> password;
    std::optional<Vector<Attributes>> attributes;
    std::optional<CoreIPCString> identifier;
    std::optional<bool> useKeychain;
    CoreIPCSecTrust trust;
    std::optional<CoreIPCString> service;
    std::optional<Vector<Flags>> flags;
    std::optional<CoreIPCString> uuid;
    std::optional<CoreIPCString> appleID;
    std::optional<CoreIPCString> realm;
    std::optional<CoreIPCString> token;
};

class CoreIPCNSURLCredential {
    WTF_MAKE_TZONE_ALLOCATED(CoreIPCNSURLCredential);
public:
    CoreIPCNSURLCredential(NSURLCredential *);
    CoreIPCNSURLCredential(CoreIPCNSURLCredentialData&&);

    RetainPtr<id> toID() const;
private:
    friend struct IPC::ArgumentCoder<CoreIPCNSURLCredential, void>;
    CoreIPCNSURLCredentialData m_data;
};

#endif

#if !HAVE(WK_SECURE_CODING_NSURLCREDENTIAL) && !HAVE(DICTIONARY_SERIALIZABLE_NSURLCREDENTIAL)

class CoreIPCNSURLCredential {
public:
    CoreIPCNSURLCredential(NSURLCredential *);
    CoreIPCNSURLCredential(const RetainPtr<NSURLCredential>& credential)
        : CoreIPCNSURLCredential(credential.get()) { }
    CoreIPCNSURLCredential(RetainPtr<NSData>&& serializedBytes)
        : m_serializedBytes(WTFMove(serializedBytes)) { }

    RetainPtr<id> toID() const;

private:
    friend struct IPC::ArgumentCoder<CoreIPCNSURLCredential, void>;

    RetainPtr<NSData> m_serializedBytes;
};

#endif

} // namespace WebKit
