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
#include "CDM.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMFactory.h"
#include "CDMPrivate.h"
#include "Document.h"
#include "InitDataRegistry.h"
#include "MediaKeysRequirement.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "Page.h"
#include "ParsedContentType.h"
#include "ScriptExecutionContext.h"
#include "SecurityOrigin.h"
#include "SecurityOriginData.h"
#include "Settings.h"
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>

namespace WebCore {

bool CDM::supportsKeySystem(const String& keySystem)
{
    for (auto* factory : CDMFactory::registeredFactories()) {
        if (factory->supportsKeySystem(keySystem))
            return true;
    }
    return false;
}

Ref<CDM> CDM::create(Document& document, const String& keySystem)
{
    return adoptRef(*new CDM(document, keySystem));
}

CDM::CDM(Document& document, const String& keySystem)
    : ContextDestructionObserver(&document)
    , m_keySystem(keySystem)
{
    ASSERT(supportsKeySystem(keySystem));
    for (auto* factory : CDMFactory::registeredFactories()) {
        if (factory->supportsKeySystem(keySystem)) {
            m_private = factory->createCDM(keySystem);
            break;
        }
    }
}

CDM::~CDM() = default;

void CDM::getSupportedConfiguration(MediaKeySystemConfiguration&& candidateConfiguration, SupportedConfigurationCallback&& callback)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration
    // W3C Editor's Draft 09 November 2016

    // 3.1.1.1 Get Supported Configuration
    // Given a Key Systems implementation implementation, MediaKeySystemConfiguration candidate configuration, and origin,
    // this algorithm returns a supported configuration or NotSupported as appropriate.

    // 1. Let supported configuration be ConsentDenied.
    // 2. Initialize restrictions to indicate that no configurations have had user consent denied.
    MediaKeysRestrictions restrictions { };
    doSupportedConfigurationStep(WTFMove(candidateConfiguration), WTFMove(restrictions), WTFMove(callback));
}

void CDM::doSupportedConfigurationStep(MediaKeySystemConfiguration&& candidateConfiguration, MediaKeysRestrictions&& restrictions, SupportedConfigurationCallback&& callback)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration
    // W3C Editor's Draft 09 November 2016, ctd.

    // 3.1.1.1 Get Supported Configuration
    // 3. Repeat the following step while supported configuration is ConsentDenied:
    // 3.1. Let supported configuration and, if provided, restrictions be the result of executing the
    // Get Supported Configuration and Consent algorithm with implementation, candidate configuration,
    // restrictions and origin.
    auto optionalConfiguration = getSupportedConfiguration(candidateConfiguration, restrictions);
    if (!optionalConfiguration) {
        callback(WTF::nullopt);
        return;
    }

    auto consentCallback = [weakThis = makeWeakPtr(*this), callback = WTFMove(callback)] (ConsentStatus status, MediaKeySystemConfiguration&& configuration, MediaKeysRestrictions&& restrictions) mutable {
        if (!weakThis) {
            callback(WTF::nullopt);
            return;
        }
        // 3.1.1.2 Get Supported Configuration and Consent, ctd.
        // 22. Let consent status and updated restrictions be the result of running the Get Consent Status algorithm on accumulated configuration,
        //     restrictions and origin and follow the steps for the value of consent status from the following list:
        switch (status) {
        case ConsentStatus::ConsentDenied:
            // ↳ ConsentDenied:
            //    Return ConsentDenied and updated restrictions.
            weakThis->doSupportedConfigurationStep(WTFMove(configuration), WTFMove(restrictions), WTFMove(callback));
            return;

        case ConsentStatus::InformUser:
            // ↳ InformUser
            //    Inform the user that accumulated configuration is in use in the origin including, specifically, the information that
            //    Distinctive Identifier(s) and/or Distinctive Permanent Identifier(s) as appropriate will be used if the
            //    distinctiveIdentifier member of accumulated configuration is "required". Continue to the next step.
            // NOTE: Implement.
            break;

        case ConsentStatus::Allowed:
            // ↳ Allowed:
            // Continue to the next step.
            break;
        }
        // 23. Return accumulated configuration.
        callback(WTFMove(configuration));
    };
    getConsentStatus(WTFMove(optionalConfiguration.value()), WTFMove(restrictions), WTFMove(consentCallback));
}

bool CDM::isPersistentType(MediaKeySessionType sessionType)
{
    // https://w3c.github.io/encrypted-media/#is-persistent-session-type
    // W3C Editor's Draft 09 November 2016

    // 5.1.1. Is persistent session type?
    // 1. Let the session type be the specified MediaKeySessionType value.
    // 2. Follow the steps for the value of session type from the following list:
    switch (sessionType) {
    case MediaKeySessionType::Temporary:
        // ↳ "temporary"
        return false;
    case MediaKeySessionType::PersistentLicense:
    case MediaKeySessionType::PersistentUsageRecord:
        // ↳ "persistent-license"
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

Optional<MediaKeySystemConfiguration> CDM::getSupportedConfiguration(const MediaKeySystemConfiguration& candidateConfiguration, MediaKeysRestrictions& restrictions)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration-and-consent
    // W3C Editor's Draft 09 November 2016

    ASSERT(m_private);
    if (!m_private)
        return WTF::nullopt;

    // 3.1.1.2 Get Supported Configuration and Consent
    // Given a Key Systems implementation implementation, MediaKeySystemConfiguration candidate configuration,
    // restrictions and origin, this algorithm returns a supported configuration, NotSupported, or ConsentDenied
    // as appropriate and, in the ConsentDenied case, restrictions.

    // 1. Let accumulated configuration be a new MediaKeySystemConfiguration dictionary.
    MediaKeySystemConfiguration accumulatedConfiguration { };

    // 2. Set the label member of accumulated configuration to equal the label member of candidate configuration.
    accumulatedConfiguration.label = candidateConfiguration.label;

    // 3. If the initDataTypes member of candidate configuration is non-empty, run the following steps:
    if (!candidateConfiguration.initDataTypes.isEmpty()) {
        // 3.1. Let supported types be an empty sequence of DOMStrings.
        Vector<String> supportedTypes;

        // 3.2. For each value in candidate configuration's initDataTypes member:
        for (auto initDataType : candidateConfiguration.initDataTypes) {
            // 3.2.1. Let initDataType be the value.
            // 3.2.2. If the implementation supports generating requests based on initDataType, add initDataType
            //        to supported types. String comparison is case-sensitive. The empty string is never supported.
            if (initDataType.isEmpty())
                continue;

            if (m_private && m_private->supportsInitDataType(initDataType))
                supportedTypes.append(initDataType);
        }

        // 3.3. If supported types is empty, return NotSupported.
        if (supportedTypes.isEmpty())
            return WTF::nullopt;

        // 3.4. Set the initDataTypes member of accumulated configuration to supported types.
        accumulatedConfiguration.initDataTypes = WTFMove(supportedTypes);
    }

    // 4. Let distinctive identifier requirement be the value of candidate configuration's distinctiveIdentifier member.
    MediaKeysRequirement distinctiveIdentifierRequirement = candidateConfiguration.distinctiveIdentifier;

    // 5. If distinctive identifier requirement is "optional" and Distinctive Identifiers are not allowed according to
    //    restrictions, set distinctive identifier requirement to "not-allowed".
    if (distinctiveIdentifierRequirement == MediaKeysRequirement::Optional && restrictions.distinctiveIdentifierDenied)
        distinctiveIdentifierRequirement = MediaKeysRequirement::NotAllowed;

    // 6. Follow the steps for distinctive identifier requirement from the following list:
    switch (distinctiveIdentifierRequirement) {
    case MediaKeysRequirement::Required:
        // ↳ "required"
        // If the implementation does not support use of Distinctive Identifier(s) in combination
        // with accumulated configuration and restrictions, return NotSupported.
        if (m_private->distinctiveIdentifiersRequirement(accumulatedConfiguration, restrictions) == MediaKeysRequirement::NotAllowed)
            return WTF::nullopt;
        break;

    case MediaKeysRequirement::Optional:
        // ↳ "optional"
        // Continue with the following steps.
        break;

    case MediaKeysRequirement::NotAllowed:
        // ↳ "not-allowed"
        // If the implementation requires use Distinctive Identifier(s) or Distinctive Permanent Identifier(s)
        // in combination with accumulated configuration and restrictions, return NotSupported.
        if (m_private->distinctiveIdentifiersRequirement(accumulatedConfiguration, restrictions) == MediaKeysRequirement::Required)
            return WTF::nullopt;
        break;
    }

    // 7. Set the distinctiveIdentifier member of accumulated configuration to equal distinctive identifier requirement.
    accumulatedConfiguration.distinctiveIdentifier = distinctiveIdentifierRequirement;

    // 8. Let persistent state requirement be equal to the value of candidate configuration's persistentState member.
    MediaKeysRequirement persistentStateRequirement = candidateConfiguration.persistentState;

    // 9. If persistent state requirement is "optional" and persisting state is not allowed according to restrictions,
    //    set persistent state requirement to "not-allowed".
    if (persistentStateRequirement == MediaKeysRequirement::Optional && restrictions.persistentStateDenied)
        persistentStateRequirement =  MediaKeysRequirement::NotAllowed;

    // 10. Follow the steps for persistent state requirement from the following list:
    switch (persistentStateRequirement) {
    case MediaKeysRequirement::Required:
        // ↳ "required"
        // If the implementation does not support persisting state in combination with accumulated configuration
        // and restrictions, return NotSupported.
        if (m_private->persistentStateRequirement(accumulatedConfiguration, restrictions) == MediaKeysRequirement::NotAllowed)
            return WTF::nullopt;
        break;

    case MediaKeysRequirement::Optional:
        // ↳ "optional"
        // Continue with the following steps.
        break;

    case MediaKeysRequirement::NotAllowed:
        // ↳ "not-allowed"
        // If the implementation requires persisting state in combination with accumulated configuration
        // and restrictions, return NotSupported
        if (m_private->persistentStateRequirement(accumulatedConfiguration, restrictions) == MediaKeysRequirement::Required)
            return WTF::nullopt;
        break;
    }

    // 11. Set the persistentState member of accumulated configuration to equal the value of persistent state requirement.
    accumulatedConfiguration.persistentState = persistentStateRequirement;

    // 12. Follow the steps for the first matching condition from the following list:
    Vector<MediaKeySessionType> sessionTypes;

    if (!candidateConfiguration.sessionTypes.isEmpty()) {
        // ↳ If the sessionTypes member is present [WebIDL] in candidate configuration
        // Let session types be candidate configuration's sessionTypes member.
        sessionTypes = candidateConfiguration.sessionTypes;
    } else {
        // ↳ Otherwise
        // Let session types be [ "temporary" ].
        sessionTypes = { MediaKeySessionType::Temporary };
    }

    // 13. For each value in session types:
    for (auto& sessionType : sessionTypes) {
        // 13.1. Let session type be the value.
        // 13.2. If accumulated configuration's persistentState value is "not-allowed" and the
        //       Is persistent session type? algorithm returns true for session type return NotSupported.
        if (accumulatedConfiguration.persistentState == MediaKeysRequirement::NotAllowed && isPersistentType(sessionType))
            return WTF::nullopt;

        // 13.3. If the implementation does not support session type in combination with accumulated configuration
        //       and restrictions for other reasons, return NotSupported.
        if (!m_private->supportsSessionTypeWithConfiguration(sessionType, accumulatedConfiguration))
            return WTF::nullopt;

        // 13.4 If accumulated configuration's persistentState value is "optional" and the result of running the Is
        //      persistent session type? algorithm on session type is true, change accumulated configuration's persistentState
        //      value to "required".
        if (accumulatedConfiguration.persistentState == MediaKeysRequirement::Optional && isPersistentType(sessionType))
            accumulatedConfiguration.persistentState = MediaKeysRequirement::Required;
    }

    // 14. Set the sessionTypes member of accumulated configuration to session types.
    accumulatedConfiguration.sessionTypes = sessionTypes;

    // 15. If the videoCapabilities and audioCapabilities members in candidate configuration are both empty, return NotSupported.
    if (candidateConfiguration.videoCapabilities.isEmpty() && candidateConfiguration.audioCapabilities.isEmpty())
        return WTF::nullopt;

    // 16. ↳ If the videoCapabilities member in candidate configuration is non-empty:
    if (!candidateConfiguration.videoCapabilities.isEmpty()) {
        // 16.1. Let video capabilities be the result of executing the Get Supported Capabilities for Audio/Video Type algorithm on
        //       Video, candidate configuration's videoCapabilities member, accumulated configuration, and restrictions.
        auto videoCapabilities = getSupportedCapabilitiesForAudioVideoType(AudioVideoType::Video, candidateConfiguration.videoCapabilities, accumulatedConfiguration, restrictions);

        // 16.2. If video capabilities is null, return NotSupported.
        if (!videoCapabilities)
            return WTF::nullopt;

        // 16.3 Set the videoCapabilities member of accumulated configuration to video capabilities.
        accumulatedConfiguration.videoCapabilities = WTFMove(videoCapabilities.value());
    } else {
        // 16. ↳ Otherwise:
        //     Set the videoCapabilities member of accumulated configuration to an empty sequence.
        accumulatedConfiguration.videoCapabilities = { };
    }

    // 17. ↳ If the audioCapabilities member in candidate configuration is non-empty:
    if (!candidateConfiguration.audioCapabilities.isEmpty()) {
        // 17.1. Let audio capabilities be the result of executing the Get Supported Capabilities for Audio/Video Type algorithm on
        //       Audio, candidate configuration's audioCapabilities member, accumulated configuration, and restrictions.
        auto audioCapabilities = getSupportedCapabilitiesForAudioVideoType(AudioVideoType::Audio, candidateConfiguration.audioCapabilities, accumulatedConfiguration, restrictions);

        // 17.2. If audio capabilities is null, return NotSupported.
        if (!audioCapabilities)
            return WTF::nullopt;

        // 17.3 Set the audioCapabilities member of accumulated configuration to audio capabilities.
        accumulatedConfiguration.audioCapabilities = WTFMove(audioCapabilities.value());
    } else {
        // 17. ↳ Otherwise:
        //     Set the audioCapabilities member of accumulated configuration to an empty sequence.
        accumulatedConfiguration.audioCapabilities = { };
    }

    // 18. If accumulated configuration's distinctiveIdentifier value is "optional", follow the steps for the first matching
    //     condition from the following list:
    if (accumulatedConfiguration.distinctiveIdentifier == MediaKeysRequirement::Optional) {
        // ↳ If the implementation requires use Distinctive Identifier(s) or Distinctive Permanent Identifier(s) for any of the
        //    combinations in accumulated configuration
        if (m_private->distinctiveIdentifiersRequirement(accumulatedConfiguration, restrictions) == MediaKeysRequirement::Required) {
            // Change accumulated configuration's distinctiveIdentifier value to "required".
            accumulatedConfiguration.distinctiveIdentifier = MediaKeysRequirement::Required;
        } else {
            // ↳ Otherwise
            //    Change accumulated configuration's distinctiveIdentifier value to "not-allowed".
            accumulatedConfiguration.distinctiveIdentifier = MediaKeysRequirement::NotAllowed;
        }
    }

    // 19. If accumulated configuration's persistentState value is "optional", follow the steps for the first matching
    //     condition from the following list:
    if (accumulatedConfiguration.persistentState == MediaKeysRequirement::Optional) {
        // ↳ If the implementation requires persisting state for any of the combinations in accumulated configuration
        if (m_private->persistentStateRequirement(accumulatedConfiguration, restrictions) == MediaKeysRequirement::Required) {
            // Change accumulated configuration's persistentState value to "required".
            accumulatedConfiguration.persistentState = MediaKeysRequirement::Required;
        } else {
            // ↳ Otherwise
            //    Change accumulated configuration's persistentState value to "not-allowed".
            accumulatedConfiguration.persistentState = MediaKeysRequirement::NotAllowed;
        }
    }

    // 20. If implementation in the configuration specified by the combination of the values in accumulated configuration
    //     is not supported or not allowed in the origin, return NotSupported.
    if (!m_private->supportsConfiguration(accumulatedConfiguration))
        return WTF::nullopt;

    Document* document = downcast<Document>(m_scriptExecutionContext);
    if (!document)
        return WTF::nullopt;

    SecurityOrigin& origin = document->securityOrigin();
    SecurityOrigin& topOrigin = document->topOrigin();

    if ((accumulatedConfiguration.distinctiveIdentifier == MediaKeysRequirement::Required || accumulatedConfiguration.persistentState == MediaKeysRequirement::Required) && !origin.canAccessLocalStorage(&topOrigin))
        return WTF::nullopt;

    return WTFMove(accumulatedConfiguration);
    // NOTE: Continued in getConsentStatus().
}

Optional<Vector<MediaKeySystemMediaCapability>> CDM::getSupportedCapabilitiesForAudioVideoType(CDM::AudioVideoType type, const Vector<MediaKeySystemMediaCapability>& requestedCapabilities, const MediaKeySystemConfiguration& partialConfiguration, MediaKeysRestrictions& restrictions)
{
    // https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-audio-video-type
    // W3C Editor's Draft 09 November 2016

    ASSERT(m_private);
    if (!m_private)
        return WTF::nullopt;

    // 3.1.1.3 Get Supported Capabilities for Audio/Video Type

    // Given an audio/video type, MediaKeySystemMediaCapability sequence requested media capabilities, MediaKeySystemConfiguration
    // partial configuration, and restrictions, this algorithm returns a sequence of supported MediaKeySystemMediaCapability values
    // for this audio/video type or null as appropriate.

    // 1. Let local accumulated configuration be a local copy of partial configuration.
    MediaKeySystemConfiguration accumulatedConfiguration = partialConfiguration;

    // 2. Let supported media capabilities be an empty sequence of MediaKeySystemMediaCapability dictionaries.
    Vector<MediaKeySystemMediaCapability> supportedMediaCapabilities { };

    // 3. For each requested media capability in requested media capabilities:
    for (auto& requestedCapability : requestedCapabilities) {
        // 3.1. Let content type be requested media capability's contentType member.
        // 3.2. Let robustness be requested media capability's robustness member.
        String robustness = requestedCapability.robustness;

        // 3.3. If content type is the empty string, return null.
        if (requestedCapability.contentType.isEmpty())
            return WTF::nullopt;

        // 3.4. If content type is an invalid or unrecognized MIME type, continue to the next iteration.
        if (!isValidContentType(requestedCapability.contentType, Mode::Rfc2045))
            continue;

        // 3.5. Let container be the container type specified by content type.
        ParsedContentType contentType { requestedCapability.contentType };
        String container = contentType.mimeType();

        // 3.6. If the user agent does not support container, continue to the next iteration. The case-sensitivity
        //      of string comparisons is determined by the appropriate RFC.
        // 3.7. Let parameters be the RFC 6381 [RFC6381] parameters, if any, specified by content type.
        // 3.8. If the user agent does not recognize one or more parameters, continue to the next iteration.
        // 3.9. Let media types be the set of codecs and codec constraints specified by parameters. The case-sensitivity
        //      of string comparisons is determined by the appropriate RFC or other specification.
        String codecs = contentType.parameterValueForName("codecs");
        if (contentType.parameterCount() > (codecs.isEmpty() ? 0 : 1))
            continue;

        // 3.10. If media types is empty:
        if (codecs.isEmpty()) {
            // ↳ If container normatively implies a specific set of codecs and codec constraints:
            // ↳ Otherwise:
            notImplemented();
        }

        // 3.11. If content type is not strictly a audio/video type, continue to the next iteration.
        // 3.12. If robustness is not the empty string and contains an unrecognized value or a value not supported by
        //       implementation, continue to the next iteration. String comparison is case-sensitive.
        if (!robustness.isEmpty() && !m_private->supportsRobustness(robustness))
            continue;

        // 3.13. If the user agent and implementation definitely support playback of encrypted media data for the
        //       combination of container, media types, robustness and local accumulated configuration in combination
        //       with restrictions:
        MediaEngineSupportParameters parameters;
        parameters.type = ContentType(contentType.mimeType());
        if (!MediaPlayer::supportsType(parameters)) {
            // Try with Media Source:
            parameters.isMediaSource = true;
            if (!MediaPlayer::supportsType(parameters))
                continue;
        }

        if (!m_private->supportsConfigurationWithRestrictions(accumulatedConfiguration, restrictions))
            continue;

        // 3.13.1. Add requested media capability to supported media capabilities.
        supportedMediaCapabilities.append(requestedCapability);

        // 3.13.2. ↳ If audio/video type is Video:
        //         Add requested media capability to the videoCapabilities member of local accumulated configuration.
        if (type == AudioVideoType::Video)
            accumulatedConfiguration.videoCapabilities.append(requestedCapability);
        // 3.13.2. ↳ If audio/video type is Audio:
        //         Add requested media capability to the audioCapabilities member of local accumulated configuration.
        else
            accumulatedConfiguration.audioCapabilities.append(requestedCapability);
    }

    // 4. If supported media capabilities is empty, return null.
    if (supportedMediaCapabilities.isEmpty())
        return WTF::nullopt;

    // 5. Return supported media capabilities.
    return supportedMediaCapabilities;
}

void CDM::getConsentStatus(MediaKeySystemConfiguration&& accumulatedConfiguration, MediaKeysRestrictions&& restrictions, ConsentStatusCallback&& callback)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration-and-consent
    // W3C Editor's Draft 09 November 2016
    if (!m_scriptExecutionContext) {
        callback(ConsentStatus::ConsentDenied, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
        return;
    }

    // NOTE: In the future, these checks belowe will involve asking the page client, possibly across a process boundary.
    // They will by necessity be asynchronous with callbacks. For now, imply this behavior by performing it in an async task.

    m_scriptExecutionContext->postTask([this, weakThis = makeWeakPtr(*this), accumulatedConfiguration = WTFMove(accumulatedConfiguration), restrictions = WTFMove(restrictions), callback = WTFMove(callback)] (ScriptExecutionContext&) mutable {
        if (!weakThis || !m_private) {
            callback(ConsentStatus::ConsentDenied, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
            return;
        }

        Document* document = downcast<Document>(m_scriptExecutionContext);
        if (!document) {
            callback(ConsentStatus::ConsentDenied, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
            return;
        }

        SecurityOrigin& origin = document->securityOrigin();
        SecurityOrigin& topOrigin = document->topOrigin();

        // 3.1.1.2 Get Supported Configuration and Consent, ctd.
        // 21. If accumulated configuration's distinctiveIdentifier value is "required" and the Distinctive Identifier(s) associated
        //     with accumulated configuration are not unique per origin and profile and clearable:
        if (accumulatedConfiguration.distinctiveIdentifier == MediaKeysRequirement::Required && !m_private->distinctiveIdentifiersAreUniquePerOriginAndClearable(accumulatedConfiguration)) {
            // 21.1. Update restrictions to reflect that all configurations described by accumulated configuration do not have user consent.
            restrictions.distinctiveIdentifierDenied = true;
            callback(ConsentStatus::ConsentDenied, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
            return;
        }

        // https://w3c.github.io/encrypted-media/#get-consent-status
        // 3.1.1.4 Get Consent Status
        // Given an accumulated configuration, restrictions and origin, this algorithm returns the consent status for accumulated
        // configuration and origin as one of ConsentDenied, InformUser or Allowed, together with an updated value for restrictions
        // in the ConsentDenied case.

        // 1. If there is persisted denial for origin indicating that accumulated configuration is not allowed, run the following steps:
        // 1.1. Update restrictions to reflect the configurations for which consent has been denied.
        // 1.2. Return ConsentDenied and restrictions.
        // 2. If there is persisted consent for origin indicating accumulated configuration is allowed, return Allowed.
        // NOTE: persisted denial / consent unimplemented.

        // 3. If any of the following are true:
        //    ↳ The distinctiveIdentifier member of accumulated configuration is not "not-allowed" and the combination of the User Agent,
        //       implementation and accumulated configuration does not follow all the recommendations of Allow Persistent Data to Be Cleared
        //       with respect to Distinctive Identifier(s).
        // NOTE: assume that implementations follow all recommendations.

        //    ↳ The user agent requires explicit user consent for the accumulated configuration for other reasons.
        // NOTE: assume the user agent does not require explicit user consent.

        // 3.1. Request user consent to use accumulated configuration in the origin and wait for the user response.
        //      The consent must include consent to use a Distinctive Identifier(s) and/or Distinctive Permanent Identifier(s) as appropriate
        //      if accumulated configuration's distinctiveIdentifier member is "required".
        // 3.2. If consent was denied, run the following steps:
        // 3.2.1. Update restrictions to reflect the configurations for which consent was denied.
        // 3.2.1. Return ConsentDenied and restrictions.
        // NOTE: assume implied consent if the combination of origin and topOrigin allows it.
        if (accumulatedConfiguration.distinctiveIdentifier == MediaKeysRequirement::Required && !origin.canAccessLocalStorage(&topOrigin)) {
            restrictions.distinctiveIdentifierDenied = true;
            callback(ConsentStatus::ConsentDenied, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
            return;
        }

        // 4. If the distinctiveIdentifier member of accumulated configuration is not "not-allowed", return InformUser.
        if (accumulatedConfiguration.distinctiveIdentifier != MediaKeysRequirement::NotAllowed) {
            callback(ConsentStatus::InformUser, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
            return;
        }

        // 5. If the user agent requires informing the user for the accumulated configuration for other reasons, return InformUser.
        // NOTE: assume the user agent does not require informing the user.

        // 6. Return Allowed.
        callback(ConsentStatus::Allowed, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
    });
}

void CDM::loadAndInitialize()
{
    if (m_private)
        m_private->loadAndInitialize();
}

RefPtr<CDMInstance> CDM::createInstance()
{
    if (!m_private)
        return nullptr;
    auto instance = m_private->createInstance();
    instance->setStorageDirectory(storageDirectory());
    return instance;
}

bool CDM::supportsServerCertificates() const
{
    return m_private && m_private->supportsServerCertificates();
}

bool CDM::supportsSessions() const
{
    return m_private && m_private->supportsSessions();
}

bool CDM::supportsInitDataType(const AtomicString& initDataType) const
{
    return m_private && m_private->supportsInitDataType(initDataType);
}

RefPtr<SharedBuffer> CDM::sanitizeInitData(const AtomicString& initDataType, const SharedBuffer& initData)
{
    return InitDataRegistry::shared().sanitizeInitData(initDataType, initData);
}

bool CDM::supportsInitData(const AtomicString& initDataType, const SharedBuffer& initData)
{
    return m_private && m_private->supportsInitData(initDataType, initData);
}

RefPtr<SharedBuffer> CDM::sanitizeResponse(const SharedBuffer& response)
{
    if (!m_private)
        return nullptr;
    return m_private->sanitizeResponse(response);
}

Optional<String> CDM::sanitizeSessionId(const String& sessionId)
{
    if (!m_private)
        return WTF::nullopt;
    return m_private->sanitizeSessionId(sessionId);
}

String CDM::storageDirectory() const
{
    auto* document = downcast<Document>(scriptExecutionContext());
    if (!document)
        return emptyString();

    auto* page = document->page();
    if (!page || page->usesEphemeralSession())
        return emptyString();

    auto storageDirectory = document->settings().mediaKeysStorageDirectory();
    if (storageDirectory.isEmpty())
        return emptyString();

    return FileSystem::pathByAppendingComponent(storageDirectory, document->securityOrigin().data().databaseIdentifier());
}

}

#endif
