/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

#if ENABLE(ENCRYPTED_MEDIA)

#include "MediaKeyMessageType.h"
#include "MediaKeySessionType.h"
#include "MediaKeyStatus.h"
#include <utility>
#include <wtf/Forward.h>
#include <wtf/Optional.h>
#include <wtf/RefCounted.h>
#include <wtf/Vector.h>

namespace WebCore {

struct MediaKeySystemConfiguration;
class SharedBuffer;

class CDMInstance : public RefCounted<CDMInstance> {
public:
    virtual ~CDMInstance() { }

    enum SuccessValue {
        Failed,
        Succeeded,
    };

    using LicenseType = MediaKeySessionType;
    using KeyStatus = MediaKeyStatus;
    using MessageType = MediaKeyMessageType;

    virtual SuccessValue initializeWithConfiguration(const MediaKeySystemConfiguration&) = 0;
    virtual SuccessValue setDistinctiveIdentifiersAllowed(bool) = 0;
    virtual SuccessValue setPersistentStateAllowed(bool) = 0;
    virtual SuccessValue setServerCertificate(Ref<SharedBuffer>&&) = 0;

    using LicenseCallback = Function<void(Ref<SharedBuffer>&& message, const String& sessionId, bool needsIndividualization, SuccessValue succeeded)>;
    virtual void requestLicense(LicenseType, const AtomicString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback) = 0;

    using KeyStatusVector = Vector<std::pair<Ref<SharedBuffer>, KeyStatus>>;
    using Message = std::pair<MessageType, Ref<SharedBuffer>>;
    using LicenseUpdateCallback = Function<void(bool sessionWasClosed, std::optional<KeyStatusVector>&& changedKeys, std::optional<double>&& changedExpiration, std::optional<Message>&& message, SuccessValue succeeded)>;
    virtual void updateLicense(const String& sessionId, LicenseType, const SharedBuffer& response, LicenseUpdateCallback) = 0;

    enum class SessionLoadFailure {
        None,
        NoSessionData,
        MismatchedSessionType,
        QuotaExceeded,
        Other,
    };

    using LoadSessionCallback = Function<void(std::optional<KeyStatusVector>&&, std::optional<double>&&, std::optional<Message>&&, SuccessValue, SessionLoadFailure)>;
    virtual void loadSession(LicenseType, const String& sessionId, const String& origin, LoadSessionCallback) = 0;

    using CloseSessionCallback = Function<void()>;
    virtual void closeSession(const String& sessionId, CloseSessionCallback) = 0;

    using RemoveSessionDataCallback = Function<void(KeyStatusVector&&, std::optional<Ref<SharedBuffer>>&&, SuccessValue)>;
    virtual void removeSessionData(const String& sessionId, LicenseType, RemoveSessionDataCallback) = 0;

    virtual void storeRecordOfKeyUsage(const String& sessionId) = 0;
};

}

#endif
