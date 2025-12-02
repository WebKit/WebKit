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

#include "config.h"
#include "CDMFairPlayStreaming.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDM.h"
#include "CDMClearKey.h"
#include "CDMKeySystemConfiguration.h"
#include "CDMRestrictions.h"
#include "CDMSessionType.h"
#include "ISOProtectionSystemSpecificHeaderBox.h"
#include "ISOSchemeInformationBox.h"
#include "ISOSchemeTypeBox.h"
#include "ISOTrackEncryptionBox.h"
#include "InitDataRegistry.h"
#include "Logging.h"
#include "NotImplemented.h"
#include <JavaScriptCore/ArrayBuffer.h>
#include <JavaScriptCore/DataView.h>
#include <algorithm>
#include <wtf/JSONValues.h>
#include <wtf/LoggerHelper.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RobinHoodHashSet.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/Base64.h>

#if HAVE(AVCONTENTKEYSESSION)
#include "CDMInstanceFairPlayStreamingAVFObjC.h"
#endif

#if HAVE(FAIRPLAYSTREAMING_CENC_INITDATA)
#include "ISOFairPlayStreamingPsshBox.h"
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(CDMFactoryFairPlayStreaming);
WTF_MAKE_TZONE_ALLOCATED_IMPL(CDMPrivateFairPlayStreaming);

#if !RELEASE_LOG_DISABLED
static WTFLogChannel& logChannel() { return LogEME; }
#endif

const Vector<FourCC>& CDMPrivateFairPlayStreaming::validFairPlayStreamingSchemes()
{
    static NeverDestroyed<Vector<FourCC>> validSchemes = Vector<FourCC>({
        std::span { "cbcs" },
        std::span { "cbc2" },
        std::span { "cbc1" },
        std::span { "cenc" }
    });

    return validSchemes;
}

const String& CDMPrivateFairPlayStreaming::sinfName()
{
    static NeverDestroyed<const String> staticSinfName(MAKE_STATIC_STRING_IMPL("sinf"));
    return staticSinfName;
}

const String& CDMPrivateFairPlayStreaming::skdName()
{
    static NeverDestroyed<const String> staticSkdName(MAKE_STATIC_STRING_IMPL("skd"));
    return staticSkdName;
}

static Vector<Ref<SharedBuffer>> extractSinfData(const SharedBuffer& buffer)
{
    // JSON of the format: "{ sinf: [ <base64-encoded-string> ] }"
    if (buffer.size() > std::numeric_limits<unsigned>::max())
        return { };

    auto value = JSON::Value::parseJSON(byteCast<Latin1Character>(buffer.makeContiguous()->span()));
    if (!value)
        return { };

    auto object = value->asObject();
    if (!object)
        return { };

    auto sinfArray = object->getArray(CDMPrivateFairPlayStreaming::sinfName());
    if (!sinfArray)
        return { };

    return WTF::compactMap(*sinfArray, [](auto& value) -> RefPtr<SharedBuffer> {
        auto keyID = value->asString();
        if (!keyID)
            return nullptr;

        auto sinfData = base64Decode(keyID, { Base64DecodeOption::ValidatePadding, Base64DecodeOption::IgnoreWhitespace });
        if (!sinfData)
            return nullptr;

        return SharedBuffer::create(WTFMove(*sinfData));
    });
}

using SchemeAndKeyResult = Vector<std::pair<FourCC, Vector<uint8_t>>>;
static SchemeAndKeyResult extractSchemeAndKeyIdFromSinf(const SharedBuffer& buffer)
{
    auto buffers = extractSinfData(buffer);
    if (!buffers.size())
        return { };

    SchemeAndKeyResult result;
    for (auto& buffer : buffers) {
        unsigned offset = 0;
        std::optional<FourCC> scheme;
        std::optional<Vector<uint8_t>> keyID;

        auto view = JSC::DataView::create(buffer->tryCreateArrayBuffer(), offset, buffer->size());
        while (auto optionalBoxType = ISOBox::peekBox(view, offset)) {
            auto& boxTypeName = optionalBoxType.value().first;
            auto& boxSize = optionalBoxType.value().second;

            if (boxTypeName == ISOSchemeTypeBox::boxTypeName()) {
                ISOSchemeTypeBox schemeTypeBox;
                if (!schemeTypeBox.read(view, offset))
                    break;

                scheme = schemeTypeBox.schemeType();
                continue;
            }

            if (boxTypeName == ISOSchemeInformationBox::boxTypeName()) {
                ISOSchemeInformationBox schemeInfoBox;
                if (!schemeInfoBox.read(view, offset))
                    break;

                auto trackEncryptionBox = downcast<ISOTrackEncryptionBox>(schemeInfoBox.schemeSpecificData());
                if (trackEncryptionBox)
                    keyID = trackEncryptionBox->defaultKID();
                continue;
            }

            offset += boxSize;
        }
        if (scheme && keyID)
            result.append(std::make_pair(scheme.value(), WTFMove(keyID.value())));
    }

    return result;
}

std::optional<Vector<Ref<SharedBuffer>>> CDMPrivateFairPlayStreaming::extractKeyIDsSinf(const SharedBuffer& buffer)
{
    Vector<Ref<SharedBuffer>> keyIDs;
    auto results = extractSchemeAndKeyIdFromSinf(buffer);
    if (results.isEmpty())
        return std::nullopt;

    for (auto& result : results) {
        if (validFairPlayStreamingSchemes().contains(result.first))
            keyIDs.append(SharedBuffer::create(WTFMove(result.second)));
    }

    return keyIDs;
}

RefPtr<SharedBuffer> CDMPrivateFairPlayStreaming::sanitizeSinf(const SharedBuffer& buffer)
{
    // Common SINF Box Format
    UNUSED_PARAM(buffer);
    notImplemented();
    return buffer.makeContiguous();
}

RefPtr<SharedBuffer> CDMPrivateFairPlayStreaming::sanitizeSkd(const SharedBuffer& buffer)
{
    UNUSED_PARAM(buffer);
    notImplemented();
    return buffer.makeContiguous();
}

std::optional<Vector<Ref<SharedBuffer>>> CDMPrivateFairPlayStreaming::extractKeyIDsSkd(const SharedBuffer& buffer)
{
    // In the 'skd' scheme, the init data is the key ID.
    return Vector { buffer.makeContiguous() };
}

#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
const String& CDMPrivateFairPlayStreaming::mptsName()
{
    static NeverDestroyed<const String> staticMptsName(MAKE_STATIC_STRING_IMPL("mpts"));
    return staticMptsName;
}

std::optional<Vector<Ref<SharedBuffer>>> CDMPrivateFairPlayStreaming::extractKeyIDsMpts(const SharedBuffer& buffer)
{
    // JSON of the format: "{ "codc" : [integer],  "mtyp" : [integer],  "cont" : "mpts"} }"
    if (buffer.size() > std::numeric_limits<unsigned>::max())
        return { };

    auto value = JSON::Value::parseJSON(byteCast<Latin1Character>(buffer.makeContiguous()->span()));
    if (!value)
        return { };

    auto object = value->asObject();
    if (!object)
        return { };

    auto contValue = object->getString("cont"_s);
    if (contValue != "mpts"_s)
        return { };

    auto codcValue = object->getInteger("codc"_s);
    if (!codcValue)
        return { };

    auto mtypValue = object->getInteger("mtyp"_s);
    if (!mtypValue)
        return { };

    return mptsKeyIDs();
}

RefPtr<SharedBuffer> CDMPrivateFairPlayStreaming::sanitizeMpts(const SharedBuffer& buffer)
{
    UNUSED_PARAM(buffer);
    notImplemented();
    return buffer.makeContiguous();
}

const Vector<Ref<SharedBuffer>>& CDMPrivateFairPlayStreaming::mptsKeyIDs() {
    static NeverDestroyed<Vector<Ref<SharedBuffer>>> mptsKeyID = [] {
        return Vector { 1, SharedBuffer::create("TransportStreamIdentifier"_span) };
    }();
    return mptsKeyID;
}
#endif

static const MemoryCompactLookupOnlyRobinHoodHashSet<String>& validInitDataTypes()
{
    static NeverDestroyed<MemoryCompactLookupOnlyRobinHoodHashSet<String>> validTypes(std::initializer_list<String> {
        CDMPrivateFairPlayStreaming::sinfName(),
        CDMPrivateFairPlayStreaming::skdName(),
#if HAVE(FAIRPLAYSTREAMING_CENC_INITDATA)
        InitDataRegistry::cencName(),
#endif
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
        CDMPrivateFairPlayStreaming::mptsName(),
#endif
    });
    return validTypes;
}

void CDMFactory::platformRegisterFactories(Vector<WeakRef<CDMFactory>>& factories)
{
    factories.append(CDMFactoryClearKey::singleton());
    factories.append(CDMFactoryFairPlayStreaming::singleton());

    static std::once_flag onceFlag;
    std::call_once(onceFlag, [] {
        InitDataRegistry::singleton().registerInitDataType(CDMPrivateFairPlayStreaming::sinfName(), { CDMPrivateFairPlayStreaming::sanitizeSinf, CDMPrivateFairPlayStreaming::extractKeyIDsSinf });
        InitDataRegistry::singleton().registerInitDataType(CDMPrivateFairPlayStreaming::skdName(), { CDMPrivateFairPlayStreaming::sanitizeSkd, CDMPrivateFairPlayStreaming::extractKeyIDsSkd });
#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
        InitDataRegistry::singleton().registerInitDataType(CDMPrivateFairPlayStreaming::mptsName(), { CDMPrivateFairPlayStreaming::sanitizeMpts, CDMPrivateFairPlayStreaming::extractKeyIDsMpts });
#endif
    });
}

CDMFactoryFairPlayStreaming& CDMFactoryFairPlayStreaming::singleton()
{
    static NeverDestroyed<CDMFactoryFairPlayStreaming> s_factory;
    return s_factory;
}

CDMFactoryFairPlayStreaming::CDMFactoryFairPlayStreaming() = default;
CDMFactoryFairPlayStreaming::~CDMFactoryFairPlayStreaming() = default;

std::unique_ptr<CDMPrivate> CDMFactoryFairPlayStreaming::createCDM(const String& keySystem, const String& mediaKeysHashSalt, const CDMPrivateClient& client)
{
    if (!supportsKeySystem(keySystem))
        return nullptr;

    return makeUnique<CDMPrivateFairPlayStreaming>(mediaKeysHashSalt, client);
}

bool CDMFactoryFairPlayStreaming::supportsKeySystem(const String& keySystem)
{
    // https://w3c.github.io/encrypted-media/#key-system
    // "Key System strings are compared using case-sensitive matching."
    return keySystem == "com.apple.fps"_s || keySystem.startsWith("com.apple.fps."_s);
}

CDMPrivateFairPlayStreaming::CDMPrivateFairPlayStreaming(const String& mediaKeysHashSalt, const CDMPrivateClient& client)
    : m_mediaKeysHashSalt { mediaKeysHashSalt }
#if !RELEASE_LOG_DISABLED
    , m_logger { client.logger() }
#endif
{
}

CDMPrivateFairPlayStreaming::~CDMPrivateFairPlayStreaming() = default;

Vector<String> CDMPrivateFairPlayStreaming::supportedInitDataTypes() const
{
    return copyToVector(validInitDataTypes());
}

bool CDMPrivateFairPlayStreaming::supportsConfiguration(const CDMKeySystemConfiguration& configuration) const
{
    if (std::ranges::none_of(configuration.initDataTypes, [](auto& initDataType) { return validInitDataTypes().contains(initDataType); })) {
        INFO_LOG(LOGIDENTIFIER, " false, no initDataType supported");
        return false;
    }

#if HAVE(AVCONTENTKEYSESSION)
    // FIXME: verify that FairPlayStreaming does not (and cannot) expose a distinctive identifier to the client
    if (configuration.distinctiveIdentifier == CDMRequirement::Required) {
        INFO_LOG(LOGIDENTIFIER, "false, requried distinctiveIdentifier not supported");
        return false;
    }

    if (configuration.persistentState == CDMRequirement::Required && !CDMInstanceFairPlayStreamingAVFObjC::supportsPersistableState()) {
        INFO_LOG(LOGIDENTIFIER, "false, required persistentState not supported");
        return false;
    }

    if (configuration.sessionTypes.contains(CDMSessionType::PersistentLicense)
        && !configuration.sessionTypes.contains(CDMSessionType::Temporary)
        && !CDMInstanceFairPlayStreamingAVFObjC::supportsPersistentKeys()) {
        INFO_LOG(LOGIDENTIFIER, "false, sessionType PersistentLicense not supported");
        return false;
    }

    if (!configuration.audioCapabilities.isEmpty()
        && std::ranges::none_of(configuration.audioCapabilities, CDMInstanceFairPlayStreamingAVFObjC::supportsMediaCapability)) {
        INFO_LOG(LOGIDENTIFIER, "false, no audio configuration supported");
        return false;
    }

    if (!configuration.videoCapabilities.isEmpty()
        && std::ranges::none_of(configuration.videoCapabilities, CDMInstanceFairPlayStreamingAVFObjC::supportsMediaCapability)) {
            INFO_LOG(LOGIDENTIFIER, "false, no video configuration supported");
        return false;
    }

    INFO_LOG(LOGIDENTIFIER, "true, supported");
    return true;
#else
    return false;
#endif    
}

bool CDMPrivateFairPlayStreaming::supportsConfigurationWithRestrictions(const CDMKeySystemConfiguration& configuration, const CDMRestrictions& restrictions) const
{
    if (restrictions.persistentStateDenied
        && !configuration.sessionTypes.isEmpty()
        && !configuration.sessionTypes.contains(CDMSessionType::Temporary))
        return false;

    if (restrictions.persistentStateDenied && configuration.persistentState == CDMRequirement::Required)
        return false;

    if (std::ranges::all_of(configuration.sessionTypes, [restrictions](auto& sessionType) {
        return restrictions.deniedSessionTypes.contains(sessionType);
    }))
        return false;

    return supportsConfiguration(configuration);
}

bool CDMPrivateFairPlayStreaming::supportsSessionTypeWithConfiguration(const CDMSessionType& sessionType, const CDMKeySystemConfiguration& configuration) const
{
    if (sessionType == CDMSessionType::Temporary) {
        if (configuration.persistentState == CDMRequirement::Required)
            return false;
    } else if (configuration.persistentState == CDMRequirement::NotAllowed)
        return false;

    return supportsConfiguration(configuration);
}

Vector<String> CDMPrivateFairPlayStreaming::supportedRobustnesses() const
{
    // FIXME: Determine an enumerated list of robustness values supported by FPS.
    return { emptyString() };
}

CDMRequirement CDMPrivateFairPlayStreaming::distinctiveIdentifiersRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const
{
    // FIXME: verify that FairPlayStreaming does not (and cannot) expose a distinctive identifier to the client
    return CDMRequirement::NotAllowed;
}

CDMRequirement CDMPrivateFairPlayStreaming::persistentStateRequirement(const CDMKeySystemConfiguration&, const CDMRestrictions&) const
{
    return CDMRequirement::Optional;
}

bool CDMPrivateFairPlayStreaming::distinctiveIdentifiersAreUniquePerOriginAndClearable(const CDMKeySystemConfiguration&) const
{
    return true;
}

RefPtr<CDMInstance> CDMPrivateFairPlayStreaming::createInstance()
{
#if HAVE(AVCONTENTKEYSESSION)
    auto instance = adoptRef(new CDMInstanceFairPlayStreamingAVFObjC(*this));
#if !RELEASE_LOG_DISABLED
    instance->setLogIdentifier(m_logIdentifier);
#endif
    return instance;
#else
    return nullptr;
#endif
}

void CDMPrivateFairPlayStreaming::loadAndInitialize()
{
}

bool CDMPrivateFairPlayStreaming::supportsServerCertificates() const
{
    return true;
}

bool CDMPrivateFairPlayStreaming::supportsSessions() const
{
    return true;
}

bool CDMPrivateFairPlayStreaming::supportsInitData(const String& initDataType, const SharedBuffer& initData) const
{
    if (!validInitDataTypes().contains(initDataType))
        return false;

    if (initDataType == sinfName()) {
        return std::ranges::any_of(extractSchemeAndKeyIdFromSinf(initData), [](auto& result) {
            return validFairPlayStreamingSchemes().contains(result.first);
        });
    }

#if HAVE(FAIRPLAYSTREAMING_CENC_INITDATA)
    if (initDataType == InitDataRegistry::cencName()) {
        auto psshBoxes = InitDataRegistry::extractPsshBoxesFromCenc(initData);
        if (!psshBoxes)
            return false;

        return std::ranges::any_of(psshBoxes.value(), [](auto& psshBox) {
            return is<ISOFairPlayStreamingPsshBox>(*psshBox);
        });
    }
#endif

    if (initDataType == skdName())
        return true;

#if HAVE(FAIRPLAYSTREAMING_MTPS_INITDATA)
    if (initDataType == mptsName())
        return true;
#endif

    ASSERT_NOT_REACHED();
    return false;
}

RefPtr<SharedBuffer> CDMPrivateFairPlayStreaming::sanitizeResponse(const SharedBuffer& response) const
{
    return response.makeContiguous();
}

std::optional<String> CDMPrivateFairPlayStreaming::sanitizeSessionId(const String& sessionId) const
{
    return sessionId;
}

} // namespace WebCore

#endif // ENABLE(ENCRYPTED_MEDIA)
