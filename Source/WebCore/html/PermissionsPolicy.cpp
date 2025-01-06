/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PermissionsPolicy.h"

#include "Allowlist.h"
#include "Document.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "LocalDOMWindow.h"
#include "Quirks.h"
#include "SecurityOrigin.h"
#include <wtf/TZoneMalloc.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PermissionsPolicy);

using namespace HTMLNames;

static ASCIILiteral toFeatureNameForLogging(PermissionsPolicy::Feature feature)
{
    switch (feature) {
    case PermissionsPolicy::Feature::Camera:
        return "Camera"_s;
    case PermissionsPolicy::Feature::Microphone:
        return "Microphone"_s;
    case PermissionsPolicy::Feature::SpeakerSelection:
        return "SpeakerSelection"_s;
    case PermissionsPolicy::Feature::DisplayCapture:
        return "DisplayCapture"_s;
    case PermissionsPolicy::Feature::Gamepad:
        return "Gamepad"_s;
    case PermissionsPolicy::Feature::Geolocation:
        return "Geolocation"_s;
    case PermissionsPolicy::Feature::Payment:
        return "Payment"_s;
    case PermissionsPolicy::Feature::ScreenWakeLock:
        return "ScreenWakeLock"_s;
    case PermissionsPolicy::Feature::SyncXHR:
        return "SyncXHR"_s;
    case PermissionsPolicy::Feature::Fullscreen:
        return "Fullscreen"_s;
    case PermissionsPolicy::Feature::WebShare:
        return "WebShare"_s;
#if ENABLE(DEVICE_ORIENTATION)
    case PermissionsPolicy::Feature::Gyroscope:
        return "Gyroscope"_s;
    case PermissionsPolicy::Feature::Accelerometer:
        return "Accelerometer"_s;
    case PermissionsPolicy::Feature::Magnetometer:
        return "Magnetometer"_s;
#endif
#if ENABLE(WEB_AUTHN)
    case PermissionsPolicy::Feature::PublickeyCredentialsGetRule:
        return "PublickeyCredentialsGet"_s;
    case PermissionsPolicy::Feature::DigitalCredentialsGetRule:
        return "DigitalCredentialsGet"_s;
#endif
#if ENABLE(WEBXR)
    case PermissionsPolicy::Feature::XRSpatialTracking:
        return "XRSpatialTracking"_s;
#endif
    case PermissionsPolicy::Feature::PrivateToken:
        return "PrivateToken"_s;
    case PermissionsPolicy::Feature::Invalid:
        return "Invalid"_s;
    }
    ASSERT_NOT_REACHED();
    return ""_s;
}

// https://w3c.github.io/webappsec-permissions-policy/#serialized-policy-directive
static std::pair<PermissionsPolicy::Feature, StringView> readFeatureIdentifier(StringView value)
{
    value = value.trim(isASCIIWhitespace<UChar>);

    PermissionsPolicy::Feature feature = PermissionsPolicy::Feature::Invalid;
    StringView remainingValue;

    constexpr auto cameraToken { "camera"_s };
    constexpr auto microphoneToken { "microphone"_s };
    constexpr auto speakerSelectionToken { "speaker-selection"_s };
    constexpr auto displayCaptureToken { "display-capture"_s };
    constexpr auto gamepadToken { "gamepad"_s };
    constexpr auto geolocationToken { "geolocation"_s };
    constexpr auto paymentToken { "payment"_s };
    constexpr auto screenWakeLockToken { "screen-wake-lock"_s };
    constexpr auto syncXHRToken { "sync-xhr"_s };
    constexpr auto fullscreenToken { "fullscreen"_s };
    constexpr auto webShareToken { "web-share"_s };
#if ENABLE(DEVICE_ORIENTATION)
    constexpr auto gyroscopeToken { "gyroscope"_s };
    constexpr auto accelerometerToken { "accelerometer"_s };
    constexpr auto magnetometerToken { "magnetometer"_s };
#endif
#if ENABLE(WEB_AUTHN)
    constexpr auto publickeyCredentialsGetRuleToken { "publickey-credentials-get"_s };
    constexpr auto digitalCredentialsGetRuleToken { "digital-credentials-get"_s };
#endif
#if ENABLE(WEBXR)
    constexpr auto xrSpatialTrackingToken { "xr-spatial-tracking"_s };
#endif
    constexpr auto privateTokenToken { "private-token"_s };

    if (value.startsWith(cameraToken)) {
        feature = PermissionsPolicy::Feature::Camera;
        remainingValue = value.substring(cameraToken.length());
    } else if (value.startsWith(microphoneToken)) {
        feature = PermissionsPolicy::Feature::Microphone;
        remainingValue = value.substring(microphoneToken.length());
    } else if (value.startsWith(speakerSelectionToken)) {
        feature = PermissionsPolicy::Feature::SpeakerSelection;
        remainingValue = value.substring(speakerSelectionToken.length());
    } else if (value.startsWith(displayCaptureToken)) {
        feature = PermissionsPolicy::Feature::DisplayCapture;
        remainingValue = value.substring(displayCaptureToken.length());
    } else if (value.startsWith(gamepadToken)) {
        feature = PermissionsPolicy::Feature::Gamepad;
        remainingValue = value.substring(gamepadToken.length());
    } else if (value.startsWith(geolocationToken)) {
        feature = PermissionsPolicy::Feature::Geolocation;
        remainingValue = value.substring(geolocationToken.length());
    } else if (value.startsWith(paymentToken)) {
        feature = PermissionsPolicy::Feature::Payment;
        remainingValue = value.substring(paymentToken.length());
    } else if (value.startsWith(screenWakeLockToken)) {
        feature = PermissionsPolicy::Feature::ScreenWakeLock;
        remainingValue = value.substring(screenWakeLockToken.length());
    } else if (value.startsWith(syncXHRToken)) {
        feature = PermissionsPolicy::Feature::SyncXHR;
        remainingValue = value.substring(syncXHRToken.length());
    } else if (value.startsWith(fullscreenToken)) {
        feature = PermissionsPolicy::Feature::Fullscreen;
        remainingValue = value.substring(fullscreenToken.length());
    } else if (value.startsWith(webShareToken)) {
        feature = PermissionsPolicy::Feature::WebShare;
        remainingValue = value.substring(webShareToken.length());
#if ENABLE(DEVICE_ORIENTATION)
    } else if (value.startsWith(gyroscopeToken)) {
        feature = PermissionsPolicy::Feature::Gyroscope;
        remainingValue = value.substring(gyroscopeToken.length());
    } else if (value.startsWith(accelerometerToken)) {
        feature = PermissionsPolicy::Feature::Accelerometer;
        remainingValue = value.substring(accelerometerToken.length());
    } else if (value.startsWith(magnetometerToken)) {
        feature = PermissionsPolicy::Feature::Magnetometer;
        remainingValue = value.substring(magnetometerToken.length());
#endif
#if ENABLE(WEB_AUTHN)
    } else if (value.startsWith(publickeyCredentialsGetRuleToken)) {
        feature = PermissionsPolicy::Feature::PublickeyCredentialsGetRule;
        remainingValue = value.substring(publickeyCredentialsGetRuleToken.length());
    } else if (value.startsWith(digitalCredentialsGetRuleToken)) {
        feature = PermissionsPolicy::Feature::DigitalCredentialsGetRule;
        remainingValue = value.substring(digitalCredentialsGetRuleToken.length());
#endif
#if ENABLE(WEBXR)
    } else if (value.startsWith(xrSpatialTrackingToken)) {
        feature = PermissionsPolicy::Feature::XRSpatialTracking;
        remainingValue = value.substring(xrSpatialTrackingToken.length());
#endif
    } else if (value.startsWith(privateTokenToken)) {
        feature = PermissionsPolicy::Feature::PrivateToken;
        remainingValue = value.substring(privateTokenToken.length());
    }

    // FIXME: webkit.org/b/274159.
    // Using colon as delimiter is no longer standard behavior. We should drop this,
    // update tests and use splitOnAsciiWhiteSpace to read feature identifier.
    if (remainingValue.startsWith(":"_s))
        remainingValue = remainingValue.substring(1);

    return { feature, remainingValue };
}

static ASCIILiteral defaultAllowlistValue(PermissionsPolicy::Feature feature)
{
    switch (feature) {
    case PermissionsPolicy::Feature::Gamepad:
    case PermissionsPolicy::Feature::SyncXHR:
        return "*"_s;
    case PermissionsPolicy::Feature::Camera:
    case PermissionsPolicy::Feature::Microphone:
    case PermissionsPolicy::Feature::SpeakerSelection:
    case PermissionsPolicy::Feature::DisplayCapture:
    case PermissionsPolicy::Feature::Geolocation:
    case PermissionsPolicy::Feature::Payment:
    case PermissionsPolicy::Feature::ScreenWakeLock:
    case PermissionsPolicy::Feature::Fullscreen:
    case PermissionsPolicy::Feature::WebShare:
#if ENABLE(DEVICE_ORIENTATION)
    case PermissionsPolicy::Feature::Gyroscope:
    case PermissionsPolicy::Feature::Accelerometer:
    case PermissionsPolicy::Feature::Magnetometer:
#endif
#if ENABLE(WEB_AUTHN)
    case PermissionsPolicy::Feature::PublickeyCredentialsGetRule:
    case PermissionsPolicy::Feature::DigitalCredentialsGetRule:
#endif
#if ENABLE(WEBXR)
    case PermissionsPolicy::Feature::XRSpatialTracking:
#endif
    case PermissionsPolicy::Feature::PrivateToken:
        return "'self'"_s;
    case PermissionsPolicy::Feature::Invalid:
        return "'none'"_s;
    }

    ASSERT_NOT_REACHED();
    return "'none'"_s;
}

static void forEachFeature(const Function<void(PermissionsPolicy::Feature)>& apply)
{
    for (uint8_t index = 0, end = static_cast<uint8_t>(PermissionsPolicy::Feature::Invalid); index < end; ++index)
        apply(static_cast<PermissionsPolicy::Feature>(index));
}

static bool isFeatureAllowedByDefaultAllowlist(PermissionsPolicy::Feature feature, const SecurityOriginData& origin, const SecurityOriginData& documentOrigin)
{
    auto allowlistValue = defaultAllowlistValue(feature);
    if (allowlistValue == "*"_s)
        return true;

    if (equalIgnoringASCIICase(allowlistValue, "'self'"_s))
        return origin == documentOrigin;

    return false;
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-check-permissions-policy
static bool checkPermissionsPolicy(const PermissionsPolicy& permissionsPolicy, PermissionsPolicy::Feature feature, const SecurityOriginData& origin, const SecurityOriginData& documentOrigin)
{
    if (!permissionsPolicy.inheritedPolicyValueForFeature(feature))
        return false;

    return isFeatureAllowedByDefaultAllowlist(feature, origin, documentOrigin);
}

// Similar to https://infra.spec.whatwg.org/#split-on-ascii-whitespace but only extract one token at a time.
static std::pair<StringView, StringView> splitOnAsciiWhiteSpace(StringView input)
{
    input = input.trim(isASCIIWhitespace<UChar>);
    auto position = input.find(isASCIIWhitespace<UChar>);
    if (position == notFound)
        return { input, StringView { } };

    return  { input.left(position), input.substring(position) };
}

// https://w3c.github.io/webappsec-permissions-policy/#declared-origin
static Ref<SecurityOrigin> declaredOrigin(const HTMLIFrameElement& iframe)
{
    if (iframe.document().isSandboxed(SandboxFlag::Origin) || (iframe.sandboxFlags().contains(SandboxFlag::Origin)))
        return SecurityOrigin::createOpaque();

    if (iframe.hasAttributeWithoutSynchronization(srcdocAttr))
        return iframe.document().securityOrigin();

    if (iframe.hasAttributeWithoutSynchronization(srcAttr)) {
        auto url = iframe.document().completeURL(iframe.getAttribute(srcAttr));
        if (url.isValid()) {
            if (url.protocolIsInHTTPFamily())
                return SecurityOrigin::create(url);
            if (auto contentDocument = iframe.contentDocument())
                return contentDocument->securityOrigin();
        }
    }

    return iframe.document().securityOrigin();
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-is-feature-enabled
static bool computeFeatureEnabled(PermissionsPolicy::Feature feature, const Document& document, const SecurityOriginData& origin, PermissionsPolicy::ShouldReportViolation shouldReportViolation)
{
    bool enabled = checkPermissionsPolicy(document.permissionsPolicy(), feature, origin, document.securityOrigin().data());
    // FIXME: Spec suggests generating violation report for Reporting API but we only add log now.
    if (!enabled && shouldReportViolation == PermissionsPolicy::ShouldReportViolation::Yes) {
        if (RefPtr window = document.domWindow())
            window->printErrorMessage(makeString("Permission policy '"_s, toFeatureNameForLogging(feature), "' check failed for document with origin '"_s, origin.toString(), "'."_s));
    }

    return enabled;
}

static Allowlist parseAllowlist(StringView value, const SecurityOriginData& containerOrigin, const SecurityOriginData& targetOrigin)
{
    if (value.isEmpty())
        return Allowlist { targetOrigin };

    HashSet<SecurityOriginData> allowedOrigins;
    while (!value.isEmpty()) {
        auto [token, remainingValue] = splitOnAsciiWhiteSpace(value);
        if (!token.isEmpty()) {
            if (token == "*"_s)
                return Allowlist::AllowAllOrigins { };

            if (equalIgnoringASCIICase(token, "'none'"_s))
                return Allowlist { };

            if (equalIgnoringASCIICase(token, "'self'"_s))
                allowedOrigins.add(containerOrigin);
            else if (equalIgnoringASCIICase(token, "'src'"_s))
                allowedOrigins.add(targetOrigin);
            else {
                URL url { { }, token.toString() };
                if (url.isValid()) {
                    auto origin = SecurityOriginData::fromURL(url);
                    if (!origin.isOpaque())
                        allowedOrigins.add(origin);
                }
            }
        }
        value = remainingValue;
    }

    return Allowlist { WTFMove(allowedOrigins) };
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-parse-policy-directive
static PermissionsPolicy::PolicyDirective parsePolicyDirective(StringView value, const SecurityOriginData& containerOrigin, const SecurityOriginData& targetOrigin)
{
    PermissionsPolicy::PolicyDirective result;
    for (auto item : value.split(';')) {
        auto [feature, remainingItem] = readFeatureIdentifier(item);
        if (feature == PermissionsPolicy::Feature::Invalid)
            continue;

        result.add(feature, parseAllowlist(remainingItem, containerOrigin, targetOrigin));
    }

    return result;
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-process-policy-attributes
PermissionsPolicy::PolicyDirective PermissionsPolicy::processPermissionsPolicyAttribute(const HTMLIFrameElement& iframe)
{
    auto allowAttributeValue = iframe.attributeWithoutSynchronization(allowAttr);
    auto policyDirective = parsePolicyDirective(allowAttributeValue, iframe.document().securityOrigin().data(), declaredOrigin(iframe)->data());

    if (iframe.hasAttribute(allowfullscreenAttr) || iframe.hasAttribute(webkitallowfullscreenAttr))
        policyDirective.add(Feature::Fullscreen, Allowlist::AllowAllOrigins { });

    return policyDirective;
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-get-feature-value-for-origin
static bool featureValueForOrigin(PermissionsPolicy::Feature feature, const PermissionsPolicy& documentPermissionsPolicy, const SecurityOriginData&)
{
    // Declared policy is not implemented yet, so origin is unused.
    return documentPermissionsPolicy.inheritedPolicyValueForFeature(feature);
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-define-inherited-policy-in-container
bool PermissionsPolicy::computeInheritedPolicyValueInContainer(Feature feature, const std::optional<OwnerPermissionsPolicyData>& ownerPermissionsPolicy, const SecurityOriginData& origin) const
{
    if (!ownerPermissionsPolicy)
        return true;

    auto documentPolicy = ownerPermissionsPolicy->documentPolicy;
    auto documentOrigin = ownerPermissionsPolicy->documentOrigin;
    if (!featureValueForOrigin(feature, documentPolicy, documentOrigin))
        return false;

    if (!featureValueForOrigin(feature, documentPolicy, origin))
        return false;

    auto containerPolicy = ownerPermissionsPolicy->containerPolicy;
    if (auto iterator = containerPolicy.find(feature); iterator != containerPolicy.end())
        return iterator->value.matches(origin);

    return isFeatureAllowedByDefaultAllowlist(feature, origin, documentOrigin);
}

bool PermissionsPolicy::inheritedPolicyValueForFeature(Feature feature) const
{
    ASSERT(feature != Feature::Invalid);

    return m_inheritedPolicy.contains(feature);
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-create-for-navigable
PermissionsPolicy::PermissionsPolicy(const Document& document)
{
    auto ownerPermissionsPolicy = document.ownerPermissionsPolicy();
    forEachFeature([&](auto feature) {
        if (computeInheritedPolicyValueInContainer(feature, ownerPermissionsPolicy, document.securityOrigin().data()))
            m_inheritedPolicy.add(feature);
    });
}

// https://w3c.github.io/webappsec-permissions-policy/#empty-permissions-policy
PermissionsPolicy::PermissionsPolicy()
{
    forEachFeature([&](auto feature) {
        m_inheritedPolicy.add(feature);
    });
}

bool PermissionsPolicy::isFeatureEnabled(Feature feature, const Document& document, ShouldReportViolation shouldReportViolation)
{
    return computeFeatureEnabled(feature, document, document.securityOrigin().data(), shouldReportViolation);
}

}
