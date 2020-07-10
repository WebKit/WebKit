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

#include "config.h"
#include "CDMThunder.h"

#if ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)

#include "CDMKeySystemConfiguration.h"
#include "CDMProxyThunder.h"
#include "CDMRestrictions.h"
#include "CDMSessionType.h"
#include "CDMUtilities.h"
#include "GStreamerEMEUtilities.h"
#include "Logging.h"
#include "MediaKeyMessageType.h"
#include "MediaKeyStatus.h"
#include "NotImplemented.h"
#include "SharedBuffer.h"
#include <algorithm>
#include <iterator>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>

#if (!defined(GST_DISABLE_GST_DEBUG))
GST_DEBUG_CATEGORY_EXTERN(webkitMediaThunderDecryptDebugCategory);
#define GST_CAT_DEFAULT webkitMediaThunderDecryptDebugCategory
#endif

namespace {

// We are doing this to avoid conflict with CDMInstanceSession::KeyStatus.
using ThunderKeyStatus = KeyStatus;

}

static LicenseType thunderLicenseType(WebCore::CDMInstanceSession::LicenseType licenseType)
{
    switch (licenseType) {
    case WebCore::CDMInstanceSession::LicenseType::Temporary:
        return Temporary;
    case WebCore::CDMInstanceSession::LicenseType::PersistentUsageRecord:
        return PersistentUsageRecord;
    case WebCore::CDMInstanceSession::LicenseType::PersistentLicense:
        return PersistentLicense;
    default:
        ASSERT_NOT_REACHED();
        return Temporary;
    }
}

namespace WebCore {

static CDMInstanceSession::SessionLoadFailure sessionLoadFailureFromThunder(const StringView& loadStatus)
{
    if (loadStatus == "None")
        return CDMInstanceSession::SessionLoadFailure::None;
    if (loadStatus == "SessionNotFound")
        return CDMInstanceSession::SessionLoadFailure::NoSessionData;
    if (loadStatus == "MismatchedSessionType")
        return CDMInstanceSession::SessionLoadFailure::MismatchedSessionType;
    if (loadStatus == "QuotaExceeded")
        return CDMInstanceSession::SessionLoadFailure::QuotaExceeded;
    return CDMInstanceSession::SessionLoadFailure::Other;
}


CDMFactoryThunder& CDMFactoryThunder::singleton()
{
    static NeverDestroyed<CDMFactoryThunder> s_factory;
    return s_factory;
}

std::unique_ptr<CDMPrivate> CDMFactoryThunder::createCDM(const String& keySystem)
{
    ASSERT(supportsKeySystem(keySystem));
    return makeUnique<CDMPrivateThunder>(keySystem);
}

RefPtr<CDMProxy> CDMFactoryThunder::createCDMProxy(const String& keySystem)
{
    ASSERT(supportsKeySystem(keySystem));
    return adoptRef(new CDMProxyThunder(keySystem));
}

const Vector<String>& CDMFactoryThunder::supportedKeySystems() const
{
    static std::once_flag onceFlag;
    static Vector<String> supportedKeySystems;
    std::call_once(onceFlag, [] {
        std::string emptyString;
        // Yes, this is right, 0 means supported, hence something else means not supported.
        if (!opencdm_is_type_supported(GStreamerEMEUtilities::s_WidevineKeySystem, emptyString.c_str()))
            supportedKeySystems.append(GStreamerEMEUtilities::s_WidevineKeySystem);
#ifndef NDEBUG
        if (supportedKeySystems.isEmpty() && isThunderRanked()) {
            ASSERT_NOT_REACHED_WITH_MESSAGE("Thunder is up-ranked as preferred decryptor but Thunder is not supporting any encryption system. Is "
                "Thunder running? Are the plugins built?");
        }
#endif
    });
    return supportedKeySystems;
}

bool CDMFactoryThunder::supportsKeySystem(const String& keySystem)
{
    return CDMFactoryThunder::singleton().supportedKeySystems().contains(keySystem);
}

Vector<AtomString> CDMPrivateThunder::supportedInitDataTypes() const
{
    static std::once_flag onceFlag;
    static Vector<AtomString> supportedInitDataTypes;
    std::call_once(onceFlag, [] {
        supportedInitDataTypes.reserveInitialCapacity(3);
        supportedInitDataTypes.uncheckedAppend(AtomString("keyids"));
        supportedInitDataTypes.uncheckedAppend(AtomString("cenc"));
        supportedInitDataTypes.uncheckedAppend(AtomString("webm"));
    });
    return supportedInitDataTypes;
}

bool CDMPrivateThunder::supportsConfiguration(const CDMKeySystemConfiguration& configuration) const
{
    for (auto& audioCapability : configuration.audioCapabilities) {
        if (opencdm_is_type_supported(m_keySystem.utf8().data(), audioCapability.contentType.utf8().data()))
            return false;
    }
    for (auto& videoCapability : configuration.videoCapabilities) {
        if (opencdm_is_type_supported(m_keySystem.utf8().data(), videoCapability.contentType.utf8().data()))
            return false;
    }
    return true;
}

Vector<AtomString> CDMPrivateThunder::supportedRobustnesses() const
{
    return { emptyAtom() };
}

CDMRequirement CDMPrivateThunder::distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const
{
    return CDMRequirement::Optional;
}

CDMRequirement CDMPrivateThunder::persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const
{
    return CDMRequirement::Optional;
}

bool CDMPrivateThunder::distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const
{
    return false;
}

RefPtr<CDMInstance> CDMPrivateThunder::createInstance()
{
    return adoptRef(new CDMInstanceThunder(m_keySystem));
}

void CDMPrivateThunder::loadAndInitialize()
{
    // No-op.
}

bool CDMPrivateThunder::supportsServerCertificates() const
{
    // Server certificates are not supported.
    return false;
}

bool CDMPrivateThunder::supportsSessions() const
{
    // Sessions are supported.
    return true;
}

bool CDMPrivateThunder::supportsInitData(const AtomString& initDataType, const SharedBuffer& initData) const
{
    // Validate the initData buffer as an JSON object in keyids case.
    if (equalLettersIgnoringASCIICase(initDataType, "keyids") && CDMUtilities::parseJSONObject(initData))
        return true;

    // Validate the initData buffer as CENC initData. FIXME: Validate it is actually CENC.
    if (equalLettersIgnoringASCIICase(initDataType, "cenc") && !initData.isEmpty())
        return true;

    // Validate the initData buffer as WebM initData.
    if (equalLettersIgnoringASCIICase(initDataType, "webm") && !initData.isEmpty())
        return true;

    return false;
}

RefPtr<SharedBuffer> CDMPrivateThunder::sanitizeResponse(const SharedBuffer& response) const
{
    return response.copy();
}

Optional<String> CDMPrivateThunder::sanitizeSessionId(const String& sessionId) const
{
    return sessionId;
}

CDMInstanceThunder::CDMInstanceThunder(const String& keySystem)
    : CDMInstanceProxy(keySystem)
    , m_thunderSystem(opencdm_create_system(keySystem.utf8().data()))
    , m_keySystem(keySystem)
{
}

void CDMInstanceThunder::initializeWithConfiguration(const CDMKeySystemConfiguration&, AllowDistinctiveIdentifiers, AllowPersistentState,
    SuccessCallback&& callback)
{
    callback(Succeeded);
}

void CDMInstanceThunder::setServerCertificate(Ref<SharedBuffer>&& certificate,  SuccessCallback&& callback)
{
    OpenCDMError error = opencdm_system_set_server_certificate(m_thunderSystem.get(), const_cast<uint8_t*>(certificate->dataAsUInt8Ptr()),
        certificate->size());
    callback(!error ? Succeeded : Failed);
}

void CDMInstanceThunder::setStorageDirectory(const String&)
{
    notImplemented();
}

CDMInstanceSessionThunder::CDMInstanceSessionThunder(CDMInstanceThunder& instance)
    : CDMInstanceSessionProxy(instance)
{
    ASSERT(isMainThread());
    m_thunderSessionCallbacks.process_challenge_callback = [](OpenCDMSession*, void* userData, const char[], const uint8_t challenge[],
        const uint16_t challengeLength) {
        GST_DEBUG("Got 'challenge' OCDM notification");
        callOnMainThread([session = makeWeakPtr(static_cast<CDMInstanceSessionThunder*>(userData)), buffer = WebCore::SharedBuffer::create(challenge,
            challengeLength)]() mutable {
            if (!session)
                return;
            session->challengeGeneratedCallback(WTFMove(buffer));
        });
    };
    m_thunderSessionCallbacks.key_update_callback = [](OpenCDMSession*, void* userData, const uint8_t keyIDData[], const uint8_t keyIDLength) {
        GST_DEBUG("Got 'key updated' OCDM notification");
        KeyIDType keyID;
        keyID.append(keyIDData, keyIDLength);
        callOnMainThread([session = makeWeakPtr(static_cast<CDMInstanceSessionThunder*>(userData)), keyID = WTFMove(keyID)]() mutable {
            if (!session)
                return;
            session->keyUpdatedCallback(WTFMove(keyID));
        });
    };
    m_thunderSessionCallbacks.keys_updated_callback = [](const OpenCDMSession*, void* userData) {
        GST_DEBUG("Got 'all keys updated' OCDM notification");
        callOnMainThread([session = makeWeakPtr(static_cast<CDMInstanceSessionThunder*>(userData))]() {
            if (!session)
                return;
            session->keysUpdateDoneCallback();
        });
    };
    m_thunderSessionCallbacks.error_message_callback = [](OpenCDMSession*, void* userData, const char message[]) {
        GST_ERROR("Got 'error' OCDM notification: %s", message);
        callOnMainThread([session = makeWeakPtr(static_cast<CDMInstanceSessionThunder*>(userData)), buffer = WebCore::SharedBuffer::create(message,
            strlen(message))]() mutable {
            if (!session)
                return;
            session->errorCallback(WTFMove(buffer));
        });
    };
}

RefPtr<CDMInstanceSession> CDMInstanceThunder::createSession()
{
    RefPtr<CDMInstanceSessionThunder> newSession = adoptRef(new CDMInstanceSessionThunder(*this));
    ASSERT(newSession);
    trackSession(*newSession);
    return newSession;
}

class ParsedResponseMessage {

public:
    ParsedResponseMessage(const RefPtr<SharedBuffer>& buffer)
    {
        if (!buffer)
            return;

        StringView payload(reinterpret_cast<const LChar*>(buffer->data()), buffer->size());
        size_t typePosition = payload.find(":Type:");
        StringView requestType(payload.characters8(), typePosition != notFound ? typePosition : 0);
        unsigned offset = 0u;
        if (!requestType.isEmpty() && requestType.length() != payload.length())
            offset = typePosition + 6;

        if (requestType.length() == 1)
            m_type = makeOptional(static_cast<WebCore::MediaKeyMessageType>(requestType.toInt()));

        m_payload = SharedBuffer::create(payload.characters8() + offset, payload.length() - offset);
    }

    bool hasPayload() const { return static_cast<bool>(m_payload); }
    const Ref<SharedBuffer>& payload() const& { ASSERT(m_payload); return m_payload.value(); }
    Ref<SharedBuffer>& payload() & { ASSERT(m_payload); return m_payload.value(); }
    bool hasType() const { return static_cast<bool>(m_type); }
    WebCore::MediaKeyMessageType type() const { ASSERT(m_type); return m_type.value(); }
    WebCore::MediaKeyMessageType typeOr(WebCore::MediaKeyMessageType alternate) const { return m_type ? m_type.value() : alternate; }

private:
    Optional<Ref<SharedBuffer>> m_payload;
    Optional<WebCore::MediaKeyMessageType> m_type;
};

void CDMInstanceSessionThunder::challengeGeneratedCallback(RefPtr<SharedBuffer>&& buffer)
{
    ParsedResponseMessage parsedResponseMessage(buffer);

    if (!m_challengeCallbacks.isEmpty()) {
        m_message = WTFMove(parsedResponseMessage.payload());
        m_needsIndividualization = parsedResponseMessage.type() == CDMInstanceSession::MessageType::IndividualizationRequest;

        for (const auto& challengeCallback : m_challengeCallbacks)
            challengeCallback();
        m_challengeCallbacks.clear();
    } else if (!m_sessionChangedCallbacks.isEmpty()) {
        for (auto& sessionChangedCallback : m_sessionChangedCallbacks)
            sessionChangedCallback(true, parsedResponseMessage.payload().copyRef());
        m_sessionChangedCallbacks.clear();
    } else {
        if (m_client && parsedResponseMessage.hasType()) {
            m_client->sendMessage(static_cast<CDMMessageType>(parsedResponseMessage.type()),
                WTFMove(parsedResponseMessage.payload()));
        }
    }
}

#if !defined(GST_DISABLE_GST_DEBUG) || !GST_DISABLE_GST_DEBUG
static const char* toString(CDMInstanceSession::KeyStatus status)
{
    switch (status) {
    case CDMInstanceSession::KeyStatus::Usable:
        return "Usable";
    case CDMInstanceSession::KeyStatus::Expired:
        return "Expired";
    case CDMInstanceSession::KeyStatus::Released:
        return "Released";
    case CDMInstanceSession::KeyStatus::OutputRestricted:
        return "OutputRestricted";
    case CDMInstanceSession::KeyStatus::OutputDownscaled:
        return "OutputDownscaled";
    case CDMInstanceSession::KeyStatus::StatusPending:
        return "StatusPending";
    case CDMInstanceSession::KeyStatus::InternalError:
        return "InternalError";
    default:
        ASSERT_NOT_REACHED();
        return "unknown";
    }
}
#endif

CDMInstanceSession::KeyStatus CDMInstanceSessionThunder::status(const KeyIDType& keyID) const
{
    ThunderKeyStatus status = m_session && !m_sessionID.isEmpty() ? opencdm_session_status(m_session.get(), keyID.data(), keyID.size()) : StatusPending;

    switch (status) {
    case Usable:
        return CDMInstanceSession::KeyStatus::Usable;
    case Expired:
        return CDMInstanceSession::KeyStatus::Expired;
    case Released:
        return CDMInstanceSession::KeyStatus::Released;
    case OutputRestricted:
        return CDMInstanceSession::KeyStatus::OutputRestricted;
    case OutputDownscaled:
        return CDMInstanceSession::KeyStatus::OutputDownscaled;
    case StatusPending:
        return CDMInstanceSession::KeyStatus::StatusPending;
    case InternalError:
        return CDMInstanceSession::KeyStatus::InternalError;
    default:
        ASSERT_NOT_REACHED();
        return CDMInstanceSession::KeyStatus::InternalError;
    }
}

void CDMInstanceSessionThunder::keyUpdatedCallback(KeyIDType&& keyID)
{
    GST_MEMDUMP("updated key", keyID.data(), keyID.size());

    auto keyStatus = status(keyID);
    GST_DEBUG("updated with with key status %s", toString(keyStatus));
    m_doesKeyStoreNeedMerging |= m_keyStore.add(KeyHandle::create(keyStatus, WTFMove(keyID), BoxPtr<OpenCDMSession>()));
}

void CDMInstanceSessionThunder::keysUpdateDoneCallback()
{
    GST_DEBUG("update done");
    if (m_doesKeyStoreNeedMerging) {
        m_doesKeyStoreNeedMerging = false;
        auto instance = cdmInstanceThunder();
        if (instance)
            instance->mergeKeysFrom(m_keyStore);
    }

    if (m_sessionChangedCallbacks.isEmpty() && m_client) {
        m_client->updateKeyStatuses(m_keyStore.convertToJSKeyStatusVector());
        return;
    }

    for (auto& sessionChangedCallback : m_sessionChangedCallbacks)
        sessionChangedCallback(true, nullptr);
    m_sessionChangedCallbacks.clear();
}

void CDMInstanceSessionThunder::errorCallback(RefPtr<SharedBuffer>&& message)
{
    GST_ERROR("CDM error");
    GST_MEMDUMP("error dump", message->dataAsUInt8Ptr(), message->size());
    for (const auto& challengeCallback : m_challengeCallbacks)
        challengeCallback();
    m_challengeCallbacks.clear();

    for (auto& sessionChangedCallback : m_sessionChangedCallbacks)
        sessionChangedCallback(false, message.copyRef());
    m_sessionChangedCallbacks.clear();
}

void CDMInstanceSessionThunder::requestLicense(LicenseType licenseType, const AtomString& initDataType, Ref<SharedBuffer>&& initDataSharedBuffer,
    LicenseCallback&& callback)
{
    ASSERT(isMainThread());

    // FIXME: UUID or system ID?
    auto instance = cdmInstanceThunder();
    ASSERT(instance);
    m_initData = InitData(instance->keySystem(), WTFMove(initDataSharedBuffer));

    GST_TRACE("Going to request a new session id, init data size %lu", m_initData.payload()->size());
    GST_MEMDUMP("init data", m_initData.payload()->dataAsUInt8Ptr(), m_initData.payload()->size());

    OpenCDMSession* session = nullptr;
    opencdm_construct_session(&instance->thunderSystem(), thunderLicenseType(licenseType), initDataType.string().utf8().data(),
        m_initData.payload()->dataAsUInt8Ptr(), m_initData.payload()->size(), nullptr, 0, &m_thunderSessionCallbacks, this, &session);
    if (!session) {
        GST_ERROR("Could not create session");
        RefPtr<SharedBuffer> initData = m_initData.payload();
        callback(initData.releaseNonNull(), { }, false, Failed);
        return;
    }
    m_session.reset(session);
    m_sessionID = String::fromUTF8(opencdm_session_id(m_session.get()));

    auto generateChallenge = [this, callback = WTFMove(callback)]() mutable {
        ASSERT(isMainThread());
        RefPtr<SharedBuffer> initData = m_initData.payload();
        if (m_sessionID.isEmpty()) {
            GST_ERROR("could not create session id");
            callback(initData.releaseNonNull(), { }, false, Failed);
            return;
        }

        if (!isValid()) {
            GST_WARNING("created invalid session %s", m_sessionID.utf8().data());
            callback(initData.releaseNonNull(), m_sessionID, false, Failed);
            removeFromInstanceProxy();
            return;
        }

        GST_DEBUG("created valid session %s", m_sessionID.utf8().data());
        callback(m_message.copyRef().releaseNonNull(), m_sessionID, m_needsIndividualization, Succeeded);
    };

    if (m_sessionID.isEmpty() || isValid())
        generateChallenge();
    else
        m_challengeCallbacks.append(WTFMove(generateChallenge));
}

void CDMInstanceSessionThunder::sessionFailure()
{
    for (auto& sessionChangedCallback : m_sessionChangedCallbacks)
        sessionChangedCallback(false, nullptr);
    m_sessionChangedCallbacks.clear();
}

void CDMInstanceSessionThunder::updateLicense(const String& sessionID, LicenseType, Ref<SharedBuffer>&& response, LicenseUpdateCallback&& callback)
{
    ASSERT_UNUSED(sessionID, sessionID == m_sessionID);

    GST_TRACE("Updating session %s", sessionID.utf8().data());

    m_sessionChangedCallbacks.append([this, callback = WTFMove(callback)](bool success, RefPtr<SharedBuffer>&& responseMessage) mutable {
        ASSERT(isMainThread());
        if (success) {
            if (!responseMessage) {
                ASSERT(!m_keyStore.isEmpty());
                callback(false, m_keyStore.convertToJSKeyStatusVector(), WTF::nullopt, WTF::nullopt, SuccessValue::Succeeded);
            } else {
                // FIXME: Using JSON reponse messages is much cleaner than using string prefixes, I believe there
                // will even be other parts of the spec where not having structured data will be bad.
                ParsedResponseMessage parsedResponseMessage(responseMessage);
                if (parsedResponseMessage.hasPayload()) {
                    Ref<SharedBuffer> message = WTFMove(parsedResponseMessage.payload());
                    GST_DEBUG("got message of size %lu", message->size());
                    GST_MEMDUMP("message", message->dataAsUInt8Ptr(), message->size());
                    callback(false, WTF::nullopt, WTF::nullopt,
                        std::make_pair(parsedResponseMessage.typeOr(MediaKeyMessageType::LicenseRequest),
                            WTFMove(message)), SuccessValue::Succeeded);
                } else {
                    GST_ERROR("message of size %lu incorrectly formatted", responseMessage ? responseMessage->size() : 0);
                    callback(false, WTF::nullopt, WTF::nullopt, WTF::nullopt, SuccessValue::Failed);
                }
            }
        } else {
            GST_ERROR("update license reported error state");
            callback(false, WTF::nullopt, WTF::nullopt, WTF::nullopt, SuccessValue::Failed);
        }
    });
    if (!m_session || m_sessionID.isEmpty() || opencdm_session_update(m_session.get(), response->dataAsUInt8Ptr(), response->size()))
        sessionFailure();
}

void CDMInstanceSessionThunder::loadSession(LicenseType, const String& sessionID, const String&, LoadSessionCallback&& callback)
{
    ASSERT_UNUSED(sessionID, sessionID == m_sessionID);

    m_sessionChangedCallbacks.append([this, callback = WTFMove(callback)](bool success, RefPtr<SharedBuffer>&& responseMessage) mutable {
        ASSERT(isMainThread());
        if (success) {
            if (!responseMessage)
                callback(m_keyStore.convertToJSKeyStatusVector(), WTF::nullopt, WTF::nullopt, SuccessValue::Succeeded, SessionLoadFailure::None);
            else {
                // FIXME: Using JSON reponse messages is much cleaner than using string prefixes, I believe there
                // will even be other parts of the spec where not having structured data will be bad.
                ParsedResponseMessage parsedResponseMessage(responseMessage);
                if (parsedResponseMessage.hasPayload()) {
                    Ref<SharedBuffer> message = WTFMove(parsedResponseMessage.payload());
                    GST_DEBUG("got message of size %lu", message->size());
                    GST_MEMDUMP("message", message->dataAsUInt8Ptr(), message->size());
                    callback(WTF::nullopt, WTF::nullopt, std::make_pair(parsedResponseMessage.typeOr(MediaKeyMessageType::LicenseRequest),
                        WTFMove(message)), SuccessValue::Succeeded, SessionLoadFailure::None);
                } else {
                    GST_ERROR("message of size %lu incorrectly formatted", responseMessage ? responseMessage->size() : 0);
                    callback(WTF::nullopt, WTF::nullopt, WTF::nullopt, SuccessValue::Failed, SessionLoadFailure::Other);
                }
            }
        } else {
            auto responseMessageData = responseMessage ? responseMessage->data() : nullptr;
            auto responseMessageSize = responseMessage ? responseMessage->size() : 0;
            StringView response(reinterpret_cast<const LChar*>(responseMessageData), responseMessageSize);
            GST_ERROR("session %s not loaded, reason %s", m_sessionID.utf8().data(), response.utf8().data());
            callback(WTF::nullopt, WTF::nullopt, WTF::nullopt, SuccessValue::Failed, sessionLoadFailureFromThunder(response));
        }
    });
    if (!m_session || m_sessionID.isEmpty() || opencdm_session_load(m_session.get()))
        sessionFailure();
}

void CDMInstanceSessionThunder::closeSession(const String& sessionID, CloseSessionCallback&& callback)
{
    ASSERT_UNUSED(sessionID, m_sessionID == sessionID);

    if (m_session && !m_sessionID.isEmpty())
        opencdm_session_close(m_session.get());

    removeFromInstanceProxy();

    callback();
}

void CDMInstanceSessionThunder::removeSessionData(const String& sessionID, LicenseType, RemoveSessionDataCallback&& callback)
{
    ASSERT_UNUSED(sessionID, m_sessionID == sessionID);

    m_sessionChangedCallbacks.append([this, callback = WTFMove(callback)](bool success, RefPtr<SharedBuffer>&& buffer) mutable {
        ASSERT(isMainThread());
        if (success) {
            if (!buffer)
                callback(m_keyStore.allKeysAs(MediaKeyStatus::Released), WTF::nullopt, SuccessValue::Succeeded);
            else {
                ParsedResponseMessage parsedResponseMessage(buffer);
                if (parsedResponseMessage.hasPayload()) {
                    Ref<SharedBuffer> message = WTFMove(parsedResponseMessage.payload());
                    GST_DEBUG("session %s removed, message length %lu", m_sessionID.utf8().data(), message->size());
                    callback(m_keyStore.allKeysAs(MediaKeyStatus::Released), WTFMove(message), SuccessValue::Succeeded);
                } else {
                    GST_WARNING("message of size %lu incorrectly formatted as session %s removal answer", buffer ? buffer->size() : 0,
                        m_sessionID.utf8().data());
                    callback(m_keyStore.allKeysAs(MediaKeyStatus::InternalError), WTF::nullopt, SuccessValue::Failed);
                }
            }
        } else {
            GST_WARNING("could not remove session %s", m_sessionID.utf8().data());
            callback(m_keyStore.allKeysAs(MediaKeyStatus::InternalError), WTF::nullopt, SuccessValue::Failed);
        }
    });
    if (!m_session || m_sessionID.isEmpty() || opencdm_session_remove(m_session.get()))
        sessionFailure();

    removeFromInstanceProxy();
}

void CDMInstanceSessionThunder::storeRecordOfKeyUsage(const String&)
{
    notImplemented();
}

Optional<CDMInstanceThunder&> CDMInstanceSessionThunder::cdmInstanceThunder() const
{
    auto instance = cdmInstanceProxy();
    if (!instance)
        return WTF::nullopt;
    return static_cast<CDMInstanceThunder&>(*instance);
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA) && ENABLE(THUNDER)
