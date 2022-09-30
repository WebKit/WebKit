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
#include "FeaturePolicy.h"

#include "DOMWindow.h"
#include "Document.h"
#include "ElementInlines.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLParserIdioms.h"
#include "SecurityOrigin.h"

namespace WebCore {

using namespace HTMLNames;

static const char* policyTypeName(FeaturePolicy::Type type)
{
    switch (type) {
    case FeaturePolicy::Type::Camera:
        return "Camera";
    case FeaturePolicy::Type::Microphone:
        return "Microphone";
    case FeaturePolicy::Type::SpeakerSelection:
        return "SpeakerSelection";
    case FeaturePolicy::Type::DisplayCapture:
        return "DisplayCapture";
    case FeaturePolicy::Type::Geolocation:
        return "Geolocation";
    case FeaturePolicy::Type::Payment:
        return "Payment";
    case FeaturePolicy::Type::ScreenWakeLock:
        return "ScreenWakeLock";
    case FeaturePolicy::Type::SyncXHR:
        return "SyncXHR";
    case FeaturePolicy::Type::Fullscreen:
        return "Fullscreen";
    case FeaturePolicy::Type::WebShare:
        return "WebShare";
#if ENABLE(DEVICE_ORIENTATION)
    case FeaturePolicy::Type::Gyroscope:
        return "Gyroscope";
    case FeaturePolicy::Type::Accelerometer:
        return "Accelerometer";
    case FeaturePolicy::Type::Magnetometer:
        return "Magnetometer";
#endif
#if ENABLE(WEB_AUTHN)
    case FeaturePolicy::Type::PublickeyCredentialsGetRule:
        return "PublickeyCredentialsGet";
#endif
#if ENABLE(WEBXR)
    case FeaturePolicy::Type::XRSpatialTracking:
        return "XRSpatialTracking";
#endif
    }
    ASSERT_NOT_REACHED();
    return "";
}

bool isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type type, const Document& document, LogFeaturePolicyFailure logFailure)
{
    auto& topDocument = document.topDocument();
    auto* ancestorDocument = &document;
    while (ancestorDocument != &topDocument) {
        if (!ancestorDocument) {
            if (logFailure == LogFeaturePolicyFailure::Yes && document.domWindow())
                document.domWindow()->printErrorMessage(makeString("Feature policy '", policyTypeName(type), "' check failed."));
            return false;
        }

        auto* ownerElement = ancestorDocument->ownerElement();
        if (is<HTMLIFrameElement>(ownerElement)) {
            const auto& featurePolicy = downcast<HTMLIFrameElement>(ownerElement)->featurePolicy();
            if (!featurePolicy.allows(type, ancestorDocument->securityOrigin().data())) {
                if (logFailure == LogFeaturePolicyFailure::Yes && document.domWindow()) {
                    auto& allowValue = downcast<HTMLIFrameElement>(ownerElement)->attributeWithoutSynchronization(HTMLNames::allowAttr);
                    document.domWindow()->printErrorMessage(makeString("Feature policy '", policyTypeName(type), "' check failed for iframe with origin '", document.securityOrigin().toString(), "' and allow attribute '", allowValue, "'."));
                }
                return false;
            }
        }

        ancestorDocument = ancestorDocument->parentDocument();
    }

    return true;
}

static bool isAllowedByFeaturePolicy(const FeaturePolicy::AllowRule& rule, const SecurityOriginData& origin)
{
    switch (rule.type) {
    case FeaturePolicy::AllowRule::Type::None:
        return false;
    case FeaturePolicy::AllowRule::Type::All:
        return true;
    case FeaturePolicy::AllowRule::Type::List:
        return rule.allowedList.contains(origin);
    }
    ASSERT_NOT_REACHED();
    return false;
}

static inline void processOriginItem(Document& document, FeaturePolicy::AllowRule& rule, StringView item)
{
    if (rule.type == FeaturePolicy::AllowRule::Type::None)
        return;

    item = item.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    // FIXME: Support 'src'.
    if (item == "'src'"_s)
        return;

    if (item == "*"_s) {
        rule.type = FeaturePolicy::AllowRule::Type::All;
        return;
    }

    if (item == "'self'"_s) {
        rule.allowedList.add(document.securityOrigin().data());
        return;
    }
    if (item == "'none'"_s) {
        rule.type = FeaturePolicy::AllowRule::Type::None;
        return;
    }
    URL url { { }, item.toString() };
    if (url.isValid())
        rule.allowedList.add(SecurityOriginData::fromURL(url));
}

static inline void updateList(Document& document, FeaturePolicy::AllowRule& rule, StringView value)
{
    // We keep the empty string value equivalent to '*' for existing websites.
    if (value.isEmpty()) {
        rule.type = FeaturePolicy::AllowRule::Type::All;
        return;
    }

    while (!value.isEmpty()) {
        auto position = value.find(isHTMLSpace<UChar>);
        if (position == notFound) {
            processOriginItem(document, rule, value);
            return;
        }

        processOriginItem(document, rule, value.left(position));
        value = value.substring(position + 1).stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
    }
}

FeaturePolicy FeaturePolicy::parse(Document& document, const HTMLIFrameElement& iframe, StringView allowAttributeValue)
{
    FeaturePolicy policy;
    bool isCameraInitialized = false;
    bool isMicrophoneInitialized = false;
    bool isSpeakerSelectionInitialized = false;
    bool isDisplayCaptureInitialized = false;
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
    for (auto allowItem : allowAttributeValue.split(';')) {
        auto item = allowItem.stripLeadingAndTrailingMatchedCharacters(isHTMLSpace<UChar>);
        if (item.startsWith("camera"_s)) {
            isCameraInitialized = true;
            updateList(document, policy.m_cameraRule, item.substring(7));
            continue;
        }
        if (item.startsWith("microphone"_s)) {
            isMicrophoneInitialized = true;
            updateList(document, policy.m_microphoneRule, item.substring(11));
            continue;
        }
        if (item.startsWith("speaker-selection"_s)) {
            isSpeakerSelectionInitialized = true;
            updateList(document, policy.m_speakerSelectionRule, item.substring(18));
            continue;
        }
        if (item.startsWith("display-capture"_s)) {
            isDisplayCaptureInitialized = true;
            updateList(document, policy.m_displayCaptureRule, item.substring(16));
            continue;
        }
        if (item.startsWith("geolocation"_s)) {
            isGeolocationInitialized = true;
            updateList(document, policy.m_geolocationRule, item.substring(12));
            continue;
        }
        if (item.startsWith("payment"_s)) {
            isPaymentInitialized = true;
            updateList(document, policy.m_paymentRule, item.substring(8));
            continue;
        }
        if (item.startsWith("screen-wake-lock"_s)) {
            isScreenWakeLockInitialized = true;
            updateList(document, policy.m_screenWakeLockRule, item.substring(17));
            continue;
        }
        if (item.startsWith("sync-xhr"_s)) {
            isSyncXHRInitialized = true;
            updateList(document, policy.m_syncXHRRule, item.substring(9));
            continue;
        }
        if (item.startsWith("fullscreen"_s)) {
            isFullscreenInitialized = true;
            updateList(document, policy.m_fullscreenRule, item.substring(11));
            continue;
        }
        if (item.startsWith("web-share"_s)) {
            isWebShareInitialized = true;
            updateList(document, policy.m_webShareRule, item.substring(10));
            continue;
        }
#if ENABLE(DEVICE_ORIENTATION)
        if (item.startsWith("gyroscope"_s)) {
            isGyroscopeInitialized = true;
            updateList(document, policy.m_gyroscopeRule, item.substring(10));
            continue;
        }
        if (item.startsWith("accelerometer"_s)) {
            isAccelerometerInitialized = true;
            updateList(document, policy.m_accelerometerRule, item.substring(14));
            continue;
        }
        if (item.startsWith("magnetometer"_s)) {
            isMagnetometerInitialized = true;
            updateList(document, policy.m_magnetometerRule, item.substring(13));
            continue;
        }
#endif
#if ENABLE(WEB_AUTHN)
        if (item.startsWith("publickey-credentials-get"_s)) {
            isPublickeyCredentialsGetInitialized = true;
            updateList(document, policy.m_publickeyCredentialsGetRule, item.substring(26));
            continue;
        }
#endif
#if ENABLE(WEBXR)
        if (item.startsWith("xr-spatial-tracking"_s)) {
            isXRSpatialTrackingInitialized = true;
            updateList(document, policy.m_xrSpatialTrackingRule, item.substring(19));
            continue;
        }
#endif
    }

    // By default, camera, microphone, speaker-selection, display-capture, fullscreen and xr-spatial-tracking, screen-wake-lock policy is 'self'.
    if (!isCameraInitialized)
        policy.m_cameraRule.allowedList.add(document.securityOrigin().data());
    if (!isMicrophoneInitialized)
        policy.m_microphoneRule.allowedList.add(document.securityOrigin().data());
    if (!isSpeakerSelectionInitialized)
        policy.m_speakerSelectionRule.allowedList.add(document.securityOrigin().data());
    if (!isDisplayCaptureInitialized)
        policy.m_displayCaptureRule.allowedList.add(document.securityOrigin().data());
    if (!isScreenWakeLockInitialized)
        policy.m_screenWakeLockRule.allowedList.add(document.securityOrigin().data());
    if (!isGeolocationInitialized)
        policy.m_geolocationRule.allowedList.add(document.securityOrigin().data());
    if (!isPaymentInitialized)
        policy.m_paymentRule.allowedList.add(document.securityOrigin().data());
    if (!isWebShareInitialized)
        policy.m_webShareRule.type = FeaturePolicy::AllowRule::Type::All;
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

    // https://w3c.github.io/webappsec-feature-policy/#process-feature-policy-attributes
    // 9.5 Process Feature Policy Attributes
    // 3.1 If element’s allowfullscreen attribute is specified, and container policy does
    //     not contain an allowlist for fullscreen,
    if (!isFullscreenInitialized) {
        if (iframe.hasAttribute(allowfullscreenAttr) || iframe.hasAttribute(webkitallowfullscreenAttr)) {
            // 3.1.1 Construct a new declaration for fullscreen, whose allowlist is the special value *.
            policy.m_fullscreenRule.type = FeaturePolicy::AllowRule::Type::All;
        } else {
            // https://fullscreen.spec.whatwg.org/#feature-policy-integration
            // The default allowlist is 'self'.
            policy.m_fullscreenRule.allowedList.add(document.securityOrigin().data());
        }
    }

    if (!isSyncXHRInitialized)
        policy.m_syncXHRRule.type = AllowRule::Type::All;

    return policy;
}

bool FeaturePolicy::allows(Type type, const SecurityOriginData& origin) const
{
    switch (type) {
    case Type::Camera:
        return isAllowedByFeaturePolicy(m_cameraRule, origin);
    case Type::Microphone:
        return isAllowedByFeaturePolicy(m_microphoneRule, origin);
    case Type::SpeakerSelection:
        return isAllowedByFeaturePolicy(m_speakerSelectionRule, origin);
    case Type::DisplayCapture:
        return isAllowedByFeaturePolicy(m_displayCaptureRule, origin);
    case Type::Geolocation:
        return isAllowedByFeaturePolicy(m_geolocationRule, origin);
    case Type::Payment:
        return isAllowedByFeaturePolicy(m_paymentRule, origin);
    case Type::ScreenWakeLock:
        return isAllowedByFeaturePolicy(m_screenWakeLockRule, origin);
    case Type::SyncXHR:
        return isAllowedByFeaturePolicy(m_syncXHRRule, origin);
    case Type::Fullscreen:
        return isAllowedByFeaturePolicy(m_fullscreenRule, origin);
    case Type::WebShare:
        return isAllowedByFeaturePolicy(m_webShareRule, origin);
#if ENABLE(DEVICE_ORIENTATION)
    case Type::Gyroscope:
        return isAllowedByFeaturePolicy(m_gyroscopeRule, origin);
    case Type::Accelerometer:
        return isAllowedByFeaturePolicy(m_accelerometerRule, origin);
    case Type::Magnetometer:
        return isAllowedByFeaturePolicy(m_magnetometerRule, origin);
#endif
#if ENABLE(WEB_AUTHN)
    case Type::PublickeyCredentialsGetRule:
        return isAllowedByFeaturePolicy(m_publickeyCredentialsGetRule, origin);
#endif
#if ENABLE(WEBXR)
    case Type::XRSpatialTracking:
        return isAllowedByFeaturePolicy(m_xrSpatialTrackingRule, origin);
#endif
    }
    ASSERT_NOT_REACHED();
    return false;
}

}
