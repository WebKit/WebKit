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
#include "SharedBuffer.h"
#include <wtf/JSONValues.h>
#include <wtf/MainThread.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/text/Base64.h>

namespace WebCore {

// ClearKey CENC SystemID.
// https://www.w3.org/TR/eme-initdata-cenc/#common-system
const uint8_t clearKeyCencSystemId[] = { 0x10, 0x77, 0xef, 0xec, 0xc0, 0xb2, 0x4d, 0x02, 0xac, 0xe3, 0x3c, 0x1e, 0x52, 0xe2, 0xfb, 0x4b };
const unsigned clearKeyCencSystemIdSize = sizeof(clearKeyCencSystemId);
const unsigned keyIdSize = 16;

class ClearKeyState {
    using KeyStore = HashMap<String, Vector<CDMInstanceClearKey::Key>>;

public:
    static ClearKeyState& singleton();

    KeyStore& keys() { return m_keys; }

private:
    friend class NeverDestroyed<ClearKeyState>;
    ClearKeyState();
    KeyStore m_keys;
};

ClearKeyState& ClearKeyState::singleton()
{
    static NeverDestroyed<ClearKeyState> s_state;
    return s_state;
}

ClearKeyState::ClearKeyState() = default;

static RefPtr<JSON::Object> parseJSONObject(const SharedBuffer& buffer)
{
    // Fail on large buffers whose size doesn't fit into a 32-bit unsigned integer.
    size_t size = buffer.size();
    if (size > std::numeric_limits<unsigned>::max())
        return nullptr;

    // Parse the buffer contents as JSON, returning the root object (if any).
    String json { buffer.data(), static_cast<unsigned>(size) };
    RefPtr<JSON::Value> value;
    RefPtr<JSON::Object> object;
    if (!JSON::Value::parseJSON(json, value) || !value->asObject(object))
        return nullptr;

    return object;
}

static std::optional<Vector<CDMInstanceClearKey::Key>> parseLicenseFormat(const JSON::Object& root)
{
    // If the 'keys' key is present in the root object, parse the JSON further
    // according to the specified 'license' format.
    auto it = root.find("keys");
    if (it == root.end())
        return std::nullopt;

    // Retrieve the keys array.
    RefPtr<JSON::Array> keysArray;
    if (!it->value->asArray(keysArray))
        return std::nullopt;

    Vector<CDMInstanceClearKey::Key> decodedKeys;
    bool validFormat = std::all_of(keysArray->begin(), keysArray->end(),
        [&decodedKeys] (const auto& value) {
            RefPtr<JSON::Object> keyObject;
            if (!value->asObject(keyObject))
                return false;

            String keyType;
            if (!keyObject->getString("kty", keyType) || !equalLettersIgnoringASCIICase(keyType, "oct"))
                return false;

            String keyID, keyValue;
            if (!keyObject->getString("kid", keyID) || !keyObject->getString("k", keyValue))
                return false;

            Vector<char> keyIDData, keyValueData;
            if (!WTF::base64URLDecode(keyID, { keyIDData }) || !WTF::base64URLDecode(keyValue, { keyValueData }))
                return false;

            decodedKeys.append({ CDMInstanceSession::KeyStatus::Usable, SharedBuffer::create(WTFMove(keyIDData)), SharedBuffer::create(WTFMove(keyValueData)) });
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
    RefPtr<JSON::Array> kidsArray;
    if (!it->value->asArray(kidsArray))
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

    const char* data = initData.data();
    unsigned initDataSize = initData.size();
    unsigned index = 0;
    unsigned psshSize = 0;

    // Search in the concatenated or the simple InitData, the ClearKey PSSH.
    bool foundPssh = false;
    while (true) {

        // Check the overflow InitData.
        if (index + 12 + clearKeyCencSystemIdSize >= initDataSize)
            return keyIdsMap;

        psshSize = data[index + 2] * 256 + data[index + 3];

        // Check the pssh size
        if (!psshSize)
            return keyIdsMap;

        // 12 = BMFF box header + Full box header.
        if (!memcmp(&data[index + 12], clearKeyCencSystemId, clearKeyCencSystemIdSize)) {
            foundPssh = true;
            break;
        }
        index += psshSize;
    }

    // Check if the InitData contains the ClearKey PSSH.
    if (!foundPssh)
        return keyIdsMap;

    index += (12 + clearKeyCencSystemIdSize); // 12 (BMFF box header + Full box header) + SystemID size.

    // Check the overflow.
    if (index + 3 >= initDataSize)
        return keyIdsMap;

    keyIdsMap.first = data[index + 3]; // Read the KeyIdsCount.
    index += 4; // KeyIdsCount size.

    // Check the overflow.
    if ((index + (keyIdsMap.first * keyIdSize)) >= initDataSize)
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
    Ref<SharedBuffer> keyIds = SharedBuffer::create();

    std::pair<unsigned, unsigned> keyIdsMap = extractKeyidsLocationFromCencInitData(initData);
    unsigned keyIdCount = keyIdsMap.first;
    unsigned index = keyIdsMap.second;

    // Check if initData is a valid CENC initData.
    if (!keyIdCount || !index)
        return keyIds;

    const char* data = initData.data();

    auto object = JSON::Object::create();
    auto keyIdsArray = JSON::Array::create();

    // Read the KeyId
    // 9.1.3 License Request Format
    // This section describes the format of the license request provided to the application via the message attribute of the message event.
    // The format is a JSON object containing the following members:
    // "kids"
    // An array of key IDs. Each element of the array is the base64url encoding of the octet sequence containing the key ID value.
    for (unsigned i = 0; i < keyIdCount; i++) {
        String keyId = WTF::base64URLEncode(&data[index], keyIdSize);
        keyIdsArray->pushString(keyId);
        index += keyIdSize;
    }

    object->setArray("kids", WTFMove(keyIdsArray));
    CString jsonData = object->toJSONString().utf8();
    keyIds->append(jsonData.data(), jsonData.length());
    return keyIds;
}

static Ref<SharedBuffer> extractKeyIdFromWebMInitData(const SharedBuffer& initData)
{
    Ref<SharedBuffer> keyIds = SharedBuffer::create();

    // Check if initData is a valid WebM initData.
    if (initData.isEmpty() || initData.size() > std::numeric_limits<unsigned>::max())
        return keyIds;

    auto object = JSON::Object::create();
    auto keyIdsArray = JSON::Array::create();

    // Read the KeyId
    // 9.1.3 License Request Format
    // This section describes the format of the license request provided to the application via the message attribute of the message event.
    // The format is a JSON object containing the following members:
    // "kids"
    // An array of key IDs. Each element of the array is the base64url encoding of the octet sequence containing the key ID value.
    String keyId = WTF::base64URLEncode(initData.data(), initData.size());
    keyIdsArray->pushString(keyId);

    object->setArray("kids", WTFMove(keyIdsArray));
    CString jsonData = object->toJSONString().utf8();
    keyIds->append(jsonData.data(), jsonData.length());
    return keyIds;
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
    return std::make_unique<CDMPrivateClearKey>();
}

bool CDMFactoryClearKey::supportsKeySystem(const String& keySystem)
{
    // `org.w3.clearkey` is the only supported key system.
    return equalLettersIgnoringASCIICase(keySystem, "org.w3.clearkey");
}

CDMPrivateClearKey::CDMPrivateClearKey() = default;
CDMPrivateClearKey::~CDMPrivateClearKey() = default;

bool CDMPrivateClearKey::supportsInitDataType(const AtomicString& initDataType) const
{
    // `keyids` and 'cenc' are the only supported init data type.
    return (equalLettersIgnoringASCIICase(initDataType, "keyids") || equalLettersIgnoringASCIICase(initDataType, "cenc") || equalLettersIgnoringASCIICase(initDataType, "webm"));
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

bool CDMPrivateClearKey::supportsSessionTypeWithConfiguration(CDMSessionType& sessionType, const CDMKeySystemConfiguration& configuration) const
{
    // Only support the 'temporary' and 'persistent-license' session types.
    if (sessionType != CDMSessionType::Temporary && sessionType != CDMSessionType::PersistentLicense)
        return false;
    return supportsConfiguration(configuration);
}

bool CDMPrivateClearKey::supportsRobustness(const String& robustness) const
{
    // Only empty `robustness` string is supported.
    return robustness.isEmpty();
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

bool CDMPrivateClearKey::supportsInitData(const AtomicString& initDataType, const SharedBuffer& initData) const
{
    // Validate the initData buffer as an JSON object in keyids case.
    if (equalLettersIgnoringASCIICase(initDataType, "keyids") && parseJSONObject(initData))
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
    if (!parseJSONObject(response))
        return nullptr;

    return response.copy();
}

std::optional<String> CDMPrivateClearKey::sanitizeSessionId(const String& sessionId) const
{
    // Validate the session ID string as an 32-bit integer.
    bool ok;
    sessionId.toUIntStrict(&ok);
    if (!ok)
        return std::nullopt;
    return sessionId;
}

CDMInstanceClearKey::CDMInstanceClearKey()
{
}

CDMInstanceClearKey::~CDMInstanceClearKey() = default;

CDMInstance::SuccessValue CDMInstanceClearKey::initializeWithConfiguration(const CDMKeySystemConfiguration&)
{
    // No-op.
    return Succeeded;
}

CDMInstance::SuccessValue CDMInstanceClearKey::setDistinctiveIdentifiersAllowed(bool allowed)
{
    // Reject setting distinctive identifiers as allowed.
    return !allowed ? Succeeded : Failed;
}

CDMInstance::SuccessValue CDMInstanceClearKey::setPersistentStateAllowed(bool allowed)
{
    // Reject setting persistent state as allowed.
    return !allowed ? Succeeded : Failed;
}

CDMInstance::SuccessValue CDMInstanceClearKey::setServerCertificate(Ref<SharedBuffer>&&)
{
    // Reject setting any server certificate.
    return Failed;
}

CDMInstance::SuccessValue CDMInstanceClearKey::setStorageDirectory(const String& storageDirectory)
{
    // Reject any persistent state storage.
    return storageDirectory.isEmpty() ? Succeeded : Failed;
}

const String& CDMInstanceClearKey::keySystem() const
{
    static const NeverDestroyed<String> s_keySystem { MAKE_STATIC_STRING_IMPL("org.w3.clearkey") };

    return s_keySystem;
}

RefPtr<CDMInstanceSession> CDMInstanceClearKey::createSession()
{
    return adoptRef(new CDMInstanceSessionClearKey());
}

const Vector<CDMInstanceClearKey::Key> CDMInstanceClearKey::keys() const
{
    // Return the keys of all sessions.
    Vector<CDMInstanceClearKey::Key> allKeys { };
    size_t initialCapacity = 0;
    for (auto& key : ClearKeyState::singleton().keys().values())
        initialCapacity += key.size();
    allKeys.reserveInitialCapacity(initialCapacity);

    for (auto& key : ClearKeyState::singleton().keys().values())
        allKeys.appendVector(key);

    return allKeys;
}

void CDMInstanceSessionClearKey::requestLicense(LicenseType, const AtomicString& initDataType, Ref<SharedBuffer>&& initData, LicenseCallback&& callback)
{
    static uint32_t s_sessionIdValue = 0;
    ++s_sessionIdValue;

    if (equalLettersIgnoringASCIICase(initDataType, "cenc"))
        initData = extractKeyidsFromCencInitData(initData.get());

    if (equalLettersIgnoringASCIICase(initDataType, "webm"))
        initData = extractKeyIdFromWebMInitData(initData.get());

    callOnMainThread(
        [weakThis = makeWeakPtr(*this), callback = WTFMove(callback), initData = WTFMove(initData), sessionIdValue = s_sessionIdValue]() mutable {
            if (!weakThis)
                return;

            callback(WTFMove(initData), String::number(sessionIdValue), false, Succeeded);
        });
}

void CDMInstanceSessionClearKey::updateLicense(const String& sessionId, LicenseType, const SharedBuffer& response, LicenseUpdateCallback&& callback)
{
    // Use a helper functor that schedules the callback dispatch, avoiding
    // duplicated callOnMainThread() calls.
    auto dispatchCallback =
        [this, &callback](bool sessionWasClosed, std::optional<KeyStatusVector>&& changedKeys, SuccessValue succeeded) {
            callOnMainThread(
                [weakThis = makeWeakPtr(*this), callback = WTFMove(callback), sessionWasClosed, changedKeys = WTFMove(changedKeys), succeeded] () mutable {
                    if (!weakThis)
                        return;

                    callback(sessionWasClosed, WTFMove(changedKeys), std::nullopt, std::nullopt, succeeded);
                });
        };

    // Parse the response buffer as an JSON object.
    RefPtr<JSON::Object> root = parseJSONObject(response);
    if (!root) {
        dispatchCallback(false, std::nullopt, SuccessValue::Failed);
        return;
    }

    // Parse the response using 'license' formatting, if possible.
    if (auto decodedKeys = parseLicenseFormat(*root)) {
        // Retrieve the target Vector of Key objects for this session.
        auto& keyVector = ClearKeyState::singleton().keys().ensure(sessionId, [] { return Vector<CDMInstanceClearKey::Key> { }; }).iterator->value;

        // For each decoded key, find an existing item for the decoded key's ID. If none exist,
        // the key is decoded. Otherwise, the key is updated in case there's a mismatch between
        // the size or data of the existing and proposed key.
        bool keysChanged = false;
        for (auto& key : *decodedKeys) {
            auto it = std::find_if(keyVector.begin(), keyVector.end(),
                [&key] (const CDMInstanceClearKey::Key& containedKey) {
                    return containedKey.keyIDData->size() == key.keyIDData->size()
                        && !std::memcmp(containedKey.keyIDData->data(), key.keyIDData->data(), containedKey.keyIDData->size());
                });
            if (it != keyVector.end()) {
                auto& existingKey = it->keyValueData;
                auto& proposedKey = key.keyValueData;

                // Update the existing Key if it differs from the proposed key in key value.
                if (existingKey->size() != proposedKey->size() || std::memcmp(existingKey->data(), proposedKey->data(), existingKey->size())) {
                    *it = WTFMove(key);
                    keysChanged = true;
                }
            } else {
                // In case a Key for this key ID doesn't exist yet, append the new one to keyVector.
                keyVector.append(WTFMove(key));
                keysChanged = true;
            }
        }

        // In case of changed keys, we have to provide a KeyStatusVector of all the keys for
        // this session.
        std::optional<KeyStatusVector> changedKeys;
        if (keysChanged) {
            // First a helper Vector is constructed, cotaining pairs of SharedBuffer RefPtrs
            // representint key ID data, and the corresponding key statuses.
            // We can't use KeyStatusVector here because this Vector has to be sorted, which
            // is not possible to do on Ref<> objects.
            Vector<std::pair<RefPtr<SharedBuffer>, KeyStatus>> keys;
            keys.reserveInitialCapacity(keyVector.size());
            for (auto& it : keyVector)
                keys.uncheckedAppend(std::pair<RefPtr<SharedBuffer>, KeyStatus> { it.keyIDData, it.status });

            // Sort first by size, second by data.
            std::sort(keys.begin(), keys.end(),
                [] (const auto& a, const auto& b) {
                    if (a.first->size() != b.first->size())
                        return a.first->size() < b.first->size();

                    return std::memcmp(a.first->data(), b.first->data(), a.first->size()) < 0;
                });

            // Finally construct the mirroring KeyStatusVector object and move it into the
            // std::optional<> object that will be passed to the callback.
            KeyStatusVector keyStatusVector;
            keyStatusVector.reserveInitialCapacity(keys.size());
            for (auto& it : keys)
                keyStatusVector.uncheckedAppend(std::pair<Ref<SharedBuffer>, KeyStatus> { *it.first, it.second });

            changedKeys = WTFMove(keyStatusVector);
        }

        dispatchCallback(false, WTFMove(changedKeys), SuccessValue::Succeeded);
        return;
    }

    // Parse the response using 'license release acknowledgement' formatting, if possible.
    if (parseLicenseReleaseAcknowledgementFormat(*root)) {
        // FIXME: Retrieve the key ID information and use it to validate the keys for this sessionId.
        ClearKeyState::singleton().keys().remove(sessionId);
        dispatchCallback(true, std::nullopt, SuccessValue::Succeeded);
        return;
    }

    // Bail in case no format was recognized.
    dispatchCallback(false, std::nullopt, SuccessValue::Failed);
}

void CDMInstanceSessionClearKey::loadSession(LicenseType, const String& sessionId, const String&, LoadSessionCallback&& callback)
{
    // Use a helper functor that schedules the callback dispatch, avoiding duplicated callOnMainThread() calls.
    auto dispatchCallback =
        [this, &callback](std::optional<KeyStatusVector>&& existingKeys, SuccessValue success, SessionLoadFailure loadFailure) {
            callOnMainThread(
                [weakThis = makeWeakPtr(*this), callback = WTFMove(callback), existingKeys = WTFMove(existingKeys), success, loadFailure]() mutable {
                    if (!weakThis)
                        return;

                    callback(WTFMove(existingKeys), std::nullopt, std::nullopt, success, loadFailure);
                });
        };

    // Construct the KeyStatusVector object, representing all the known keys for this session.
    KeyStatusVector keyStatusVector;
    {
        auto& keys = ClearKeyState::singleton().keys();
        auto it = keys.find(sessionId);
        if (it == keys.end()) {
            dispatchCallback(std::nullopt, Failed, SessionLoadFailure::NoSessionData);
            return;
        }

        auto& keyVector = it->value;
        keyStatusVector.reserveInitialCapacity(keyVector.size());
        for (auto& key : keyVector)
            keyStatusVector.uncheckedAppend(std::pair<Ref<SharedBuffer>, KeyStatus> { *key.keyIDData, key.status });
    }

    dispatchCallback(WTFMove(keyStatusVector), Succeeded, SessionLoadFailure::None);
}

void CDMInstanceSessionClearKey::closeSession(const String&, CloseSessionCallback&& callback)
{
    callOnMainThread(
        [weakThis = makeWeakPtr(*this), callback = WTFMove(callback)] () mutable {
            if (!weakThis)
                return;

            callback();
        });
}

void CDMInstanceSessionClearKey::removeSessionData(const String& sessionId, LicenseType, RemoveSessionDataCallback&& callback)
{
    // Use a helper functor that schedules the callback dispatch, avoiding duplicated callOnMainThread() calls.
    auto dispatchCallback =
        [this, &callback](KeyStatusVector&& keyStatusVector, std::optional<Ref<SharedBuffer>>&& message, SuccessValue success) {
            callOnMainThread(
                [weakThis = makeWeakPtr(*this), callback = WTFMove(callback), keyStatusVector = WTFMove(keyStatusVector), message = WTFMove(message), success]() mutable {
                    if (!weakThis)
                        return;

                    callback(WTFMove(keyStatusVector), WTFMove(message), success);
                });
        };

    // Construct the KeyStatusVector object, representing released keys, and the message in the
    // 'license release' format.
    KeyStatusVector keyStatusVector;
    RefPtr<SharedBuffer> message;
    {
        // Retrieve information for the given session ID, bailing if none is found.
        auto& keys = ClearKeyState::singleton().keys();
        auto it = keys.find(sessionId);
        if (it == keys.end()) {
            dispatchCallback(KeyStatusVector { }, std::nullopt, SuccessValue::Failed);
            return;
        }

        // Retrieve the Key vector, containing all the keys for this session, and
        // then remove the key map entry for this session.
        auto keyVector = WTFMove(it->value);
        keys.remove(it);

        // Construct the KeyStatusVector object, pairing key IDs with the 'released' status.
        keyStatusVector.reserveInitialCapacity(keyVector.size());
        for (auto& key : keyVector)
            keyStatusVector.uncheckedAppend(std::pair<Ref<SharedBuffer>, KeyStatus> { *key.keyIDData, KeyStatus::Released });

        // Construct JSON that represents the 'license release' format, creating a 'kids' array
        // of base64URL-encoded key IDs for all keys that were associated with this session.
        auto rootObject = JSON::Object::create();
        {
            auto array = JSON::Array::create();
            for (auto& key : keyVector) {
                ASSERT(key.keyIDData->size() <= std::numeric_limits<unsigned>::max());
                array->pushString(WTF::base64URLEncode(key.keyIDData->data(), static_cast<unsigned>(key.keyIDData->size())));
            }
            rootObject->setArray("kids", WTFMove(array));
        }

        // Copy the JSON data into a SharedBuffer object.
        String messageString = rootObject->toJSONString();
        CString messageCString = messageString.utf8();
        message = SharedBuffer::create(messageCString.data(), messageCString.length());
    }

    dispatchCallback(WTFMove(keyStatusVector), Ref<SharedBuffer>(*message), SuccessValue::Succeeded);
}

void CDMInstanceSessionClearKey::storeRecordOfKeyUsage(const String&)
{
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
