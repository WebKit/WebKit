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

#include "config.h"
#include "MockCDMFactory.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "InitDataRegistry.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <wtf/Algorithms.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/UUID.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>

namespace WebCore {

MockCDMFactory::MockCDMFactory()
    : m_supportedSessionTypes({ MediaKeySessionType::Temporary, MediaKeySessionType::PersistentUsageRecord, MediaKeySessionType::PersistentLicense })
    , m_supportedEncryptionSchemes({ MediaKeyEncryptionScheme::cenc })
{
    CDMFactory::registerFactory(*this);
}

MockCDMFactory::~MockCDMFactory()
{
    unregister();
}

void MockCDMFactory::unregister()
{
    if (m_registered) {
        CDMFactory::unregisterFactory(*this);
        m_registered = false;
    }
}

bool MockCDMFactory::supportsKeySystem(const String& keySystem)
{
    return equalIgnoringASCIICase(keySystem, "org.webkit.mock");
}

void MockCDMFactory::addKeysToSessionWithID(const String& id, Vector<Ref<SharedBuffer>>&& keys)
{
    auto addResult = m_sessions.add(id, WTFMove(keys));
    if (addResult.isNewEntry)
        return;

    auto& value = addResult.iterator->value;
    for (auto& key : keys)
        value.append(WTFMove(key));
}

Vector<Ref<SharedBuffer>> MockCDMFactory::removeKeysFromSessionWithID(const String& id)
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return { };

    return WTFMove(it->value);
}

const Vector<Ref<SharedBuffer>>* MockCDMFactory::keysForSessionWithID(const String& id) const
{
    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return nullptr;
    return &it->value;
}

void MockCDMFactory::setSupportedDataTypes(Vector<String>&& types)
{
    m_supportedDataTypes.clear();
    for (auto& type : types)
        m_supportedDataTypes.append(type);
}

std::unique_ptr<CDMPrivate> MockCDMFactory::createCDM(const String&)
{
    return std::make_unique<MockCDM>(makeWeakPtr(*this));
}

MockCDM::MockCDM(WeakPtr<MockCDMFactory> factory)
    : m_factory(WTFMove(factory))
{
}

bool MockCDM::supportsInitDataType(const AtomicString& initDataType) const
{
    if (m_factory)
        return m_factory->supportedDataTypes().contains(initDataType);
    return false;
}

bool MockCDM::supportsConfiguration(const MediaKeySystemConfiguration& configuration) const
{
    auto capabilityHasSupportedEncryptionScheme = [&] (auto& capability) {
        if (capability.encryptionScheme)
            return m_factory->supportedEncryptionSchemes().contains(capability.encryptionScheme.value());
        return true;
    };

    if (!configuration.audioCapabilities.isEmpty() && !anyOf(configuration.audioCapabilities, capabilityHasSupportedEncryptionScheme))
        return false;

    if (!configuration.videoCapabilities.isEmpty() && !anyOf(configuration.videoCapabilities, capabilityHasSupportedEncryptionScheme))
        return false;

    return true;

}

bool MockCDM::supportsConfigurationWithRestrictions(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const
{
    // NOTE: Implement;
    return true;
}

bool MockCDM::supportsSessionTypeWithConfiguration(MediaKeySessionType& sessionType, const MediaKeySystemConfiguration&) const
{
    if (!m_factory || !m_factory->supportedSessionTypes().contains(sessionType))
        return false;

    // NOTE: Implement configuration checking;
    return true;
}

bool MockCDM::supportsRobustness(const String& robustness) const
{
    if (m_factory)
        return m_factory->supportedRobustness().contains(robustness);
    return false;
}

MediaKeysRequirement MockCDM::distinctiveIdentifiersRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const
{
    if (m_factory)
        return m_factory->distinctiveIdentifiersRequirement();
    return MediaKeysRequirement::Optional;
}

MediaKeysRequirement MockCDM::persistentStateRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const
{
    if (m_factory)
        return m_factory->persistentStateRequirement();
    return MediaKeysRequirement::Optional;
}

bool MockCDM::distinctiveIdentifiersAreUniquePerOriginAndClearable(const MediaKeySystemConfiguration&) const
{
    // NOTE: Implement;
    return true;
}

RefPtr<CDMInstance> MockCDM::createInstance()
{
    if (m_factory && !m_factory->canCreateInstances())
        return nullptr;
    return adoptRef(new MockCDMInstance(makeWeakPtr(*this)));
}

void MockCDM::loadAndInitialize()
{
    // No-op.
}

bool MockCDM::supportsServerCertificates() const
{
    return m_factory && m_factory->supportsServerCertificates();
}

bool MockCDM::supportsSessions() const
{
    return m_factory && m_factory->supportsSessions();
}

bool MockCDM::supportsInitData(const AtomicString& initDataType, const SharedBuffer& initData) const
{
    if (!supportsInitDataType(initDataType))
        return false;

    UNUSED_PARAM(initData);
    return true;
}

RefPtr<SharedBuffer> MockCDM::sanitizeResponse(const SharedBuffer& response) const
{
    if (!charactersAreAllASCII(reinterpret_cast<const LChar*>(response.data()), response.size()))
        return nullptr;

    Vector<String> responseArray = String(response.data(), response.size()).split(' ');

    if (!responseArray.contains(String("valid-response"_s)))
        return nullptr;

    return response.copy();
}

std::optional<String> MockCDM::sanitizeSessionId(const String& sessionId) const
{
    if (equalLettersIgnoringASCIICase(sessionId, "valid-loaded-session"))
        return sessionId;
    return std::nullopt;
}

MockCDMInstance::MockCDMInstance(WeakPtr<MockCDM> cdm)
    : m_cdm(cdm)
{
}

CDMInstance::SuccessValue MockCDMInstance::initializeWithConfiguration(const MediaKeySystemConfiguration& configuration)
{
    if (!m_cdm || !m_cdm->supportsConfiguration(configuration))
        return Failed;

    return Succeeded;
}

CDMInstance::SuccessValue MockCDMInstance::setDistinctiveIdentifiersAllowed(bool distinctiveIdentifiersAllowed)
{
    if (m_distinctiveIdentifiersAllowed == distinctiveIdentifiersAllowed)
        return Succeeded;

    auto* factory = m_cdm ? m_cdm->factory() : nullptr;

    if (!factory || (!distinctiveIdentifiersAllowed && factory->distinctiveIdentifiersRequirement() == MediaKeysRequirement::Required))
        return Failed;

    m_distinctiveIdentifiersAllowed = distinctiveIdentifiersAllowed;
    return Succeeded;
}

CDMInstance::SuccessValue MockCDMInstance::setPersistentStateAllowed(bool persistentStateAllowed)
{
    if (m_persistentStateAllowed == persistentStateAllowed)
        return Succeeded;

    MockCDMFactory* factory = m_cdm ? m_cdm->factory() : nullptr;

    if (!factory || (!persistentStateAllowed && factory->persistentStateRequirement() == MediaKeysRequirement::Required))
        return Failed;

    m_persistentStateAllowed = persistentStateAllowed;
    return Succeeded;
}

CDMInstance::SuccessValue MockCDMInstance::setServerCertificate(Ref<SharedBuffer>&& certificate)
{
    StringView certificateStringView(reinterpret_cast<const LChar*>(certificate->data()), certificate->size());

    if (equalIgnoringASCIICase(certificateStringView, "valid"))
        return Succeeded;
    return Failed;
}

CDMInstance::SuccessValue MockCDMInstance::setStorageDirectory(const String&)
{
    // On disk storage is unused; no-op.
    return Succeeded;
}

const String& MockCDMInstance::keySystem() const
{
    static const NeverDestroyed<String> s_keySystem = MAKE_STATIC_STRING_IMPL("org.webkit.mock");

    return s_keySystem;
}

RefPtr<CDMInstanceSession> MockCDMInstance::createSession()
{
    return adoptRef(new MockCDMInstanceSession(makeWeakPtr(*this)));
}

MockCDMInstanceSession::MockCDMInstanceSession(WeakPtr<MockCDMInstance>&& instance)
    : m_instance(WTFMove(instance))
{
}

void MockCDMInstanceSession::requestLicense(LicenseType licenseType, const AtomicString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    MockCDMFactory* factory = m_instance ? m_instance->factory() : nullptr;
    if (!factory) {
        callback(SharedBuffer::create(), emptyAtom(), false, SuccessValue::Failed);
        return;
    }

    if (!factory->supportedSessionTypes().contains(licenseType) || !factory->supportedDataTypes().contains(initDataType)) {
        callback(SharedBuffer::create(), emptyString(), false, SuccessValue::Failed);
        return;
    }

    auto keyIDs = InitDataRegistry::shared().extractKeyIDs(initDataType, initData);
    if (!keyIDs || keyIDs.value().isEmpty()) {
        callback(SharedBuffer::create(), emptyString(), false, SuccessValue::Failed);
        return;
    }

    String sessionID = createCanonicalUUIDString();
    factory->addKeysToSessionWithID(sessionID, WTFMove(keyIDs.value()));

    CString license { "license" };
    callback(SharedBuffer::create(license.data(), license.length()), sessionID, false, SuccessValue::Succeeded);
}

void MockCDMInstanceSession::updateLicense(const String& sessionID, LicenseType, const SharedBuffer& response, LicenseUpdateCallback&& callback)
{
    MockCDMFactory* factory = m_instance ? m_instance->factory() : nullptr;
    if (!factory) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, SuccessValue::Failed);
        return;
    }

    Vector<String> responseVector = String(response.data(), response.size()).split(' ');

    if (responseVector.contains(String("invalid-format"_s))) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, SuccessValue::Failed);
        return;
    }

    std::optional<KeyStatusVector> changedKeys;
    if (responseVector.contains(String("keys-changed"_s))) {
        const auto* keys = factory->keysForSessionWithID(sessionID);
        if (keys) {
            KeyStatusVector keyStatusVector;
            keyStatusVector.reserveInitialCapacity(keys->size());
            for (auto& key : *keys)
                keyStatusVector.uncheckedAppend({ key.copyRef(), KeyStatus::Usable });

            changedKeys = WTFMove(keyStatusVector);
        }
    }

    // FIXME: Session closure, expiration and message handling should be implemented
    // once the relevant algorithms are supported.

    callback(false, WTFMove(changedKeys), std::nullopt, std::nullopt, SuccessValue::Succeeded);
}

void MockCDMInstanceSession::loadSession(LicenseType, const String&, const String&, LoadSessionCallback&& callback)
{
    MockCDMFactory* factory = m_instance ? m_instance->factory() : nullptr;
    if (!factory) {
        callback(std::nullopt, std::nullopt, std::nullopt, SuccessValue::Failed, SessionLoadFailure::Other);
        return;
    }

    // FIXME: Key status and expiration handling should be implemented once the relevant algorithms are supported.

    CString messageData { "session loaded" };
    Message message { MessageType::LicenseRenewal, SharedBuffer::create(messageData.data(), messageData.length()) };

    callback(std::nullopt, std::nullopt, WTFMove(message), SuccessValue::Succeeded, SessionLoadFailure::None);
}

void MockCDMInstanceSession::closeSession(const String& sessionID, CloseSessionCallback&& callback)
{
    MockCDMFactory* factory = m_instance ? m_instance->factory() : nullptr;
    if (!factory) {
        callback();
        return;
    }

    factory->removeSessionWithID(sessionID);
    callback();
}

void MockCDMInstanceSession::removeSessionData(const String& id, LicenseType, RemoveSessionDataCallback&& callback)
{
    MockCDMFactory* factory = m_instance ? m_instance->factory() : nullptr;
    if (!factory) {
        callback({ }, std::nullopt, SuccessValue::Failed);
        return;
    }

    auto keys = factory->removeKeysFromSessionWithID(id);
    KeyStatusVector keyStatusVector;
    keyStatusVector.reserveInitialCapacity(keys.size());
    for (auto& key : keys)
        keyStatusVector.uncheckedAppend({ WTFMove(key), KeyStatus::Released });

    CString message { "remove-message" };
    callback(WTFMove(keyStatusVector), SharedBuffer::create(message.data(), message.length()), SuccessValue::Succeeded);
}

void MockCDMInstanceSession::storeRecordOfKeyUsage(const String&)
{
    // FIXME: This should be implemented along with the support for persistent-usage-record sessions.
}

}

#endif
