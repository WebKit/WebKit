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

static const char* policyTypeName(PermissionsPolicy::Type type)
{
    switch (type) {
    case PermissionsPolicy::Type::Camera:
        return "Camera";
    case PermissionsPolicy::Type::Microphone:
        return "Microphone";
    case PermissionsPolicy::Type::SpeakerSelection:
        return "SpeakerSelection";
    case PermissionsPolicy::Type::DisplayCapture:
        return "DisplayCapture";
    case PermissionsPolicy::Type::Gamepad:
        return "Gamepad";
    case PermissionsPolicy::Type::Geolocation:
        return "Geolocation";
    case PermissionsPolicy::Type::Payment:
        return "Payment";
    case PermissionsPolicy::Type::ScreenWakeLock:
        return "ScreenWakeLock";
    case PermissionsPolicy::Type::SyncXHR:
        return "SyncXHR";
    case PermissionsPolicy::Type::Fullscreen:
        return "Fullscreen";
    case PermissionsPolicy::Type::WebShare:
        return "WebShare";
#if ENABLE(DEVICE_ORIENTATION)
    case PermissionsPolicy::Type::Gyroscope:
        return "Gyroscope";
    case PermissionsPolicy::Type::Accelerometer:
        return "Accelerometer";
    case PermissionsPolicy::Type::Magnetometer:
        return "Magnetometer";
#endif
#if ENABLE(WEB_AUTHN)
    case PermissionsPolicy::Type::PublickeyCredentialsGetRule:
        return "PublickeyCredentialsGet";
#endif
#if ENABLE(WEBXR)
    case PermissionsPolicy::Type::XRSpatialTracking:
        return "XRSpatialTracking";
#endif
    case PermissionsPolicy::Type::PrivateToken:
        return "PrivateToken";
    }
    ASSERT_NOT_REACHED();
    return "";
}

bool isPermissionsPolicyAllowedByDocumentAndAllOwners(PermissionsPolicy::Type type, const Document& document, LogPermissionsPolicyFailure logFailure)
{
    Ref topDocument = document.topDocument();
    RefPtr ancestorDocument = &document;
    while (ancestorDocument.get() != topDocument.ptr()) {
        if (!ancestorDocument) {
            if (logFailure == LogPermissionsPolicyFailure::Yes && document.domWindow())
                document.domWindow()->printErrorMessage(makeString("Feature policy '", policyTypeName(type), "' check failed."));
            return false;
        }

        RefPtr ownerElement = ancestorDocument->ownerElement();
        RefPtr iframe = dynamicDowncast<HTMLIFrameElement>(ownerElement.get());

        bool isAllowedByPermissionsPolicy = false;
        if (iframe)
            isAllowedByPermissionsPolicy = iframe->permissionsPolicy().allows(type, ancestorDocument->securityOrigin().data());
        else if (ownerElement)
            isAllowedByPermissionsPolicy = PermissionsPolicy::defaultPolicy(ownerElement->document()).allows(type, ancestorDocument->securityOrigin().data());

        if (!isAllowedByPermissionsPolicy) {
            if (logFailure == LogPermissionsPolicyFailure::Yes && document.domWindow()) {
                String allowValue;
                if (iframe)
                    allowValue = iframe->attributeWithoutSynchronization(HTMLNames::allowAttr);
                document.domWindow()->printErrorMessage(makeString("Feature policy '", policyTypeName(type), "' check failed for element with origin '", document.securityOrigin().toString(), "' and allow attribute '", allowValue, "'."));
            }
            return false;
        }

        ancestorDocument = ancestorDocument->parentDocument();
    }

    return true;
}

static bool isAllowedByPermissionsPolicy(const PermissionsPolicy::AllowRule& rule, const SecurityOriginData& origin)
{
    switch (rule.type) {
    case PermissionsPolicy::AllowRule::Type::None:
        return false;
    case PermissionsPolicy::AllowRule::Type::All:
        return true;
    case PermissionsPolicy::AllowRule::Type::List:
        return rule.allowedList.contains(origin);
    }
    ASSERT_NOT_REACHED();
    return false;
}

static inline void processOriginItem(Document& document, const HTMLIFrameElement& iframe, PermissionsPolicy::AllowRule& rule, StringView item)
{
    if (rule.type == PermissionsPolicy::AllowRule::Type::None)
        return;

    item = item.trim(isASCIIWhitespace<UChar>);
    if (item == "'src'"_s) {
        auto srcURL = document.completeURL(iframe.getAttribute(srcAttr));
        if (srcURL.isValid()) {
            RefPtr<SecurityOrigin> allowedOrigin;
            if (srcURL.protocolIsInHTTPFamily())
                allowedOrigin = SecurityOrigin::create(srcURL);
            else if (auto contentDocument = iframe.contentDocument())
                allowedOrigin = &contentDocument->securityOrigin();
            if (allowedOrigin)
                rule.allowedList.add(allowedOrigin->data());
        }
        return;
    }

    if (item == "*"_s) {
        rule.type = PermissionsPolicy::AllowRule::Type::All;
        return;
    }

    if (item == "'self'"_s) {
        rule.allowedList.add(document.securityOrigin().data());
        return;
    }
    if (item == "'none'"_s) {
        rule.type = PermissionsPolicy::AllowRule::Type::None;
        return;
    }
    URL url { { }, item.toString() };
    if (url.isValid())
        rule.allowedList.add(SecurityOriginData::fromURL(url));
}

static inline void updateList(Document& document, const HTMLIFrameElement& iframe, PermissionsPolicy::AllowRule& rule, StringView value)
{
    if (value.isEmpty()) {
        if (document.quirks().shouldStarBePermissionsPolicyDefaultValue()) {
            rule.type = PermissionsPolicy::AllowRule::Type::All;
            return;
        }

        // The allowlist for the features named in the attribute may be empty; in that case,
        // the default value for the allowlist is 'src', which represents the origin of the
        // URL in the iframe’s src attribute.
        // https://w3c.github.io/webappsec-permissions-policy/#iframe-allow-attribute
        processOriginItem(document, iframe, rule, "'src'"_s);
        return;
    }

    while (!value.isEmpty()) {
        auto position = value.find(isASCIIWhitespace<UChar>);
        if (position == notFound) {
            processOriginItem(document, iframe, rule, value);
            return;
        }

        processOriginItem(document, iframe, rule, value.left(position));
        value = value.substring(position + 1).trim(isASCIIWhitespace<UChar>);
    }
}

PermissionsPolicy PermissionsPolicy::parse(Document& document, const HTMLIFrameElement* iframe, StringView allowAttributeValue)
{
    PermissionsPolicy policy;
    bool isCameraInitialized = false;
    bool isMicrophoneInitialized = false;
    bool isSpeakerSelectionInitialized = false;
    bool isDisplayCaptureInitialized = false;
    bool isGamepadInitialized = false;
    bool isGeolocationInitialized = false;
    bool isPaymentInitialized = false;
    bool isScreenWakeLockInitialized = false;
    bool isSyncXHRInitialized = false;
    bool isFullscreenInitialized = false;
    bool isWebShareInitialized = false;
#if ENABLE(DEVICE_ORIENTATION)
    bool isGyroscopeInitialized = false;
    bool isAccelerometerInitialized = false;
    bool isMagnetometerInitialized = false;
#endif
#if ENABLE(WEB_AUTHN)
    bool isPublickeyCredentialsGetInitialized = false;
#endif
#if ENABLE(WEBXR)
    bool isXRSpatialTrackingInitialized = false;
#endif
    bool isPrivateTokenInitialized = false;
    if (iframe) {
        for (auto allowItem : allowAttributeValue.split(';')) {
            auto item = allowItem.trim(isASCIIWhitespace<UChar>);
            if (item.startsWith("camera"_s)) {
                isCameraInitialized = true;
                updateList(document, *iframe, policy.m_cameraRule, item.substring(7));
                continue;
            }
            if (item.startsWith("microphone"_s)) {
                isMicrophoneInitialized = true;
                updateList(document, *iframe, policy.m_microphoneRule, item.substring(11));
                continue;
            }
            if (item.startsWith("speaker-selection"_s)) {
                isSpeakerSelectionInitialized = true;
                updateList(document, *iframe, policy.m_speakerSelectionRule, item.substring(18));
                continue;
            }
            if (item.startsWith("display-capture"_s)) {
                isDisplayCaptureInitialized = true;
                updateList(document, *iframe, policy.m_displayCaptureRule, item.substring(16));
                continue;
            }
            if (item.startsWith("gamepad"_s)) {
                isGamepadInitialized = true;
                updateList(document, *iframe, policy.m_gamepadRule, item.substring(8));
                continue;
            }
            if (item.startsWith("geolocation"_s)) {
                isGeolocationInitialized = true;
                updateList(document, *iframe, policy.m_geolocationRule, item.substring(12));
                continue;
            }
            if (item.startsWith("payment"_s)) {
                isPaymentInitialized = true;
                updateList(document, *iframe, policy.m_paymentRule, item.substring(8));
                continue;
            }
            if (item.startsWith("screen-wake-lock"_s)) {
                isScreenWakeLockInitialized = true;
                updateList(document, *iframe, policy.m_screenWakeLockRule, item.substring(17));
                continue;
            }
            if (item.startsWith("sync-xhr"_s)) {
                isSyncXHRInitialized = true;
                updateList(document, *iframe, policy.m_syncXHRRule, item.substring(9));
                continue;
            }
            if (item.startsWith("fullscreen"_s)) {
                isFullscreenInitialized = true;
                updateList(document, *iframe, policy.m_fullscreenRule, item.substring(11));
                continue;
            }
            if (item.startsWith("web-share"_s)) {
                isWebShareInitialized = true;
                updateList(document, *iframe, policy.m_webShareRule, item.substring(10));
                continue;
            }
#if ENABLE(DEVICE_ORIENTATION)
            if (item.startsWith("gyroscope"_s)) {
                isGyroscopeInitialized = true;
                updateList(document, *iframe, policy.m_gyroscopeRule, item.substring(10));
                continue;
            }
            if (item.startsWith("accelerometer"_s)) {
                isAccelerometerInitialized = true;
                updateList(document, *iframe, policy.m_accelerometerRule, item.substring(14));
                continue;
            }
            if (item.startsWith("magnetometer"_s)) {
                isMagnetometerInitialized = true;
                updateList(document, *iframe, policy.m_magnetometerRule, item.substring(13));
                continue;
            }
#endif
#if ENABLE(WEB_AUTHN)
            if (item.startsWith("publickey-credentials-get"_s)) {
                isPublickeyCredentialsGetInitialized = true;
                updateList(document, *iframe, policy.m_publickeyCredentialsGetRule, item.substring(26));
                continue;
            }
#endif
#if ENABLE(WEBXR)
            if (item.startsWith("xr-spatial-tracking"_s)) {
                isXRSpatialTrackingInitialized = true;
                updateList(document, *iframe, policy.m_xrSpatialTrackingRule, item.substring(19));
                continue;
            }
#endif
            constexpr auto privateTokenToken { "private-token"_s };
            if (item.startsWith(privateTokenToken)) {
                isPrivateTokenInitialized = true;
                updateList(document, *iframe, policy.m_privateTokenRule, item.substring(privateTokenToken.length()));
                continue;
            }
        }
    }

    // By default, camera, microphone, speaker-selection, display-capture, fullscreen, xr-spatial-tracking, screen-wake-lock, and web-share policy is 'self'.
    if (!isCameraInitialized)
        policy.m_cameraRule.allowedList.add(document.securityOrigin().data());
    if (!isMicrophoneInitialized)
        policy.m_microphoneRule.allowedList.add(document.securityOrigin().data());
    if (!isSpeakerSelectionInitialized)
        policy.m_speakerSelectionRule.allowedList.add(document.securityOrigin().data());
    if (!isDisplayCaptureInitialized)
        policy.m_displayCaptureRule.allowedList.add(document.securityOrigin().data());
    if (!isGamepadInitialized)
        policy.m_gamepadRule.type = PermissionsPolicy::AllowRule::Type::All;
    if (!isScreenWakeLockInitialized)
        policy.m_screenWakeLockRule.allowedList.add(document.securityOrigin().data());
    if (!isGeolocationInitialized)
        policy.m_geolocationRule.allowedList.add(document.securityOrigin().data());
    if (!isPaymentInitialized)
        policy.m_paymentRule.allowedList.add(document.securityOrigin().data());
    if (!isWebShareInitialized)
        policy.m_webShareRule.allowedList.add(document.securityOrigin().data());
#if ENABLE(DEVICE_ORIENTATION)
    if (!isGyroscopeInitialized)
        policy.m_gyroscopeRule.allowedList.add(document.securityOrigin().data());
    if (!isAccelerometerInitialized)
        policy.m_accelerometerRule.allowedList.add(document.securityOrigin().data());
    if (!isMagnetometerInitialized)
        policy.m_magnetometerRule.allowedList.add(document.securityOrigin().data());
#endif
#if ENABLE(WEB_AUTHN)
    if (!isPublickeyCredentialsGetInitialized)
        policy.m_publickeyCredentialsGetRule.allowedList.add(document.securityOrigin().data());
#endif
#if ENABLE(WEBXR)
    if (!isXRSpatialTrackingInitialized)
        policy.m_xrSpatialTrackingRule.allowedList.add(document.securityOrigin().data());
#endif
    if (!isPrivateTokenInitialized)
        policy.m_privateTokenRule.allowedList.add(document.securityOrigin().data());

    // https://w3c.github.io/webappsec-permissions-policy/#algo-process-policy-attributes
    // 9.5 Process Feature Policy Attributes
    // 3.1 If element’s allowfullscreen attribute is specified, and container policy does
    //     not contain an allowlist for fullscreen,
    if (!isFullscreenInitialized) {
        if (iframe && (iframe->hasAttribute(allowfullscreenAttr) || iframe->hasAttribute(webkitallowfullscreenAttr))) {
            // 3.1.1 Construct a new declaration for fullscreen, whose allowlist is the special value *.
            policy.m_fullscreenRule.type = PermissionsPolicy::AllowRule::Type::All;
        } else {
            // https://fullscreen.spec.whatwg.org/#permissions-policy-integration
            // The default allowlist is 'self'.
            policy.m_fullscreenRule.allowedList.add(document.securityOrigin().data());
        }
    }

    if (!isSyncXHRInitialized)
        policy.m_syncXHRRule.type = AllowRule::Type::All;

    return policy;
}

bool PermissionsPolicy::allows(Type type, const SecurityOriginData& origin) const
{
    switch (type) {
    case Type::Camera:
        return isAllowedByPermissionsPolicy(m_cameraRule, origin);
    case Type::Microphone:
        return isAllowedByPermissionsPolicy(m_microphoneRule, origin);
    case Type::SpeakerSelection:
        return isAllowedByPermissionsPolicy(m_speakerSelectionRule, origin);
    case Type::DisplayCapture:
        return isAllowedByPermissionsPolicy(m_displayCaptureRule, origin);
    case Type::Gamepad:
        return isAllowedByPermissionsPolicy(m_gamepadRule, origin);
    case Type::Geolocation:
        return isAllowedByPermissionsPolicy(m_geolocationRule, origin);
    case Type::Payment:
        return isAllowedByPermissionsPolicy(m_paymentRule, origin);
    case Type::ScreenWakeLock:
        return isAllowedByPermissionsPolicy(m_screenWakeLockRule, origin);
    case Type::SyncXHR:
        return isAllowedByPermissionsPolicy(m_syncXHRRule, origin);
    case Type::Fullscreen:
        return isAllowedByPermissionsPolicy(m_fullscreenRule, origin);
    case Type::WebShare:
        return isAllowedByPermissionsPolicy(m_webShareRule, origin);
#if ENABLE(DEVICE_ORIENTATION)
    case Type::Gyroscope:
        return isAllowedByPermissionsPolicy(m_gyroscopeRule, origin);
    case Type::Accelerometer:
        return isAllowedByPermissionsPolicy(m_accelerometerRule, origin);
    case Type::Magnetometer:
        return isAllowedByPermissionsPolicy(m_magnetometerRule, origin);
#endif
#if ENABLE(WEB_AUTHN)
    case Type::PublickeyCredentialsGetRule:
        return isAllowedByPermissionsPolicy(m_publickeyCredentialsGetRule, origin);
#endif
#if ENABLE(WEBXR)
    case Type::XRSpatialTracking:
        return isAllowedByPermissionsPolicy(m_xrSpatialTrackingRule, origin);
#endif
    case Type::PrivateToken:
        return isAllowedByPermissionsPolicy(m_privateTokenRule, origin);
    }
    ASSERT_NOT_REACHED();
    return false;
}

}
