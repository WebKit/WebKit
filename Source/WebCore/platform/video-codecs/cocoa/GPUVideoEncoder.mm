/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "GPUVideoEncoder.h"

#import "GPUVideoEncoderVTBH264.h"
#import "GPUVideoEncoderVTBH265.h"
#import <wtf/BlockPtr.h>
#import <wtf/StdLibExtras.h>
#import <wtf/TZoneMallocInlines.h>

#if USE(LIBWEBRTC)
#include "LibWebRTCMacros.h"
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_BEGIN
#include <webrtc/webkit_sdk/WebKit/WebKitEncoder.h>
WTF_IGNORE_WARNINGS_IN_THIRD_PARTY_CODE_END
#endif

namespace WebCore {

RefPtr<GPUVideoEncoder> GPUVideoEncoder::create(CreationInfo&& creationInfo, const Vector<std::pair<String, String>>& parameters, GPUVideoEncoderCallback&& callback, GPUVideoEncoderDescriptionCallback&& descriptionCallback, GPUVideoEncoderErrorCallback&& errorCallback)
{
#if USE(AVFOUNDATION)
    if (creationInfo.codecType == VideoCodecType::H265) {
        // We only support L1T1 for H265.
        creationInfo.scalabilityMode = VideoEncoderScalabilityMode::L1T1;
        return GPUVideoEncoderVTBH265::create(WTF::move(creationInfo), WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback));
    }

    if (creationInfo.codecType == VideoCodecType::H264) {
        if (creationInfo.scalabilityMode == VideoEncoderScalabilityMode::L1T3)
            return nullptr;
        return GPUVideoEncoderVTBH264::create(WTF::move(creationInfo), parameters, WTF::move(callback), WTF::move(descriptionCallback), WTF::move(errorCallback));
    }
    return nullptr;
#else
    UNUSED_PARAM(creationInfo);
    UNUSED_PARAM(parameters);
    UNUSED_PARAM(callback);
    UNUSED_PARAM(descriptionCallback);
    UNUSED_PARAM(errorCallback);
    return nullptr;
#endif
}

}
