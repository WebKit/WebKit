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
#include "H264VideoToolBoxEncoder.h"

#include "Logging.h"

#if USE(LIBWEBRTC) && PLATFORM(COCOA)

namespace WebCore {

static bool isHardwareEncoderForWebRTCAllowed = true;

void H264VideoToolboxEncoder::setHardwareEncoderForWebRTCAllowed(bool allowed)
{
    isHardwareEncoderForWebRTCAllowed = allowed;
}

bool H264VideoToolboxEncoder::hardwareEncoderForWebRTCAllowed()
{
    return isHardwareEncoderForWebRTCAllowed;
}

#if PLATFORM(MAC) && ENABLE(MAC_VIDEO_TOOLBOX)
static inline bool isStandardFrameSize(int32_t width, int32_t height)
{
    // FIXME: Envision relaxing this rule, something like width and height dividable by 4 or 8 should be good enough.
    if (width == 1280)
        return height == 720;
    if (width == 720)
        return height == 1280;
    if (width == 960)
        return height == 540;
    if (width == 540)
        return height == 960;
    if (width == 640)
        return height == 480;
    if (width == 480)
        return height == 640;
    if (width == 288)
        return height == 352;
    if (width == 352)
        return height == 288;
    if (width == 320)
        return height == 240;
    if (width == 240)
        return height == 320;
    return false;
}
#endif

}

#if ENABLE(MAC_VIDEO_TOOLBOX) && USE(APPLE_INTERNAL_SDK) && __has_include(<WebKitAdditions/VideoToolBoxEncoderMac.mm>)
#import <WebKitAdditions/VideoToolBoxEncoderMac.mm>
#else

namespace WebCore {

#if PLATFORM(MAC) && ENABLE(MAC_VIDEO_TOOLBOX)
static inline bool isUsingSoftwareEncoder(VTCompressionSessionRef& compressionSession)
{
    CFNumberRef useHardwareEncoderValue = nullptr;
    OSStatus statusGetter = VTSessionCopyProperty(compressionSession, kVTCompressionPropertyKey_UsingHardwareAcceleratedVideoEncoder, nullptr, &useHardwareEncoderValue);
    if (statusGetter || !useHardwareEncoderValue)
        return true;

    int useHardwareEncoder = 0;
    CFNumberGetValue(useHardwareEncoderValue, kCFNumberIntType, &useHardwareEncoder);
    CFRelease(useHardwareEncoderValue);

    return useHardwareEncoder;
}
#endif

int H264VideoToolboxEncoder::CreateCompressionSession(VTCompressionSessionRef& compressionSession, VTCompressionOutputCallback outputCallback, int32_t width, int32_t height, bool useHardwareAcceleratedVideoEncoder)
{
    int result = webrtc::H264VideoToolboxEncoder::CreateCompressionSession(compressionSession, outputCallback, width, height, hardwareEncoderForWebRTCAllowed() ? useHardwareAcceleratedVideoEncoder : false);
    if (result)
        return result;

#if PLATFORM(MAC) && ENABLE(MAC_VIDEO_TOOLBOX)
    if (!isUsingSoftwareEncoder(compressionSession))
        return 0;

    if (!isStandardFrameSize(width, height)) {
        RELEASE_LOG(WebRTC, "Using H264 software encoder with non standard size is not supported");
        DestroyCompressionSession();
        return -1;
    }
    RELEASE_LOG(WebRTC, "Using H264 software encoder");
#endif
    return 0;
}
    
}
#endif

#endif
