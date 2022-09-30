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

#pragma once

#include "SecurityOriginData.h"
#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace WebCore {

class Document;
class HTMLIFrameElement;

class FeaturePolicy {
public:
    static FeaturePolicy parse(Document&, const HTMLIFrameElement&, StringView);

    enum class Type {
        Camera,
        Microphone,
        SpeakerSelection,
        DisplayCapture,
        Geolocation,
        Payment,
        ScreenWakeLock,
        SyncXHR,
        Fullscreen,
        WebShare,
#if ENABLE(DEVICE_ORIENTATION)
        Gyroscope,
        Accelerometer,
        Magnetometer,
#endif
#if ENABLE(WEB_AUTHN)
        PublickeyCredentialsGetRule,
#endif
#if ENABLE(WEBXR)
        XRSpatialTracking,
#endif
    };
    bool allows(Type, const SecurityOriginData&) const;

    struct AllowRule {
        enum class Type { All, None, List };
        Type type { Type::List };
        HashSet<SecurityOriginData> allowedList;
    };

private:
    AllowRule m_cameraRule;
    AllowRule m_microphoneRule;
    AllowRule m_speakerSelectionRule;
    AllowRule m_displayCaptureRule;
    AllowRule m_geolocationRule;
    AllowRule m_paymentRule;
    AllowRule m_syncXHRRule;
    AllowRule m_fullscreenRule;
    AllowRule m_webShareRule;
    AllowRule m_screenWakeLockRule;

#if ENABLE(DEVICE_ORIENTATION)
    AllowRule m_gyroscopeRule;
    AllowRule m_accelerometerRule;
    AllowRule m_magnetometerRule;
#endif
#if ENABLE(WEB_AUTHN)
    AllowRule m_publickeyCredentialsGetRule;
#endif
#if ENABLE(WEBXR)
    AllowRule m_xrSpatialTrackingRule;
#endif
};

enum class LogFeaturePolicyFailure { No, Yes };
extern bool isFeaturePolicyAllowedByDocumentAndAllOwners(FeaturePolicy::Type, const Document&, LogFeaturePolicyFailure = LogFeaturePolicyFailure::Yes);

} // namespace WebCore
