/*
 * Copyright (C) 2020 Metrological Group B.V.
 * Copyright (C) 2020 Igalia S.L.
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

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)

#include "CDMFactory.h"
#include "CDMInstanceSession.h"
#include "CDMOpenCDMTypes.h"
#include "CDMPrivate.h"
#include "CDMProxy.h"
#include "GStreamerEMEUtilities.h"
#include "MediaKeyStatus.h"
#include "SharedBuffer.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

namespace Thunder {

struct ThunderSystemDeleter {
    OpenCDMError operator()(OpenCDMSystem* ptr) const { return opencdm_destruct_system(ptr); }
};

using UniqueThunderSystem = std::unique_ptr<OpenCDMSystem, ThunderSystemDeleter>;

} // namespace Thunder

class CDMFactoryThunder final : public CDMFactory, public CDMProxyFactory {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static CDMFactoryThunder& singleton();

    virtual ~CDMFactoryThunder() = default;

    std::unique_ptr<CDMPrivate> createCDM(const String&) final;
    RefPtr<CDMProxy> createCDMProxy(const String&) final;
    bool supportsKeySystem(const String&) final;
    const Vector<String>& supportedKeySystems() const;

private:
    friend class NeverDestroyed<CDMFactoryThunder>;
    CDMFactoryThunder() = default;
};

class CDMPrivateThunder final : public CDMPrivate {
    WTF_MAKE_FAST_ALLOCATED;
public:
    CDMPrivateThunder(const String& keySystem);
    virtual ~CDMPrivateThunder() = default;

    Vector<AtomString> supportedInitDataTypes() const final;
    bool supportsConfiguration(const CDMKeySystemConfiguration&) const final;
    bool supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration& configuration, const CDMRestrictions&) const final
    {
        return supportsConfiguration(configuration);
    }
    bool supportsSessionTypeWithConfiguration(const CDMSessionType&, const CDMKeySystemConfiguration& configuration) const final
    {
        return supportsConfiguration(configuration);
    }
    Vector<AtomString> supportedRobustnesses() const final;
    CDMRequirement distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const final;
    CDMRequirement persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const final;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const final;
    RefPtr<CDMInstance> createInstance() final;
    void loadAndInitialize() final;
    bool supportsServerCertificates() const final;
    bool supportsSessions() const final;
    bool supportsInitData(const AtomString&, const SharedBuffer&) const final;
    RefPtr<SharedBuffer> sanitizeInitData(const AtomString& initDataType, const SharedBuffer& initData) const final;
    RefPtr<SharedBuffer> sanitizeResponse(const SharedBuffer&) const final;
    std::optional<String> sanitizeSessionId(const String&) const final;

private:
    String m_keySystem;
    Thunder::UniqueThunderSystem m_thunderSystem;
};

class CDMInstanceThunder final : public CDMInstanceProxy {
public:
    CDMInstanceThunder(const String& keySystem);
    virtual ~CDMInstanceThunder() = default;

    // CDMInstance
    ImplementationType implementationType() const final { return ImplementationType::Thunder; }
    void initializeWithConfiguration(const CDMKeySystemConfiguration&, AllowDistinctiveIdentifiers, AllowPersistentState, SuccessCallback&&) final;
    void setServerCertificate(Ref<SharedBuffer>&&, SuccessCallback&&) final;
    void setStorageDirectory(const String&) final;
    const String& keySystem() const final { return m_keySystem; }
    RefPtr<CDMInstanceSession> createSession() final;

    OpenCDMSystem& thunderSystem() const { return *m_thunderSystem.get(); };

private:
    Thunder::UniqueThunderSystem m_thunderSystem;
    String m_keySystem;
};

class CDMInstanceSessionThunder final : public CDMInstanceSessionProxy {
public:
    CDMInstanceSessionThunder(CDMInstanceThunder&);

    void requestLicense(LicenseType, const AtomString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&&) final;
    void updateLicense(const String&, LicenseType, Ref<SharedBuffer>&&, LicenseUpdateCallback&&) final;
    void loadSession(LicenseType, const String&, const String&, LoadSessionCallback&&) final;
    void closeSession(const String&, CloseSessionCallback&&) final;
    void removeSessionData(const String&, LicenseType, RemoveSessionDataCallback&&) final;
    void storeRecordOfKeyUsage(const String&) final;

    bool isValid() const { return m_session && m_message && !m_message->isEmpty(); }

    void setClient(WeakPtr<CDMInstanceSessionClient>&& client) final { m_client = WTFMove(client); }
    void clearClient() final { m_client.clear(); }

private:
    CDMInstanceThunder* cdmInstanceThunder() const;

    using Notification = void (CDMInstanceSessionThunder::*)(RefPtr<WebCore::SharedBuffer>&&);
    using ChallengeGeneratedCallback = Function<void()>;
    using SessionChangedCallback = Function<void(bool, RefPtr<SharedBuffer>&&)>;

    void challengeGeneratedCallback(RefPtr<SharedBuffer>&&);
    void keyUpdatedCallback(KeyIDType&&);
    void keysUpdateDoneCallback();
    void errorCallback(RefPtr<SharedBuffer>&&);
    CDMInstanceSession::KeyStatus status(const KeyIDType&) const;
    void sessionFailure();

    // FIXME: Check all original uses of these attributes.
    String m_sessionID;
    KeyStore m_keyStore;
    bool m_doesKeyStoreNeedMerging { false };
    InitData m_initData;
    OpenCDMSessionCallbacks m_thunderSessionCallbacks { };
    BoxPtr<OpenCDMSession> m_session;
    RefPtr<SharedBuffer> m_message;
    bool m_needsIndividualization { false };
    Vector<ChallengeGeneratedCallback> m_challengeCallbacks;
    Vector<SessionChangedCallback> m_sessionChangedCallbacks;
    WeakPtr<CDMInstanceSessionClient> m_client;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_CDM_INSTANCE(WebCore::CDMInstanceThunder, WebCore::CDMInstance::ImplementationType::Thunder);

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)
