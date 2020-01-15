/*
 * Copyright (C) 2016 Metrological Group B.V.
 * Copyright (C) 2016 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMFactory.h"
#include "CDMInstance.h"
#include "CDMInstanceSession.h"
#include "CDMPrivate.h"
#include "SharedBuffer.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

class CDMFactoryClearKey final : public CDMFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static CDMFactoryClearKey& singleton();

    virtual ~CDMFactoryClearKey();

    std::unique_ptr<CDMPrivate> createCDM(const String&) final;
    bool supportsKeySystem(const String&) final;

private:
    friend class NeverDestroyed<CDMFactoryClearKey>;
    CDMFactoryClearKey();
};

class CDMPrivateClearKey final : public CDMPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CDMPrivateClearKey();
    virtual ~CDMPrivateClearKey();

    bool supportsInitDataType(const AtomString&) const final;
    bool supportsConfiguration(const CDMKeySystemConfiguration&) const final;
    bool supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration&, const CDMRestrictions&) const final;
    bool supportsSessionTypeWithConfiguration(CDMSessionType&, const CDMKeySystemConfiguration&) const final;
    bool supportsRobustness(const String&) const final;
    CDMRequirement distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const final;
    CDMRequirement persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const final;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const final;
    RefPtr<CDMInstance> createInstance() final;
    void loadAndInitialize() final;
    bool supportsServerCertificates() const final;
    bool supportsSessions() const final;
    bool supportsInitData(const AtomString&, const SharedBuffer&) const final;
    RefPtr<SharedBuffer> sanitizeResponse(const SharedBuffer&) const final;
    Optional<String> sanitizeSessionId(const String&) const final;
};

class ProxyCDMClearKey final : public ProxyCDM {
public:
    struct Key {
        CDMInstanceSession::KeyStatus status;
        String keyIDData;
        String keyValueData;
    };

    virtual ~ProxyCDMClearKey() = default;
    const Vector<Key> isolatedKeys() const;
private:
    mutable Lock m_keysMutex;
};

class CDMInstanceClearKey final : public CDMInstance, public CanMakeWeakPtr<CDMInstanceClearKey> {
public:
    CDMInstanceClearKey();
    virtual ~CDMInstanceClearKey();

    ImplementationType implementationType() const final { return ImplementationType::ClearKey; }

    SuccessValue initializeWithConfiguration(const CDMKeySystemConfiguration&) final;
    SuccessValue setDistinctiveIdentifiersAllowed(bool) final;
    SuccessValue setPersistentStateAllowed(bool) final;
    SuccessValue setServerCertificate(Ref<SharedBuffer>&&) final;
    SuccessValue setStorageDirectory(const String&) final;
    const String& keySystem() const final;
    RefPtr<CDMInstanceSession> createSession() final;

    struct Key {
        CDMInstanceSession::KeyStatus status;
        RefPtr<SharedBuffer> keyIDData;
        RefPtr<SharedBuffer> keyValueData;

        String keyIDAsString() const;
        String keyValueAsString() const;

        bool hasSameKeyValue(const Key &other)
        {
            ASSERT(keyValueData);
            ASSERT(other.keyValueData);
            return *keyValueData == *other.keyValueData;
        }

        // Two keys are equal if they have the same ID, ignoring key value and status.
        friend bool operator==(const Key &k1, const Key &k2);
        // Key's are ordered by their IDs, first by size, then by contents.
        friend bool operator<(const Key &k1, const Key &k2);

        friend bool operator!=(const Key &k1, const Key &k2) { return !(operator==(k1, k2)); }
        friend bool operator>(const Key &k1, const Key &k2) { return !operator==(k1, k2) && !operator<(k1, k2); }
        friend bool operator<=(const Key &k1, const Key &k2) { return !operator>(k1, k2); }
        friend bool operator>=(const Key &k1, const Key &k2) { return !operator<(k1, k2); }
    };

    RefPtr<ProxyCDM> proxyCDM() const final { return m_proxyCDM; }

private:
    RefPtr<ProxyCDM> m_proxyCDM;
};

class CDMInstanceSessionClearKey final : public CDMInstanceSession, public CanMakeWeakPtr<CDMInstanceSessionClearKey> {
public:
    void requestLicense(LicenseType, const AtomString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&&) final;
    void updateLicense(const String&, LicenseType, const SharedBuffer&, LicenseUpdateCallback&&) final;
    void loadSession(LicenseType, const String&, const String&, LoadSessionCallback&&) final;
    void closeSession(const String&, CloseSessionCallback&&) final;
    void removeSessionData(const String&, LicenseType, RemoveSessionDataCallback&&) final;
    void storeRecordOfKeyUsage(const String&) final;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CDM_INSTANCE(WebCore::CDMInstanceClearKey, WebCore::CDMInstance::ImplementationType::ClearKey);

#endif // ENABLE(ENCRYPTED_MEDIA)
