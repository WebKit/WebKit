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

#include "Document.h"
#include "DocumentInlines.h"
#include "ElementInlines.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "LocalDOMWindow.h"
#include "Quirks.h"
#include "SecurityOrigin.h"

namespace WebCore {

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

// Similar to https://infra.spec.whatwg.org/#split-on-ascii-whitespace but only extract one token at a time.
static std::pair<StringView, StringView> splitOnAsciiWhiteSpace(StringView input)
{
    input = input.trim(isASCIIWhitespace<UChar>);
    auto position = input.find(isASCIIWhitespace<UChar>);
    if (position == notFound)
        return { input, StringView { } };

    return  { input.left(position), input.substring(position) };
}

bool isPermissionsPolicyAllowedByDocumentAndAllOwners(PermissionsPolicy::Feature feature, const Document& document, LogPermissionsPolicyFailure logFailure)
{
    Ref topDocument = document.topDocument();
    RefPtr ancestorDocument = &document;
    while (ancestorDocument.get() != topDocument.ptr()) {
        if (!ancestorDocument) {
            if (logFailure == LogPermissionsPolicyFailure::Yes && document.domWindow())
                document.domWindow()->printErrorMessage(makeString("Permission policy '"_s, toFeatureNameForLogging(feature), "' check failed."_s));
            return false;
        }

        RefPtr ownerElement = ancestorDocument->ownerElement();
        RefPtr iframe = dynamicDowncast<HTMLIFrameElement>(ownerElement.get());

        bool isAllowedByPermissionsPolicy = false;
        if (iframe)
            isAllowedByPermissionsPolicy = iframe->permissionsPolicy().allows(feature, ancestorDocument->securityOrigin().data());
        else if (ownerElement)
            isAllowedByPermissionsPolicy = PermissionsPolicy::defaultPolicy(ownerElement->document()).allows(feature, ancestorDocument->securityOrigin().data());

        if (!isAllowedByPermissionsPolicy) {
            if (logFailure == LogPermissionsPolicyFailure::Yes && document.domWindow()) {
                String allowValue;
                if (iframe)
                    allowValue = iframe->attributeWithoutSynchronization(HTMLNames::allowAttr);
                document.domWindow()->printErrorMessage(makeString("Permission policy '"_s, toFeatureNameForLogging(feature), "' check failed for element with origin '"_s, document.securityOrigin().toString(), "' and allow attribute '"_s, allowValue, "'."_s));
            }
            return false;
        }

        ancestorDocument = ancestorDocument->parentDocument();
    }

    return true;
}

// This is simplified version of https://w3c.github.io/webappsec-permissions-policy/#matches.
bool PermissionsPolicy::Allowlist::matches(const SecurityOriginData& origin) const
{
    return std::visit(WTF::makeVisitor([&origin](const HashSet<SecurityOriginData>& origins) -> bool {
        return origins.contains(origin);
    }, [&] (const auto&) {
        return true;
    }), m_origins);
}

PermissionsPolicy::Allowlist PermissionsPolicy::parseAllowlist(StringView value, const SecurityOriginData& containerOrigin, const SecurityOriginData& targetOrigin, bool useStarAsDefaultAllowlistValue)
{
    if (value.isEmpty()) {
        if (useStarAsDefaultAllowlistValue)
            return Allowlist { Allowlist::AllowAllOrigins };

        return Allowlist { targetOrigin };
    }

    HashSet<SecurityOriginData> allowedOrigins;
    while (!value.isEmpty()) {
        auto [token, remainingValue] = splitOnAsciiWhiteSpace(value);
        if (!token.isEmpty()) {
            if (token == "*"_s)
                return PermissionsPolicy::Allowlist { Allowlist::AllowAllOrigins };

            if (equalIgnoringASCIICase(token, "'none'"_s))
                return PermissionsPolicy::Allowlist { };

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

    return PermissionsPolicy::Allowlist { WTFMove(allowedOrigins) };
}

// https://w3c.github.io/webappsec-permissions-policy/#algo-parse-policy-directive
PermissionsPolicy::PolicyDirective PermissionsPolicy::parsePolicyDirective(StringView value, const SecurityOriginData& containerOrigin, const SecurityOriginData& targetOrigin, bool useStarAsDefaultAllowlistValue)
{
    PermissionsPolicy::PolicyDirective result;
    for (auto item : value.split(';')) {
        auto [feature, remainingItem] = readFeatureIdentifier(item);
        if (feature == Feature::Invalid)
            continue;

        result.add(feature, parseAllowlist(remainingItem, containerOrigin, targetOrigin, useStarAsDefaultAllowlistValue));
    }

    return result;
}

// https://w3c.github.io/webappsec-permissions-policy/#declared-origin
Ref<SecurityOrigin> PermissionsPolicy::declaredOrigin(const HTMLIFrameElement& iframe) const
{
    if (iframe.document().isSandboxed(SandboxOrigin) || (iframe.sandboxFlags() & SandboxOrigin))
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

PermissionsPolicy::PermissionsPolicy(Document& document, const HTMLIFrameElement* iframe)
{
    // https://w3c.github.io/webappsec-permissions-policy/#algo-process-policy-attributes
    if (iframe) {
        auto allowAttributeValue = iframe->attributeWithoutSynchronization(allowAttr);
        m_effectivePolicy = parsePolicyDirective(allowAttributeValue, iframe->document().securityOrigin().data(), declaredOrigin(*iframe)->data(), document.quirks().shouldStarBePermissionsPolicyDefaultValue());

        if (iframe->hasAttribute(allowfullscreenAttr) || iframe->hasAttribute(webkitallowfullscreenAttr))
            m_effectivePolicy.add(Feature::Fullscreen, Allowlist { Allowlist::AllowAllOrigins });
    }

    // Default allowlists: https://w3c.github.io/webappsec-permissions-policy/#default-allowlists
    auto selfOrigin = document.securityOrigin().data();
    auto allowlistWithSelfOrigin = Allowlist { selfOrigin };
    auto allowlistWithAllOrigins = Allowlist { Allowlist::AllowAllOrigins };
    m_effectivePolicy.add(Feature::Camera, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::Microphone, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::SpeakerSelection, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::DisplayCapture, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::Gamepad, allowlistWithAllOrigins);
    m_effectivePolicy.add(Feature::Geolocation, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::Payment, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::ScreenWakeLock, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::SyncXHR, allowlistWithAllOrigins);
    m_effectivePolicy.add(Feature::Fullscreen, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::WebShare, allowlistWithSelfOrigin);
#if ENABLE(DEVICE_ORIENTATION)
    m_effectivePolicy.add(Feature::Gyroscope, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::Accelerometer, allowlistWithSelfOrigin);
    m_effectivePolicy.add(Feature::Magnetometer, allowlistWithSelfOrigin);
#endif
#if ENABLE(WEB_AUTHN)
    m_effectivePolicy.add(Feature::PublickeyCredentialsGetRule, allowlistWithSelfOrigin);
#endif
#if ENABLE(WEBXR)
    m_effectivePolicy.add(Feature::XRSpatialTracking, allowlistWithSelfOrigin);
#endif
    m_effectivePolicy.add(Feature::PrivateToken, allowlistWithSelfOrigin);

    ASSERT(m_effectivePolicy.size() == static_cast<unsigned>(Feature::Invalid));
}

bool PermissionsPolicy::allows(Feature feature, const SecurityOriginData& origin) const
{
    auto iter = m_effectivePolicy.find(feature);
    return iter != m_effectivePolicy.end() ? iter->value.matches(origin) : false;
}

}
