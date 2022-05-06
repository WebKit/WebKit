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

#include "config.h"
#include "CDMClearKey.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMKeySystemConfiguration.h"
#include "CDMRestrictions.h"
#include "CDMSessionType.h"
#include "CDMUtilities.h"
#include "InitDataRegistry.h"
#include "Logging.h"
#include "SharedBuffer.h"
#include <algorithm>
#include <iterator>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebCore {

static std::optional<Vector<RefPtr<KeyHandle>>> parseLicenseFormat(const JSON::Object& root)
{
    // If the 'keys' key is present in the root object, parse the JSON further
    // according to the specified 'license' format.
    auto it = root.find("keys");
    if (it == root.end())
        return std::nullopt;

    // Retrieve the keys array.
    auto keysArray = it->value->asArray();
    if (!keysArray)
        return std::nullopt;

    Vector<RefPtr<KeyHandle>> decodedKeys;
    bool validFormat = std::all_of(keysArray->begin(), keysArray->end(),
        [&decodedKeys] (const auto& value) {
            auto keyObject = value->asObject();
            if (!keyObject)
                return false;

            auto keyType = keyObject->getString("kty");
            if (!keyType || !equalLettersIgnoringASCIICase(keyType, "oct"))
                return false;

            auto keyID = keyObject->getString("kid");
            if (!keyID)
                return false;

            auto keyValue = keyObject->getString("k");
            if (!keyValue)
                return false;

            auto keyIDData = base64URLDecode(keyID);
            if (!keyIDData)
                return false;
        
            auto keyHandleValueData = base64URLDecode(keyValue);
            if (!keyHandleValueData)
                return false;

            decodedKeys.append(KeyHandle::create(CDMInstanceSession::KeyStatus::Usable, WTFMove(*keyIDData), WTFMove(*keyHandleValueData)));
            return true;
        });
    if (!validFormat)
        return std::nullopt;
    return decodedKeys;
}

static bool parseLicenseReleaseAcknowledgementFormat(const JSON::Object& root)
{
    // If the 'kids' key is present in the root object, parse the JSON further
    // according to the specified 'license release acknowledgement' format.
    auto it = root.find("kids");
    if (it == root.end())
        return false;

    // Retrieve the kids array.
    auto kidsArray = it->value->asArray();
    if (!kidsArray)
        return false;

    // FIXME: Return the key IDs and validate them.
    return true;
}

// https://www.w3.org/TR/eme-initdata-cenc/#common-system
// 4.1 Definition
// The SystemID is 1077efec-c0b2-4d02-ace3-3c1e52e2fb4b.
// The PSSH box format is as follows. It follows version 1 of the 'pssh' box as defined in [CENC].
// pssh = [
// 0x00, 0x00, 0x00, 0x4c, 0x70, 0x73, 0x73, 0x68, // BMFF box header (76 bytes, 'pssh')
// 0x01, 0x00, 0x00, 0x00,                         // Full box header (version = 1, flags = 0)
// 0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, // SystemID
// 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b,
// 0x00, 0x00, 0x00, 0x02,                         // KidCount (2)
// 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, // First KID ("0123456789012345")
// 0x38, 0x39, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35,
// 0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, // Second KID ("ABCDEFGHIJKLMNOP")
// 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4e, 0x4f, 0x50,
// 0x00, 0x00, 0x00, 0x00,                         // Size of Data (0)
// ];

// This function extracts the KeyIds count and the location of the first KeyId in initData buffer.
static std::pair<unsigned, unsigned> extractKeyidsLocationFromCencInitData(const SharedBuffer& initData)
{
    std::pair<unsigned, unsigned> keyIdsMap(0, 0);

    // Check the initData size.
    if (initData.isEmpty() || initData.size() > std::numeric_limits<unsigned>::max())
        return keyIdsMap;

    auto* data = initData.data();
    unsigned initDataSize = initData.size();
    unsigned index = 0;
    unsigned psshSize = 0;

    // Search in the concatenated or the simple InitData, the ClearKey PSSH.
    bool foundPssh = false;
    while (true) {

        // Check the overflow InitData.
        if (index + 12 + ClearKey::cencSystemIdSize >= initDataSize)
            return keyIdsMap;

        psshSize = data[index + 2] * 256 + data[index + 3];

        // Check the pssh size
        if (!psshSize)
            return keyIdsMap;

        // 12 = BMFF box header + Full box header.
        if (!memcmp(&data[index + 12], ClearKey::cencSystemId, ClearKey::cencSystemIdSize)) {
            foundPssh = true;
            break;
        }
        index += psshSize;
    }

    // Check if the InitData contains the ClearKey PSSH.
    if (!foundPssh)
        return keyIdsMap;

    index += (12 + ClearKey::cencSystemIdSize); // 12 (BMFF box header + Full box header) + SystemID size.

    // Check the overflow.
    if (index + 3 >= initDataSize)
        return keyIdsMap;

    keyIdsMap.first = data[index + 3]; // Read the KeyIdsCount.
    index += 4; // KeyIdsCount size.

    // Check the overflow.
    if ((index + (keyIdsMap.first * ClearKey::KeyIDSizeInBytes)) >= initDataSize)
        return keyIdsMap;

    keyIdsMap.second = index; // The location of the first KeyId in initData.

    return keyIdsMap;
}

// This function checks if the initData sharedBuffer is a valid CENC initData.
static bool isCencInitData(const SharedBuffer& initData)
{
    std::pair<unsigned, unsigned> keyIdsMap = extractKeyidsLocationFromCencInitData(initData);
    return ((keyIdsMap.first) && (keyIdsMap.second));
}

static Ref<SharedBuffer> extractKeyidsFromCencInitData(const SharedBuffer& initData)
{
    std::pair<unsigned, unsigned> keyIdsMap = extractKeyidsLocationFromCencInitData(initData);
    unsigned keyIdCount = keyIdsMap.first;
    unsigned index = keyIdsMap.second;

    // Check if initData is a valid CENC initData.
    if (!keyIdCount || !index)
        return SharedBuffer::create();
    auto* data = initData.data();

    auto object = JSON::Object::create();
    auto keyIdsArray = JSON::Array::create();

    // Read the KeyId
    // 9.1.3 License Request Format
    // This section describes the format of the license request provided to the application via the message attribute of the message event.
    // The format is a JSON object containing the following members:
    // "kids"
    // An array of key IDs. Each element of the array is the base64url encoding of the octet sequence containing the key ID value.
    for (unsigned i = 0; i < keyIdCount; i++) {
        keyIdsArray->pushString(base64URLEncodeToString(&data[index], ClearKey::KeyIDSizeInBytes));
        index += ClearKey::KeyIDSizeInBytes;
    }

    object->setArray("kids", WTFMove(keyIdsArray));
    CString jsonData = object->toJSONString().utf8();
    return SharedBuffer::create(jsonData.data(), jsonData.length());
}

static Ref<SharedBuffer> extractKeyIdFromWebMInitData(const SharedBuffer& initData)
{
    // Check if initData is a valid WebM initData.
    if (initData.isEmpty() || initData.size() > std::numeric_limits<unsigned>::max())
        return SharedBuffer::create();

    auto object = JSON::Object::create();
    auto keyIdsArray = JSON::Array::create();

    // Read the KeyId
    // 9.1.3 License Request Format
    // This section describes the format of the license request provided to the application via the message attribute of the message event.
    // The format is a JSON object containing the following members:
    // "kids"
    // An array of key IDs. Each element of the array is the base64url encoding of the octet sequence containing the key ID value.
    keyIdsArray->pushString(base64URLEncodeToString(initData.data(), initData.size()));

    object->setArray("kids", WTFMove(keyIdsArray));
    CString jsonData = object->toJSONString().utf8();
    return SharedBuffer::create(jsonData.data(), jsonData.length());
}

CDMFactoryClearKey& CDMFactoryClearKey::singleton()
{
    static NeverDestroyed<CDMFactoryClearKey> s_factory;
    return s_factory;
}

CDMFactoryClearKey::CDMFactoryClearKey() = default;
CDMFactoryClearKey::~CDMFactoryClearKey() = default;

std::unique_ptr<CDMPrivate> CDMFactoryClearKey::createCDM(const String& keySystem)
{
#ifdef NDEBUG
    UNUSED_PARAM(keySystem);
#else
    ASSERT(supportsKeySystem(keySystem));
#endif
    return makeUnique<CDMPrivateClearKey>();
}

bool CDMFactoryClearKey::supportsKeySystem(const String& keySystem)
{
    // `org.w3.clearkey` is the only supported key system.
    return equalLettersIgnoringASCIICase(keySystem, "org.w3.clearkey");
}

CDMPrivateClearKey::CDMPrivateClearKey() = default;
CDMPrivateClearKey::~CDMPrivateClearKey() = default;

Vector<AtomString> CDMPrivateClearKey::supportedInitDataTypes() const
{
    return {
        InitDataRegistry::keyidsName(),
        InitDataRegistry::cencName(),
        InitDataRegistry::webmName(),
    };
}

static bool containsPersistentLicenseType(const Vector<CDMSessionType>& types)
{
    return std::any_of(types.begin(), types.end(),
        [] (auto& sessionType) { return sessionType == CDMSessionType::PersistentLicense; });
}

bool CDMPrivateClearKey::supportsConfiguration(const CDMKeySystemConfiguration& configuration) const
{
    // Reject any configuration that marks distinctive identifier as required.
    if (configuration.distinctiveIdentifier == CDMRequirement::Required)
        return false;

    // Reject any configuration that marks persistent state as required, unless
    // the 'persistent-license' session type has to be supported.
    if (configuration.persistentState == CDMRequirement::Required && !containsPersistentLicenseType(configuration.sessionTypes))
        return false;

    return true;
}

bool CDMPrivateClearKey::supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration& configuration, const CDMRestrictions& restrictions) const
{
    // Reject any configuration that marks distincitive identifier as required, or that marks
    // distinctive identifier as optional even when restrictions mark it as denied.
    if ((configuration.distinctiveIdentifier == CDMRequirement::Optional && restrictions.distinctiveIdentifierDenied)
        || configuration.distinctiveIdentifier == CDMRequirement::Required)
        return false;

    // Reject any configuration that marks persistent state as optional even when
    // restrictions mark it as denied.
    if (configuration.persistentState == CDMRequirement::Optional && restrictions.persistentStateDenied)
        return false;

    // Reject any configuration that marks persistent state as required, unless
    // the 'persistent-license' session type has to be supported.
    if (configuration.persistentState == CDMRequirement::Required && !containsPersistentLicenseType(configuration.sessionTypes))
        return false;

    return true;
}

bool CDMPrivateClearKey::supportsSessionTypeWithConfiguration(const CDMSessionType& sessionType, const CDMKeySystemConfiguration& configuration) const
{
    // Only support the 'temporary' and 'persistent-license' session types.
    if (sessionType != CDMSessionType::Temporary && sessionType != CDMSessionType::PersistentLicense)
        return false;
    return supportsConfiguration(configuration);
}

Vector<AtomString> CDMPrivateClearKey::supportedRobustnesses() const
{
    // Only empty `robustness` string is supported.
    return { emptyAtom() };
}

CDMRequirement CDMPrivateClearKey::distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions& restrictions) const
{
    // Distinctive identifier is not allowed if it's been denied, otherwise it's optional.
    if (restrictions.distinctiveIdentifierDenied)
        return CDMRequirement::NotAllowed;
    return CDMRequirement::Optional;
}

CDMRequirement CDMPrivateClearKey::persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions& restrictions) const
{
    // Persistent state is not allowed if it's been denied, otherwise it's optional.
    if (restrictions.persistentStateDenied)
        return CDMRequirement::NotAllowed;
    return CDMRequirement::Optional;
}

bool CDMPrivateClearKey::distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const
{
    return false;
}

RefPtr<CDMInstance> CDMPrivateClearKey::createInstance()
{
    return adoptRef(new CDMInstanceClearKey);
}

void CDMPrivateClearKey::loadAndInitialize()
{
    // No-op.
}

bool CDMPrivateClearKey::supportsServerCertificates() const
{
    // Server certificates are not supported.
    return false;
}

bool CDMPrivateClearKey::supportsSessions() const
{
    // Sessions are supported.
    return true;
}

bool CDMPrivateClearKey::supportsInitData(const AtomString& initDataType, const SharedBuffer& initData) const
{
    // Validate the initData buffer as an JSON object in keyids case.
    if (equalLettersIgnoringASCIICase(initDataType, "keyids") && CDMUtilities::parseJSONObject(initData))
        return true;

    // Validate the initData buffer as CENC initData.
    if (equalLettersIgnoringASCIICase(initDataType, "cenc") && isCencInitData(initData))
        return true;

    // Validate the initData buffer as WebM initData.
    if (equalLettersIgnoringASCIICase(initDataType, "webm") && !initData.isEmpty())
        return true;

    return false;
}

RefPtr<SharedBuffer> CDMPrivateClearKey::sanitizeResponse(const SharedBuffer& response) const
{
    // Validate the response buffer as an JSON object.
    if (!CDMUtilities::parseJSONObject(response))
        return nullptr;

    return response.makeContiguous();
}

std::optional<String> CDMPrivateClearKey::sanitizeSessionId(const String& sessionId) const
{
    // Validate the session ID string as an 32-bit integer.
    if (!parseInteger<uint32_t>(sessionId))
        return std::nullopt;
    return sessionId;
}

CDMInstanceClearKey::CDMInstanceClearKey() : CDMInstanceProxy("org.w3.clearkey"_s) { };
CDMInstanceClearKey::~CDMInstanceClearKey() = default;

void CDMInstanceClearKey::initializeWithConfiguration(const CDMKeySystemConfiguration&, AllowDistinctiveIdentifiers distinctiveIdentifiers, AllowPersistentState persistentState, SuccessCallback&& callback)
{
    SuccessValue succeeded = (distinctiveIdentifiers == AllowDistinctiveIdentifiers::No && persistentState == AllowPersistentState::No) ? Succeeded : Failed;
    callback(succeeded);
}

void CDMInstanceClearKey::setServerCertificate(Ref<SharedBuffer>&&, SuccessCallback&& callback)
{
    // Reject setting any server certificate.
    callback(Failed);
}

void CDMInstanceClearKey::setStorageDirectory(const String&)
{
}

const String& CDMInstanceClearKey::keySystem() const
{
    static const NeverDestroyed<String> s_keySystem { MAKE_STATIC_STRING_IMPL("org.w3.clearkey") };

    return s_keySystem;
}

RefPtr<CDMInstanceSession> CDMInstanceClearKey::createSession()
{
    return adoptRef(new CDMInstanceSessionClearKey(*this));
}

void CDMInstanceSessionClearKey::requestLicense(LicenseType, const AtomString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    static uint32_t s_sessionIdValue = 0;
    ++s_sessionIdValue;

    m_sessionID = String::number(s_sessionIdValue);

    if (equalLettersIgnoringASCIICase(initDataType, "cenc"))
        initData = extractKeyidsFromCencInitData(initData.get());

    if (equalLettersIgnoringASCIICase(initDataType, "webm"))
        initData = extractKeyIdFromWebMInitData(initData.get());

    callOnMainThread(
        [this, weakThis = WeakPtr { *this }, callback = WTFMove(callback), initData = WTFMove(initData)]() mutable {
            if (!weakThis)
                return;

            callback(WTFMove(initData), m_sessionID, false, Succeeded);
        });
}

void CDMInstanceSessionClearKey::updateLicense(const String& sessionId, LicenseType, Ref<SharedBuffer>&& response, LicenseUpdateCallback&& callback)
{
#if LOG_DISABLED
    // We only use the sesion ID for debug logging. The verbose preprocessor checks are because
    // the Mac port has -Werror -Wunused-parameter.
    UNUSED_PARAM(sessionId);
#endif
    auto dispatchCallback =
        [weakThis = WeakPtr { *this }, &callback](bool sessionWasClosed, std::optional<KeyStatusVector>&& changedKeys, SuccessValue succeeded) {
            callOnMainThread(
                [weakThis = WeakPtr { *weakThis }, callback = WTFMove(callback), sessionWasClosed, changedKeys = WTFMove(changedKeys), succeeded] () mutable {
                    if (!weakThis)
                        return;

                    callback(sessionWasClosed, WTFMove(changedKeys), std::nullopt, std::nullopt, succeeded);
                });
        };

    RefPtr<JSON::Object> root = CDMUtilities::parseJSONObject(response);
    if (!root) {
        LOG(EME, "EME - ClearKey - session %s update payload was not valid JSON", sessionId.utf8().data());
        dispatchCallback(false, std::nullopt, SuccessValue::Failed);
        return;
    }

    LOG(EME, "EME - ClearKey - updating license for session %s which currently contains %u keys", sessionId.utf8().data(), m_keyStore.numKeys());

    if (auto decodedKeys = parseLicenseFormat(*root)) {
        bool keysChanged = m_keyStore.addKeys(WTFMove(*decodedKeys));

        LOG(EME, "EME - ClearKey - session %s has %u keys after update()", sessionId.utf8().data(), m_keyStore.numKeys());

        std::optional<KeyStatusVector> changedKeys;
        if (keysChanged) {
            LOG(EME, "EME - ClearKey - session %s has changed keys", sessionId.utf8().data());
            parentInstance().mergeKeysFrom(m_keyStore);
            changedKeys = m_keyStore.convertToJSKeyStatusVector();
        }

        dispatchCallback(false, WTFMove(changedKeys), SuccessValue::Succeeded);
        return;
    }

    if (parseLicenseReleaseAcknowledgementFormat(*root)) {
        LOG(EME, "EME - ClearKey - session %s release acknowledged, clearing all known keys", sessionId.utf8().data());
        parentInstance().unrefAllKeysFrom(m_keyStore);
        m_keyStore.unrefAllKeys();
        dispatchCallback(true, std::nullopt, SuccessValue::Succeeded);
        return;
    }

    LOG(EME, "EME - ClearKey - session %s update payload was an unrecognized format", sessionId.utf8().data());
    dispatchCallback(false, std::nullopt, SuccessValue::Failed);
}

void CDMInstanceSessionClearKey::loadSession(LicenseType, const String& sessionId, const String&, LoadSessionCallback&& callback)
{
#ifdef NDEBUG
    UNUSED_PARAM(sessionId);
#endif

    ASSERT(sessionId == m_sessionID);
    KeyStatusVector keyStatusVector = m_keyStore.convertToJSKeyStatusVector();
    callOnMainThread([weakThis = WeakPtr { *this }, callback = WTFMove(callback), &keyStatusVector]() mutable {
        if (!weakThis)
            return;

        callback(WTFMove(keyStatusVector), std::nullopt, std::nullopt, Succeeded, SessionLoadFailure::None);
    });
}

void CDMInstanceSessionClearKey::closeSession(const String&, CloseSessionCallback&& callback)
{
    callOnMainThread(
        [weakThis = WeakPtr { *this }, callback = WTFMove(callback)] () mutable {
            if (!weakThis)
                return;

            callback();
        });
}

void CDMInstanceSessionClearKey::removeSessionData(const String& sessionId, LicenseType, RemoveSessionDataCallback&& callback)
{
#ifdef NDEBUG
    UNUSED_PARAM(sessionId);
#endif

    ASSERT(sessionId == m_sessionID);

    auto dispatchCallback =
        [weakThis = WeakPtr { *this }, &callback](KeyStatusVector&& keyStatusVector, RefPtr<SharedBuffer>&& message, SuccessValue success) {
            callOnMainThread(
                [weakThis = WeakPtr { *weakThis }, callback = WTFMove(callback), keyStatusVector = WTFMove(keyStatusVector), message = WTFMove(message), success]() mutable {
                    if (!weakThis)
                        return;

                    callback(WTFMove(keyStatusVector), WTFMove(message), success);
                });
        };

    // Construct the KeyStatusVector object, representing released keys, and the message in the
    // 'license release' format.
    KeyStatusVector keyStatusVector = m_keyStore.allKeysAs(CDMInstanceSession::KeyStatus::Released);
    RefPtr<SharedBuffer> message;
    {
        // Construct JSON that represents the 'license release' format, creating a 'kids' array
        // of base64URL-encoded key IDs for all keys that were associated with this session.
        auto rootObject = JSON::Object::create();
        {
            auto array = JSON::Array::create();
            for (const auto& key : m_keyStore) {
                ASSERT(key->id().size() <= std::numeric_limits<unsigned>::max());
                array->pushString(base64URLEncodeToString(key->id().data(), key->id().size()));
            }
            rootObject->setArray("kids", WTFMove(array));
        }

        // Copy the JSON data into a SharedBuffer object.
        String messageString = rootObject->toJSONString();
        CString messageCString = messageString.utf8();
        message = SharedBuffer::create(messageCString.data(), messageCString.length());
    }

    m_keyStore.unrefAllKeys();
    dispatchCallback(WTFMove(keyStatusVector), Ref<SharedBuffer>(*message), SuccessValue::Succeeded);
}

void CDMInstanceSessionClearKey::storeRecordOfKeyUsage(const String&)
{
}

CDMInstanceClearKey& CDMInstanceSessionClearKey::parentInstance() const
{
    auto instance = cdmInstanceProxy();
    ASSERT(instance);
    return static_cast<CDMInstanceClearKey&>(*instance);
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
