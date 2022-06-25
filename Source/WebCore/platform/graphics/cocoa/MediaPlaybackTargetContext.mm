/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "MediaPlaybackTargetContext.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET)

#import <Foundation/Foundation.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {

MediaPlaybackTargetContext::MediaPlaybackTargetContext(RetainPtr<AVOutputContext>&& outputContext)
    : m_outputContext(outputContext)
{
    ASSERT(m_outputContext);
    m_type = MediaPlaybackTargetContext::Type::AVOutputContext;
}

String MediaPlaybackTargetContext::deviceName() const
{
    ASSERT(m_type == MediaPlaybackTargetContext::Type::Mock || m_type == MediaPlaybackTargetContext::Type::AVOutputContext);

    if (m_type == MediaPlaybackTargetContext::Type::Mock)
        return m_mockDeviceName;

    ASSERT(m_type == MediaPlaybackTargetContext::Type::AVOutputContext);
    String deviceName;
    if (![m_outputContext supportsMultipleOutputDevices])
        deviceName = [m_outputContext deviceName];
    else {
        auto outputDeviceNames = adoptNS([[NSMutableArray alloc] init]);
        for (AVOutputDevice *outputDevice in [m_outputContext outputDevices])
            [outputDeviceNames addObject:[outputDevice deviceName]];

        deviceName = [outputDeviceNames componentsJoinedByString:@" + "];
    }

    return deviceName;
}
bool MediaPlaybackTargetContext::hasActiveRoute() const
{
    ASSERT(m_type != MediaPlaybackTargetContext::Type::None);
    if (m_type == MediaPlaybackTargetContext::Type::Mock)
        return !m_mockDeviceName.isEmpty();

    // FIXME: It may not be safe to use the cached `hasActiveRoute` of a serialized AVOutputContext
    if (m_type == MediaPlaybackTargetContext::Type::SerializedAVOutputContext)
        return m_cachedHasActiveRoute;

    ASSERT(m_type == MediaPlaybackTargetContext::Type::AVOutputContext);
    bool hasActiveRoute = false;
    if ([m_outputContext respondsToSelector:@selector(supportsMultipleOutputDevices)] && [m_outputContext supportsMultipleOutputDevices] && [m_outputContext respondsToSelector:@selector(outputDevices)]) {
        for (AVOutputDevice *outputDevice in [m_outputContext outputDevices]) {
            if (outputDevice.deviceFeatures & (AVOutputDeviceFeatureVideo | AVOutputDeviceFeatureAudio))
                hasActiveRoute = true;
        }
    } else if ([m_outputContext respondsToSelector:@selector(outputDevice)]) {
        if (auto *outputDevice = [m_outputContext outputDevice])
            hasActiveRoute = outputDevice.deviceFeatures & (AVOutputDeviceFeatureVideo | AVOutputDeviceFeatureAudio);
    } else
        hasActiveRoute = m_outputContext.get().deviceName;

    return hasActiveRoute;
}
bool MediaPlaybackTargetContext::supportsRemoteVideoPlayback() const
{
    ASSERT(m_type == MediaPlaybackTargetContext::Type::Mock || m_type == MediaPlaybackTargetContext::Type::AVOutputContext);
    if (m_type == MediaPlaybackTargetContext::Type::Mock)
        return !m_mockDeviceName.isEmpty();

    bool supportsRemoteVideoPlayback = false;
    if (![m_outputContext respondsToSelector:@selector(supportsMultipleOutputDevices)] || ![m_outputContext supportsMultipleOutputDevices] || ![m_outputContext respondsToSelector:@selector(outputDevices)]) {
        if (auto *outputDevice = [m_outputContext outputDevice]) {
            if (outputDevice.deviceFeatures & AVOutputDeviceFeatureVideo)
                supportsRemoteVideoPlayback = true;
        }
    } else {
        for (AVOutputDevice *outputDevice in [m_outputContext outputDevices]) {
            if (outputDevice.deviceFeatures & AVOutputDeviceFeatureVideo)
                supportsRemoteVideoPlayback = true;
        }
    }

    return supportsRemoteVideoPlayback;
}

bool MediaPlaybackTargetContext::serializeOutputContext()
{
    ASSERT(m_type != MediaPlaybackTargetContext::Type::SerializedAVOutputContext);
    if (m_type != MediaPlaybackTargetContext::Type::AVOutputContext)
        return false;

    auto archiver = adoptNS([[NSKeyedArchiver alloc] initRequiringSecureCoding:YES]);
    [archiver encodeObject:m_outputContext.get() forKey:NSKeyedArchiveRootObjectKey];
    [archiver finishEncoding];

    m_serializedOutputContext = [archiver encodedData];
    m_type = MediaPlaybackTargetContext::Type::SerializedAVOutputContext;
    m_outputContext.clear();

    return true;
}

bool MediaPlaybackTargetContext::deserializeOutputContext()
{
    ASSERT(m_type != MediaPlaybackTargetContext::Type::AVOutputContext);
    if (m_type != MediaPlaybackTargetContext::Type::SerializedAVOutputContext)
        return false;

    bool success = false;
    auto unarchiver = adoptNS([[NSKeyedUnarchiver alloc] initForReadingFromData:m_serializedOutputContext.get() error:nullptr]);
    unarchiver.get().decodingFailurePolicy = NSDecodingFailurePolicyRaiseException;
    auto allowedClassSet = adoptNS([[NSSet alloc] initWithArray:@[ PAL::getAVOutputContextClass() ]]);

    @try {
        id result = [unarchiver decodeObjectOfClasses:allowedClassSet.get() forKey:NSKeyedArchiveRootObjectKey];
        ASSERT(result);
        if (result) {
            m_outputContext = result;
            m_type = MediaPlaybackTargetContext::Type::AVOutputContext;
            m_serializedOutputContext.clear();
            success = true;
        }
    } @catch (NSException *exception) {
        LOG_ERROR("Failed to decode object of class AVOutputContext : %@", exception);
    } @finally {
        [unarchiver finishDecoding];
        unarchiver.get().delegate = nil;
    }

    return success;
}

}

#endif
