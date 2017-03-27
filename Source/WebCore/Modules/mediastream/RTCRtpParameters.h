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

#pragma once

#if ENABLE(WEB_RTC)

#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

struct RTCRtpParameters {

    struct FecParameters {
        unsigned long ssrc;
    };

    struct RtxParameters {
        unsigned long ssrc;
    };

    enum class PriorityType { VeryLow, Low, Medium, High };
    enum class DegradationPreference { MaintainFramerate, MaintainResolution, Balanced };
    enum class DtxStatus { Disabled, Enabled };

    struct CodecParameters {
        unsigned short payloadType { 0 };
        String mimeType;
        unsigned long clockRate { 0 };
        unsigned short channels = 1;
    };

    struct EncodingParameters {
        unsigned long ssrc { 0 };
        RtxParameters rtx;
        FecParameters fec;
        DtxStatus dtx { DtxStatus::Disabled };
        bool active { false};
        PriorityType priority { PriorityType::Medium };
        unsigned long maxBitrate { 0 };
        unsigned long maxFramerate { 0 };
        String rid;
        double scaleResolutionDownBy { 1 };
    };

    struct HeaderExtensionParameters {
        String uri;
        unsigned short id;
    };

    String transactionId;
    Vector<EncodingParameters> encodings;
    Vector<HeaderExtensionParameters> headerExtensions;
    Vector<CodecParameters> codecs;
    DegradationPreference degradationPreference = DegradationPreference::Balanced;
};


} // namespace WebCore

#endif // ENABLE(WEB_RTC)
