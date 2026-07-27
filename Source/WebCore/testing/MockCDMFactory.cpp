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
#include "ParsedContentType.h"
#include "SharedBuffer.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <algorithm>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UUID.h>
#include <wtf/WeakHashSet.h>
#include <wtf/text/StringHash.h>
#include <wtf/text/StringView.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(MockCDM);

static WeakHashSet<MockCDMFactory>& allMockFactories()
{
    static NeverDestroyed<WeakHashSet<MockCDMFactory>> factories;
    return factories;
}

void MockCDMFactory::unregisterAllMockFactories()
{
    for (auto& weakFactory : copyToVector(allMockFactories())) {
        if (RefPtr factory = weakFactory.get())
            factory->unregister();
    }
}

MockCDMFactory::MockCDMFactory()
    : m_supportedSessionTypes({ MediaKeySessionType::Temporary, MediaKeySessionType::PersistentUsageRecord, MediaKeySessionType::PersistentLicense })
    , m_supportedEncryptionSchemes({ MediaKeyEncryptionScheme::cenc })
{
    CDMFactory::registerFactory(*this);
    allMockFactories().add(*this);
}

MockCDMFactory::~MockCDMFactory()
{
    unregister();
    allMockFactories().remove(*this);
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
    return keySystem == "org.webkit.mock"_s;
}

bool MockCDMFactory::hasSessionWithID(const String& id)
{
    if (id.isEmpty())
        return false;

    return m_sessions.contains(id);
}

void MockCDMFactory::removeSessionWithID(const String& id)
{
    if (id.isEmpty())
        return;
    m_sessions.remove(id);
    m_sessionTypes.remove(id);
}

void MockCDMFactory::addKeysToSessionWithID(const String& id, Vector<Ref<SharedBuffer>>&& keys)
{
    if (id.isEmpty())
        return;

    auto addResult = m_sessions.add(id, WTF::move(keys));
    if (addResult.isNewEntry)
        return;

    auto& value = addResult.iterator->value;
    for (auto& key : keys)
        value.append(WTF::move(key));
}

Vector<Ref<SharedBuffer>> MockCDMFactory::removeKeysFromSessionWithID(const String& id)
{
    if (id.isEmpty())
        return { };

    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return { };

    return WTF::move(it->value);
}

const Vector<Ref<SharedBuffer>>* MockCDMFactory::keysForSessionWithID(const String& id) const
{
    if (id.isEmpty())
        return { };

    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return nullptr;
    return &it->value;
}

size_t MockCDMFactory::keyCountForSession(const String& id) const
{
    if (id.isEmpty())
        return 0;

    auto it = m_sessions.find(id);
    if (it == m_sessions.end())
        return 0;
    return it->value.size();
}

std::optional<CDMSessionType> MockCDMFactory::sessionType(const String& id) const
{
    if (id.isEmpty())
        return std::nullopt;

    auto it = m_sessionTypes.find(id);
    if (it == m_sessionTypes.end())
        return std::nullopt;
    return it->value;
}

void MockCDMFactory::setSupportedDataTypes(Vector<String>&& types)
{
    m_supportedDataTypes.clear();
    for (auto& type : types)
        m_supportedDataTypes.append(type);
}

std::unique_ptr<CDMPrivate> MockCDMFactory::createCDM(const String&, const String& mediaKeysHashSalt, const CDMPrivateClient&)
{
    return makeUnique<MockCDM>(*this, mediaKeysHashSalt);
}

MockCDM::MockCDM(WeakPtr<MockCDMFactory> factory, const String& mediaKeysHashSalt)
    : m_factory(WTF::move(factory))
    , m_mediaKeysHashSalt { mediaKeysHashSalt }
{
}

Vector<String> MockCDM::supportedInitDataTypes() const
{
    if (RefPtr factory = this->factory())
        return factory->supportedDataTypes();
    return { };
}

Vector<String> MockCDM::supportedRobustnesses() const
{
    if (RefPtr factory = this->factory())
        return factory->supportedRobustness();
    return { };
}

bool MockCDM::supportsConfiguration(const MediaKeySystemConfiguration& configuration) const
{
    RefPtr factory = this->factory();
    if (!factory)
        return false;

    auto capabilityHasSupportedEncryptionScheme = [factory = factory.releaseNonNull()] (auto& capability) {
        if (capability.encryptionScheme)
            return factory->supportedEncryptionSchemes().contains(capability.encryptionScheme.value());
        return true;
    };

    if (!configuration.audioCapabilities.isEmpty() && std::ranges::none_of(configuration.audioCapabilities, capabilityHasSupportedEncryptionScheme))
        return false;

    if (!configuration.videoCapabilities.isEmpty() && std::ranges::none_of(configuration.videoCapabilities, capabilityHasSupportedEncryptionScheme))
        return false;

    return true;

}

bool MockCDM::supportsConfigurationWithRestrictions(const MediaKeySystemConfiguration& configuration, const MediaKeysRestrictions&) const
{
    RefPtr factory = this->factory();
    if (!factory)
        return true;

    const auto& unsupportedVideoCodecs = factory->unsupportedVideoCodecs();
    if (unsupportedVideoCodecs.isEmpty())
        return true;

    for (const auto& capability : configuration.videoCapabilities) {
        auto contentType = ParsedContentType::create(capability.contentType);
        if (!contentType)
            continue;
        auto codecs = contentType->parameterValueForName("codecs"_s);
        if (!codecs.isEmpty() && unsupportedVideoCodecs.contains(codecs))
            return false;
    }

    return true;
}

bool MockCDM::supportsSessionTypeWithConfiguration(const MediaKeySessionType& sessionType, const MediaKeySystemConfiguration&) const
{
    if (RefPtr factory = this->factory(); !factory || !factory->supportedSessionTypes().contains(sessionType))
        return false;

    // NOTE: Implement configuration checking;
    return true;
}

MediaKeysRequirement MockCDM::distinctiveIdentifiersRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const
{
    if (RefPtr factory = this->factory())
        return factory->distinctiveIdentifiersRequirement();
    return MediaKeysRequirement::Optional;
}

MediaKeysRequirement MockCDM::persistentStateRequirement(const MediaKeySystemConfiguration&, const MediaKeysRestrictions&) const
{
    if (RefPtr factory = this->factory())
        return factory->persistentStateRequirement();
    return MediaKeysRequirement::Optional;
}

bool MockCDM::distinctiveIdentifiersAreUniquePerOriginAndClearable(const MediaKeySystemConfiguration&) const
{
    // NOTE: Implement;
    return true;
}

RefPtr<CDMInstance> MockCDM::createInstance()
{
    if (RefPtr factory = this->factory(); factory && factory->canCreateInstances())
        return MockCDMInstance::create(*this);
    return nullptr;
}

void MockCDM::loadAndInitialize()
{
    // No-op.
}

bool MockCDM::supportsServerCertificates() const
{
    if (RefPtr factory = this->factory())
        return factory->supportsServerCertificates();
    return false;
}

bool MockCDM::supportsSessions() const
{
    if (RefPtr factory = this->factory())
        return factory->supportsSessions();
    return false;
}

bool MockCDM::supportsInitData(const String& initDataType, const SharedBuffer& initData) const
{
    if (!supportedInitDataTypes().contains(initDataType))
        return false;

    UNUSED_PARAM(initData);
    return true;
}

RefPtr<SharedBuffer> MockCDM::sanitizeResponse(const SharedBuffer& response) const
{
    auto contiguousResponse = response.makeContiguous();
    if (!charactersAreAllASCII(contiguousResponse->span()))
        return nullptr;

    for (auto word : StringView(byteCast<Latin1Character>(contiguousResponse->span())).split(' ')) {
        if (word == "valid-response"_s)
            return contiguousResponse;
    }

    return nullptr;
}

std::optional<String> MockCDM::sanitizeSessionId(const String& sessionId) const
{
    if (equalLettersIgnoringASCIICase(sessionId, "valid-loaded-session"_s))
        return sessionId;
    // Accept session IDs currently tracked by the factory so tests can round-trip
    // (e.g. close a persistent-license session, then load() it again by the same ID).
    if (RefPtr factory = this->factory(); factory && factory->hasSessionWithID(sessionId))
        return sessionId;
    return std::nullopt;
}

Ref<MockCDMInstance> MockCDMInstance::create(MockCDM& cdm)
{
    return adoptRef(*new MockCDMInstance(cdm));
}

MockCDMInstance::MockCDMInstance(MockCDM& cdm)
    : m_cdm(cdm)
{
}

void MockCDMInstance::initializeWithConfiguration(const MediaKeySystemConfiguration& configuration, AllowDistinctiveIdentifiers distinctiveIdentifiers, AllowPersistentState persistentState, SuccessCallback&& callback)
{
    auto initialize = [&, this, protectedThis = Ref { *this }] {
        CheckedPtr cdm = m_cdm.get();
        if (!cdm || !cdm->supportsConfiguration(configuration))
            return CDMInstanceSuccessValue::Failed;

        RefPtr factory = cdm->factory();
        if (!factory)
            return CDMInstanceSuccessValue::Failed;

        bool distinctiveIdentifiersAllowed = (distinctiveIdentifiers == AllowDistinctiveIdentifiers::Yes);

        if (m_distinctiveIdentifiersAllowed != distinctiveIdentifiersAllowed) {
            if (!distinctiveIdentifiersAllowed && factory->distinctiveIdentifiersRequirement() == MediaKeysRequirement::Required)
                return CDMInstanceSuccessValue::Failed;

            m_distinctiveIdentifiersAllowed = distinctiveIdentifiersAllowed;
        }

        bool persistentStateAllowed = (persistentState == AllowPersistentState::Yes);

        if (m_persistentStateAllowed != persistentStateAllowed) {
            if (!persistentStateAllowed && factory->persistentStateRequirement() == MediaKeysRequirement::Required)
                return CDMInstanceSuccessValue::Failed;

            m_persistentStateAllowed = persistentStateAllowed;
        }
        return CDMInstanceSuccessValue::Succeeded;
    };

    callback(initialize());
}

void MockCDMInstance::setServerCertificate(Ref<SharedBuffer>&& certificate, SuccessCallback&& callback)
{
    Ref contiguousData = certificate->makeContiguous();
    callback(equalLettersIgnoringASCIICase(StringView { byteCast<Latin1Character>(contiguousData->span()) }, "valid"_s) ? CDMInstanceSuccessValue::Succeeded : CDMInstanceSuccessValue::Failed);
}

void MockCDMInstance::setStorageDirectory(const String&)
{
}

const String& MockCDMInstance::keySystem() const
{
    static const NeverDestroyed<String> s_keySystem = MAKE_STATIC_STRING_IMPL("org.webkit.mock");

    return s_keySystem;
}

RefPtr<CDMInstanceSession> MockCDMInstance::createSession()
{
    return adoptRef(new MockCDMInstanceSession(*this));
}

RefPtr<MockCDMFactory> MockCDMInstance::factory() const
{
    if (CheckedPtr cdm = m_cdm.get())
        return cdm->factory();
    return nullptr;
}

MockCDMInstanceSession::MockCDMInstanceSession(WeakPtr<MockCDMInstance>&& instance)
    : m_instance(WTF::move(instance))
{
}

RefPtr<MockCDMFactory> MockCDMInstanceSession::factory() const
{
    if (RefPtr instance = m_instance.get())
        return instance->factory();
    return nullptr;
}

void MockCDMInstanceSession::requestLicense(LicenseType licenseType, KeyGroupingStrategy, const String& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    RefPtr factory = this->factory();
    if (!factory) {
        callback(SharedBuffer::create(), emptyString(), false, SuccessValue::Failed);
        return;
    }

    if (!factory->supportedSessionTypes().contains(licenseType) || !factory->supportedDataTypes().contains(initDataType)) {
        callback(SharedBuffer::create(), emptyString(), false, SuccessValue::Failed);
        return;
    }

    auto keyIDs = InitDataRegistry::singleton().extractKeyIDs(initDataType, initData);
    if (!keyIDs || keyIDs.value().isEmpty()) {
        callback(SharedBuffer::create(), emptyString(), false, SuccessValue::Failed);
        return;
    }

    String sessionID = createVersion4UUIDString();
    factory->addKeysToSessionWithID(sessionID, WTF::move(keyIDs.value()));
    factory->setSessionType(sessionID, licenseType);

    CString license { "license"_s };
    callback(SharedBuffer::create(license.span()), sessionID, false, SuccessValue::Succeeded);
}

void MockCDMInstanceSession::updateLicense(const String& sessionID, LicenseType, Ref<SharedBuffer>&& response, LicenseUpdateCallback&& callback)
{
    RefPtr factory = this->factory();
    if (!factory) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, SuccessValue::Failed);
        return;
    }

    Vector<String> responseVector = String(byteCast<Latin1Character>(response->makeContiguous()->span())).split(' ');

    if (responseVector.contains(String("invalid-format"_s))) {
        callback(false, std::nullopt, std::nullopt, std::nullopt, SuccessValue::Failed);
        return;
    }

    std::optional<KeyStatusVector> changedKeys;
    if (responseVector.contains(String("keys-changed"_s))) {
        const auto* keys = factory->keysForSessionWithID(sessionID);
        if (keys) {
            changedKeys = keys->map([](auto& key) {
                return std::pair { key.copyRef(), KeyStatus::Usable };
            });
        }
    }

    std::optional<double> changedExpiration;
    if (double expiration = factory->expirationOnUpdate(); !std::isnan(expiration))
        changedExpiration = expiration;

    callback(false, WTF::move(changedKeys), WTF::move(changedExpiration), std::nullopt, SuccessValue::Succeeded);
}

void MockCDMInstanceSession::loadSession(LicenseType, const String& sessionID, const String&, LoadSessionCallback&& callback)
{
    RefPtr factory = this->factory();
    if (!factory) {
        callback(std::nullopt, std::nullopt, std::nullopt, SuccessValue::Failed, SessionLoadFailure::Other);
        return;
    }

    // If the factory is currently tracking this session (e.g. it was previously created
    // as a persistent-license and closed but not removed), restore its key statuses so
    // the loaded session sees its stored keys. Otherwise fall back to a minimal
    // "session loaded" stub for tests that call load() with the sentinel ID.
    std::optional<KeyStatusVector> knownKeys;
    if (const auto* keys = factory->keysForSessionWithID(sessionID)) {
        knownKeys = keys->map([](auto& key) {
            return std::pair { key.copyRef(), KeyStatus::Usable };
        });
    }

    CString messageData { "session loaded"_s };
    Message message { MessageType::LicenseRenewal, SharedBuffer::create(messageData.span()) };

    callback(WTF::move(knownKeys), std::nullopt, WTF::move(message), SuccessValue::Succeeded, SessionLoadFailure::None);
}

void MockCDMInstanceSession::closeSession(const String& sessionID, CloseSessionCallback&& callback)
{
    RefPtr factory = this->factory();
    if (!factory) {
        callback();
        return;
    }

    // Per EME "Session Closed" algorithm, keys and licenses associated with a session
    // MUST be destroyed on close unless the session was persistent-license (in which
    // case the state is retained so it can be reloaded).
    if (factory->sessionType(sessionID) != CDMSessionType::PersistentLicense)
        factory->removeSessionWithID(sessionID);
    callback();
}

void MockCDMInstanceSession::removeSessionData(const String& id, LicenseType, RemoveSessionDataCallback&& callback)
{
    RefPtr factory = this->factory();
    if (!factory) {
        callback({ }, nullptr, SuccessValue::Failed);
        return;
    }

    auto keys = factory->removeKeysFromSessionWithID(id);
    auto keyStatusVector = WTF::map(WTF::move(keys), [](Ref<SharedBuffer>&& key) {
        return std::pair { WTF::move(key), KeyStatus::Released };
    });

    CString message { "remove-message"_s };
    callback(WTF::move(keyStatusVector), SharedBuffer::create(message.span()), SuccessValue::Succeeded);
}

void MockCDMInstanceSession::storeRecordOfKeyUsage(const String&)
{
    // FIXME: This should be implemented along with the support for persistent-usage-record sessions.
}

}

#endif
