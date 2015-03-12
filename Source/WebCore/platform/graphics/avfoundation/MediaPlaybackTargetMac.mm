/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaPlaybackTarget.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

#import <AVFoundation/AVOutputDevicePickerContext.h>
#import <WebCore/MediaPlaybackTarget.h>
#import <WebCore/SoftLinking.h>
#import <objc/runtime.h>

typedef AVOutputDevicePickerContext AVOutputDevicePickerContextType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVOutputDevicePickerContext)

namespace WebCore {

static NSString * const deviceContextKey = @"deviceContext";

void MediaPlaybackTarget::encode(NSKeyedArchiver *archiver) const
{
    if ([getAVOutputDevicePickerContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
        [archiver encodeObject:m_devicePickerContext.get() forKey:deviceContextKey];
}

bool MediaPlaybackTarget::decode(NSKeyedUnarchiver *unarchiver, MediaPlaybackTarget& playbackTarget)
{
    if (![getAVOutputDevicePickerContextClass() conformsToProtocol:@protocol(NSSecureCoding)])
        return false;

    AVOutputDevicePickerContext *context = nil;

    @try {
        context = [unarchiver decodeObjectOfClass:getAVOutputDevicePickerContextClass() forKey:deviceContextKey];
    } @catch (NSException *exception) {
        LOG_ERROR("The target picker being decoded is not a AVOutputDevicePickerContext.");
    }

    playbackTarget.m_devicePickerContext = context;

    return context;
}

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)
