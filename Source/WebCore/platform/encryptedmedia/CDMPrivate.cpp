/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#include "CDMPrivate.h"

#if ENABLE(ENCRYPTED_MEDIA)

#include "CDMKeySystemConfiguration.h"
#include "CDMMediaCapability.h"
#include "CDMRequirement.h"
#include "CDMRestrictions.h"
#include "MediaPlayer.h"
#include "NotImplemented.h"
#include "ParsedContentType.h"
#include <wtf/WeakPtr.h>

namespace WebCore {

CDMPrivate::CDMPrivate() = default;
CDMPrivate::~CDMPrivate() = default;

void CDMPrivate::getSupportedConfiguration(CDMKeySystemConfiguration&& candidateConfiguration, LocalStorageAccess access, SupportedConfigurationCallback&& callback)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration
    // W3C Editor's Draft 09 November 2016

    // 3.1.1.1 Get Supported Configuration
    // Given a Key Systems implementation implementation, CDMKeySystemConfiguration candidate configuration, and origin,
    // this algorithm returns a supported configuration or NotSupported as appropriate.

    // 1. Let supported configuration be ConsentDenied.
    // 2. Initialize restrictions to indicate that no configurations have had user consent denied.
    CDMRestrictions restrictions { };
    doSupportedConfigurationStep(WTFMove(candidateConfiguration), WTFMove(restrictions), access, WTFMove(callback));
}

void CDMPrivate::doSupportedConfigurationStep(CDMKeySystemConfiguration&& candidateConfiguration, CDMRestrictions&& restrictions, LocalStorageAccess access, SupportedConfigurationCallback&& callback)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration
    // W3C Editor's Draft 09 November 2016, ctd.

    // 3.1.1.1 Get Supported Configuration
    // 3. Repeat the following step while supported configuration is ConsentDenied:
    // 3.1. Let supported configuration and, if provided, restrictions be the result of executing the
    // Get Supported Configuration and Consent algorithm with implementation, candidate configuration,
    // restrictions and origin.
    auto optionalConfiguration = getSupportedConfiguration(candidateConfiguration, restrictions, access);
    if (!optionalConfiguration) {
        callback(WTF::nullopt);
        return;
    }

    auto consentCallback = [weakThis = makeWeakPtr(*this), callback = WTFMove(callback), access] (ConsentStatus status, CDMKeySystemConfiguration&& configuration, CDMRestrictions&& restrictions) mutable {
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
            weakThis->doSupportedConfigurationStep(WTFMove(configuration), WTFMove(restrictions), access, WTFMove(callback));
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
    getConsentStatus(WTFMove(optionalConfiguration.value()), WTFMove(restrictions), access, WTFMove(consentCallback));
}

bool CDMPrivate::isPersistentType(CDMSessionType sessionType)
{
    // https://w3c.github.io/encrypted-media/#is-persistent-session-type
    // W3C Editor's Draft 09 November 2016

    // 5.1.1. Is persistent session type?
    // 1. Let the session type be the specified CDMSessionType value.
    // 2. Follow the steps for the value of session type from the following list:
    switch (sessionType) {
    case CDMSessionType::Temporary:
        // ↳ "temporary"
        return false;
    case CDMSessionType::PersistentLicense:
    case CDMSessionType::PersistentUsageRecord:
        // ↳ "persistent-license"
        return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

Optional<CDMKeySystemConfiguration> CDMPrivate::getSupportedConfiguration(const CDMKeySystemConfiguration& candidateConfiguration, CDMRestrictions& restrictions, LocalStorageAccess access)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration-and-consent
    // W3C Editor's Draft 09 November 2016

    // 3.1.1.2 Get Supported Configuration and Consent
    // Given a Key Systems implementation implementation, CDMKeySystemConfiguration candidate configuration,
    // restrictions and origin, this algorithm returns a supported configuration, NotSupported, or ConsentDenied
    // as appropriate and, in the ConsentDenied case, restrictions.

    // 1. Let accumulated configuration be a new CDMKeySystemConfiguration dictionary.
    CDMKeySystemConfiguration accumulatedConfiguration { };

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

            if (supportedInitDataTypes().contains(initDataType))
                supportedTypes.append(initDataType);
        }

        // 3.3. If supported types is empty, return NotSupported.
        if (supportedTypes.isEmpty())
            return WTF::nullopt;

        // 3.4. Set the initDataTypes member of accumulated configuration to supported types.
        accumulatedConfiguration.initDataTypes = WTFMove(supportedTypes);
    }

    // 4. Let distinctive identifier requirement be the value of candidate configuration's distinctiveIdentifier member.
    CDMRequirement distinctiveIdentifierRequirement = candidateConfiguration.distinctiveIdentifier;

    // 5. If distinctive identifier requirement is "optional" and Distinctive Identifiers are not allowed according to
    //    restrictions, set distinctive identifier requirement to "not-allowed".
    if (distinctiveIdentifierRequirement == CDMRequirement::Optional && restrictions.distinctiveIdentifierDenied)
        distinctiveIdentifierRequirement = CDMRequirement::NotAllowed;

    // 6. Follow the steps for distinctive identifier requirement from the following list:
    switch (distinctiveIdentifierRequirement) {
    case CDMRequirement::Required:
        // ↳ "required"
        // If the implementation does not support use of Distinctive Identifier(s) in combination
        // with accumulated configuration and restrictions, return NotSupported.
        if (distinctiveIdentifiersRequirement(accumulatedConfiguration, restrictions) == CDMRequirement::NotAllowed)
            return WTF::nullopt;
        break;

    case CDMRequirement::Optional:
        // ↳ "optional"
        // Continue with the following steps.
        break;

    case CDMRequirement::NotAllowed:
        // ↳ "not-allowed"
        // If the implementation requires use Distinctive Identifier(s) or Distinctive Permanent Identifier(s)
        // in combination with accumulated configuration and restrictions, return NotSupported.
        if (distinctiveIdentifiersRequirement(accumulatedConfiguration, restrictions) == CDMRequirement::Required)
            return WTF::nullopt;
        break;
    }

    // 7. Set the distinctiveIdentifier member of accumulated configuration to equal distinctive identifier requirement.
    accumulatedConfiguration.distinctiveIdentifier = distinctiveIdentifierRequirement;

    // 8. Let persistent state requirement be equal to the value of candidate configuration's persistentState member.
    CDMRequirement persistentStateRequirement = candidateConfiguration.persistentState;

    // 9. If persistent state requirement is "optional" and persisting state is not allowed according to restrictions,
    //    set persistent state requirement to "not-allowed".
    if (persistentStateRequirement == CDMRequirement::Optional && restrictions.persistentStateDenied)
        persistentStateRequirement = CDMRequirement::NotAllowed;

    // 10. Follow the steps for persistent state requirement from the following list:
    switch (persistentStateRequirement) {
    case CDMRequirement::Required:
        // ↳ "required"
        // If the implementation does not support persisting state in combination with accumulated configuration
        // and restrictions, return NotSupported.
        if (this->persistentStateRequirement(accumulatedConfiguration, restrictions) == CDMRequirement::NotAllowed)
            return WTF::nullopt;
        break;

    case CDMRequirement::Optional:
        // ↳ "optional"
        // Continue with the following steps.
        break;

    case CDMRequirement::NotAllowed:
        // ↳ "not-allowed"
        // If the implementation requires persisting state in combination with accumulated configuration
        // and restrictions, return NotSupported
        if (this->persistentStateRequirement(accumulatedConfiguration, restrictions) == CDMRequirement::Required)
            return WTF::nullopt;
        break;
    }

    // 11. Set the persistentState member of accumulated configuration to equal the value of persistent state requirement.
    accumulatedConfiguration.persistentState = persistentStateRequirement;

    // 12. Follow the steps for the first matching condition from the following list:
    Vector<CDMSessionType> sessionTypes;

    if (!candidateConfiguration.sessionTypes.isEmpty()) {
        // ↳ If the sessionTypes member is present [WebIDL] in candidate configuration
        // Let session types be candidate configuration's sessionTypes member.
        sessionTypes = candidateConfiguration.sessionTypes;
    } else {
        // ↳ Otherwise
        // Let session types be [ "temporary" ].
        sessionTypes = { CDMSessionType::Temporary };
    }

    // 13. For each value in session types:
    for (auto& sessionType : sessionTypes) {
        // 13.1. Let session type be the value.
        // 13.2. If accumulated configuration's persistentState value is "not-allowed" and the
        //       Is persistent session type? algorithm returns true for session type return NotSupported.
        if (accumulatedConfiguration.persistentState == CDMRequirement::NotAllowed && isPersistentType(sessionType))
            return WTF::nullopt;

        // 13.3. If the implementation does not support session type in combination with accumulated configuration
        //       and restrictions for other reasons, return NotSupported.
        if (!supportsSessionTypeWithConfiguration(sessionType, accumulatedConfiguration))
            return WTF::nullopt;

        // 13.4 If accumulated configuration's persistentState value is "optional" and the result of running the Is
        //      persistent session type? algorithm on session type is true, change accumulated configuration's persistentState
        //      value to "required".
        if (accumulatedConfiguration.persistentState == CDMRequirement::Optional && isPersistentType(sessionType))
            accumulatedConfiguration.persistentState = CDMRequirement::Required;
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
    if (accumulatedConfiguration.distinctiveIdentifier == CDMRequirement::Optional) {
        // ↳ If the implementation requires use Distinctive Identifier(s) or Distinctive Permanent Identifier(s) for any of the
        //    combinations in accumulated configuration
        if (distinctiveIdentifiersRequirement(accumulatedConfiguration, restrictions) == CDMRequirement::Required) {
            // Change accumulated configuration's distinctiveIdentifier value to "required".
            accumulatedConfiguration.distinctiveIdentifier = CDMRequirement::Required;
        } else {
            // ↳ Otherwise
            //    Change accumulated configuration's distinctiveIdentifier value to "not-allowed".
            accumulatedConfiguration.distinctiveIdentifier = CDMRequirement::NotAllowed;
        }
    }

    // 19. If accumulated configuration's persistentState value is "optional", follow the steps for the first matching
    //     condition from the following list:
    if (accumulatedConfiguration.persistentState == CDMRequirement::Optional) {
        // ↳ If the implementation requires persisting state for any of the combinations in accumulated configuration
        if (this->persistentStateRequirement(accumulatedConfiguration, restrictions) == CDMRequirement::Required) {
            // Change accumulated configuration's persistentState value to "required".
            accumulatedConfiguration.persistentState = CDMRequirement::Required;
        } else {
            // ↳ Otherwise
            //    Change accumulated configuration's persistentState value to "not-allowed".
            accumulatedConfiguration.persistentState = CDMRequirement::NotAllowed;
        }
    }

    // 20. If implementation in the configuration specified by the combination of the values in accumulated configuration
    //     is not supported or not allowed in the origin, return NotSupported.
    if (!supportsConfiguration(accumulatedConfiguration))
        return WTF::nullopt;

    if ((accumulatedConfiguration.distinctiveIdentifier == CDMRequirement::Required || accumulatedConfiguration.persistentState == CDMRequirement::Required) && access == LocalStorageAccess::NotAllowed)
        return WTF::nullopt;

    return accumulatedConfiguration;
    // NOTE: Continued in getConsentStatus().
}

Optional<Vector<CDMMediaCapability>> CDMPrivate::getSupportedCapabilitiesForAudioVideoType(CDMPrivate::AudioVideoType type, const Vector<CDMMediaCapability>& requestedCapabilities, const CDMKeySystemConfiguration& partialConfiguration, CDMRestrictions& restrictions)
{
    // https://w3c.github.io/encrypted-media/#get-supported-capabilities-for-audio-video-type
    // W3C Editor's Draft 09 November 2016

    // 3.1.1.3 Get Supported Capabilities for Audio/Video Type

    // Given an audio/video type, CDMMediaCapability sequence requested media capabilities, CDMKeySystemConfiguration
    // partial configuration, and restrictions, this algorithm returns a sequence of supported CDMMediaCapability values
    // for this audio/video type or null as appropriate.

    // 1. Let local accumulated configuration be a local copy of partial configuration.
    CDMKeySystemConfiguration accumulatedConfiguration = partialConfiguration;

    // 2. Let supported media capabilities be an empty sequence of CDMMediaCapability dictionaries.
    Vector<CDMMediaCapability> supportedMediaCapabilities { };

    // 3. For each requested media capability in requested media capabilities:
    for (auto& requestedCapability : requestedCapabilities) {
        // 3.1. Let content type be requested media capability's contentType member.
        // 3.2. Let robustness be requested media capability's robustness member.
        String robustness = requestedCapability.robustness;

        // 3.3. If content type is the empty string, return null.
        if (requestedCapability.contentType.isEmpty())
            return WTF::nullopt;

        // 3.4. If content type is an invalid or unrecognized MIME type, continue to the next iteration.
        Optional<ParsedContentType> contentType = ParsedContentType::create(requestedCapability.contentType, Mode::Rfc2045);
        if (!contentType)
            continue;

        // 3.5. Let container be the container type specified by content type.
        String container = contentType->mimeType();

        // 3.6. If the user agent does not support container, continue to the next iteration. The case-sensitivity
        //      of string comparisons is determined by the appropriate RFC.
        // 3.7. Let parameters be the RFC 6381 [RFC6381] parameters, if any, specified by content type.
        // 3.8. If the user agent does not recognize one or more parameters, continue to the next iteration.
        // 3.9. Let media types be the set of codecs and codec constraints specified by parameters. The case-sensitivity
        //      of string comparisons is determined by the appropriate RFC or other specification.
        String codecs = contentType->parameterValueForName("codecs");
        if (contentType->parameterCount() > (codecs.isEmpty() ? 0 : 1))
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
        if (!robustness.isEmpty() && !supportedRobustnesses().contains(robustness))
            continue;

        // 3.13. If the user agent and implementation definitely support playback of encrypted media data for the
        //       combination of container, media types, robustness and local accumulated configuration in combination
        //       with restrictions:
        MediaEngineSupportParameters parameters;
        parameters.type = ContentType(contentType->mimeType());
        if (MediaPlayer::supportsType(parameters) == MediaPlayer::SupportsType::IsNotSupported) {

            // Try with Media Source:
            parameters.isMediaSource = true;
            if (MediaPlayer::supportsType(parameters) == MediaPlayer::SupportsType::IsNotSupported)
                continue;
        }

        if (!supportsConfigurationWithRestrictions(accumulatedConfiguration, restrictions))
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

void CDMPrivate::getConsentStatus(CDMKeySystemConfiguration&& accumulatedConfiguration, CDMRestrictions&& restrictions, LocalStorageAccess access, ConsentStatusCallback&& callback)
{
    // https://w3c.github.io/encrypted-media/#get-supported-configuration-and-consent
    // W3C Editor's Draft 09 November 2016

    // 3.1.1.2 Get Supported Configuration and Consent, ctd.
    // 21. If accumulated configuration's distinctiveIdentifier value is "required" and the Distinctive Identifier(s) associated
    //     with accumulated configuration are not unique per origin and profile and clearable:
    if (accumulatedConfiguration.distinctiveIdentifier == CDMRequirement::Required && !distinctiveIdentifiersAreUniquePerOriginAndClearable(accumulatedConfiguration)) {
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
    if (accumulatedConfiguration.distinctiveIdentifier == CDMRequirement::Required && access == LocalStorageAccess::NotAllowed) {
        restrictions.distinctiveIdentifierDenied = true;
        callback(ConsentStatus::ConsentDenied, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
        return;
    }

    // 4. If the distinctiveIdentifier member of accumulated configuration is not "not-allowed", return InformUser.
    if (accumulatedConfiguration.distinctiveIdentifier != CDMRequirement::NotAllowed) {
        callback(ConsentStatus::InformUser, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
        return;
    }

    // 5. If the user agent requires informing the user for the accumulated configuration for other reasons, return InformUser.
    // NOTE: assume the user agent does not require informing the user.

    // 6. Return Allowed.
    callback(ConsentStatus::Allowed, WTFMove(accumulatedConfiguration), WTFMove(restrictions));
}


}

#endif
