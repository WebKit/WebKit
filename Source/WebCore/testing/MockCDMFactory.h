/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "CDM.h"
#include "CDMFactory.h"
#include "CDMInstance.h"
#include "CDMInstanceSession.h"
#include "CDMPrivate.h"
#include "CDMSessionType.h"
#include "MediaKeyEncryptionScheme.h"
#include "MediaKeysRequirement.h"
#include <wtf/HashMap.h>
#include <wtf/RefCountedAndCanMakeWeakPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class MockCDMFactory : public RefCounted<MockCDMFactory>, public CDMFactory {
public:
    static Ref<MockCDMFactory> create() { return adoptRef(*new MockCDMFactory); }
    static void unregisterAllMockFactories();
    ~MockCDMFactory();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    const Vector<RefPtr<StringImpl>>& supportedDataTypes() const LIFETIME_BOUND { return m_supportedDataTypes; }
    void setSupportedDataTypes(Vector<RefPtr<StringImpl>>&&);

    const Vector<MediaKeySessionType>& supportedSessionTypes() const LIFETIME_BOUND { return m_supportedSessionTypes; }
    void setSupportedSessionTypes(Vector<MediaKeySessionType>&& types) { m_supportedSessionTypes = WTF::move(types); }

    const Vector<RefPtr<StringImpl>>& supportedRobustness() const LIFETIME_BOUND { return m_supportedRobustness; }
    void setSupportedRobustness(Vector<RefPtr<StringImpl>>&& supportedRobustness) { m_supportedRobustness = WTF::move(supportedRobustness); }

    MediaKeysRequirement distinctiveIdentifiersRequirement() const { return m_distinctiveIdentifiersRequirement; }
    void setDistinctiveIdentifiersRequirement(MediaKeysRequirement requirement) { m_distinctiveIdentifiersRequirement = requirement; }

    MediaKeysRequirement persistentStateRequirement() const { return m_persistentStateRequirement; }
    void setPersistentStateRequirement(MediaKeysRequirement requirement) { m_persistentStateRequirement = requirement; }

    bool canCreateInstances() const { return m_canCreateInstances; }
    void setCanCreateInstances(bool flag) { m_canCreateInstances = flag; }

    bool supportsServerCertificates() const { return m_supportsServerCertificates; }
    void setSupportsServerCertificates(bool flag) { m_supportsServerCertificates = flag; }

    bool supportsSessions() const { return m_supportsSessions; }
    void setSupportsSessions(bool flag) { m_supportsSessions = flag; }

    const Vector<MediaKeyEncryptionScheme>& supportedEncryptionSchemes() const LIFETIME_BOUND { return m_supportedEncryptionSchemes; }
    void setSupportedEncryptionSchemes(Vector<MediaKeyEncryptionScheme>&& schemes) { m_supportedEncryptionSchemes = WTF::move(schemes); }

    const Vector<RefPtr<StringImpl>>& unsupportedVideoCodecs() const LIFETIME_BOUND { return m_unsupportedVideoCodecs; }
    void setUnsupportedVideoCodecs(Vector<RefPtr<StringImpl>>&& codecs) { m_unsupportedVideoCodecs = WTF::move(codecs); }

    void unregister();

    bool hasSessionWithID(StringImpl* id);
    void removeSessionWithID(StringImpl* id);
    void addKeysToSessionWithID(StringImpl* id, Vector<Ref<SharedBuffer>>&&);
    const Vector<Ref<SharedBuffer>>* keysForSessionWithID(StringImpl* id) const;
    Vector<Ref<SharedBuffer>> removeKeysFromSessionWithID(StringImpl* id);
    size_t keyCountForSession(StringImpl* id) const;

    void setSessionType(StringImpl* id, CDMSessionType type) { m_sessionTypes.set(id, type); }
    std::optional<CDMSessionType> sessionType(StringImpl* id) const;

    double expirationOnUpdate() const { return m_expirationOnUpdate; }
    void setExpirationOnUpdate(double t) { m_expirationOnUpdate = t; }

private:
    MockCDMFactory();
    std::unique_ptr<CDMPrivate> createCDM(StringImpl*, StringImpl* mediaKeysHashSalt, const CDMPrivateClient&) final;
    bool supportsKeySystem(StringImpl*) final;

    MediaKeysRequirement m_distinctiveIdentifiersRequirement { MediaKeysRequirement::Optional };
    MediaKeysRequirement m_persistentStateRequirement { MediaKeysRequirement::Optional };
    Vector<RefPtr<StringImpl>> m_supportedDataTypes;
    Vector<MediaKeySessionType> m_supportedSessionTypes;
    Vector<RefPtr<StringImpl>> m_supportedRobustness;
    Vector<MediaKeyEncryptionScheme> m_supportedEncryptionSchemes;
    Vector<RefPtr<StringImpl>> m_unsupportedVideoCodecs;
    bool m_registered { true };
    bool m_canCreateInstances { true };
    bool m_supportsServerCertificates { true };
    bool m_supportsSessions { true };
    HashMap<RefPtr<StringImpl>, Vector<Ref<SharedBuffer>>> m_sessions;
    HashMap<RefPtr<StringImpl>, CDMSessionType> m_sessionTypes;
    double m_expirationOnUpdate { std::numeric_limits<double>::quiet_NaN() };
};

class MockCDM : public CDMPrivate {
    WTF_MAKE_TZONE_ALLOCATED(MockCDM);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(MockCDM);
public:
    MockCDM(WeakPtr<MockCDMFactory>, StringImpl*);

    RefPtr<MockCDMFactory> factory() const { return m_factory.get(); }
    const RefPtr<StringImpl>& mediaKeysHashSalt() const LIFETIME_BOUND { return m_mediaKeysHashSalt; }

private:
    friend class MockCDMInstance;

    Vector<RefPtr<StringImpl>> supportedInitDataTypes() const final;
    Vector<RefPtr<StringImpl>> supportedRobustnesses() const final;
    bool supportsConfiguration(const MediaKeySystemConfiguration&) const final;
    bool supportsConfigurationWithRestrictions(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const final;
    bool supportsSessionTypeWithConfiguration(const MediaKeySessionType&, const MediaKeySystemConfiguration&) const final;
    MediaKeysRequirement distinctiveIdentifiersRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const final;
    MediaKeysRequirement persistentStateRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const final;
    bool distinctiveIdentifiersAreUniquePerOriginAndClearable(const MediaKeySystemConfiguration&) const final;
    RefPtr<CDMInstance> createInstance() final;
    void loadAndInitialize() final;
    bool supportsServerCertificates() const final;
    bool supportsSessions() const final;
    bool supportsInitData(StringImpl*, const SharedBuffer&) const final;
    RefPtr<SharedBuffer> sanitizeResponse(const SharedBuffer&) const final;
    std::optional<RefPtr<StringImpl>> sanitizeSessionId(StringImpl*) const final;

    WeakPtr<MockCDMFactory> m_factory;
    RefPtr<StringImpl> m_mediaKeysHashSalt;
};

class MockCDMInstance : public CDMInstance, public CanMakeWeakPtr<MockCDMInstance> {
public:
    NODELETE static Ref<MockCDMInstance> create(MockCDM&);

    NODELETE RefPtr<MockCDMFactory> factory() const;
    bool distinctiveIdentifiersAllowed() const { return m_distinctiveIdentifiersAllowed; }
    bool persistentStateAllowed() const { return m_persistentStateAllowed; }

private:
    explicit MockCDMInstance(MockCDM&);

    ImplementationType implementationType() const final { return ImplementationType::Mock; }
    void initializeWithConfiguration(const MediaKeySystemConfiguration&, AllowDistinctiveIdentifiers, AllowPersistentState, SuccessCallback&&) final;
    void setServerCertificate(Ref<SharedBuffer>&&, SuccessCallback&&) final;
    void setStorageDirectory(StringImpl*) final;
    const RefPtr<StringImpl>& keySystem() const final;
    RefPtr<CDMInstanceSession> createSession() final;

    WeakPtr<MockCDM> m_cdm;
    bool m_distinctiveIdentifiersAllowed { true };
    bool m_persistentStateAllowed { true };
};

class MockCDMInstanceSession : public CDMInstanceSession {
public:
    explicit MockCDMInstanceSession(WeakPtr<MockCDMInstance>&&);

    RefPtr<MockCDMFactory> factory() const;

private:
    void requestLicense(LicenseType, KeyGroupingStrategy, StringImpl* initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&&) final;
    void updateLicense(StringImpl*, LicenseType, Ref<SharedBuffer>&&, LicenseUpdateCallback&&) final;
    void loadSession(LicenseType, StringImpl*, StringImpl*, LoadSessionCallback&&) final;
    void closeSession(StringImpl*, CloseSessionCallback&&) final;
    void removeSessionData(StringImpl*, LicenseType, RemoveSessionDataCallback&&) final;
    void storeRecordOfKeyUsage(StringImpl*) final;

    WeakPtr<MockCDMInstance> m_instance;
};

}

#endif
