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
#include "CoreAudioCaptureSourceIOS.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import <AVFoundation/AVAudioSession.h>
#import <wtf/MainThread.h>
#import <wtf/SoftLinking.h>

typedef AVAudioSession AVAudioSessionType;

SOFT_LINK_FRAMEWORK(AVFoundation)
SOFT_LINK_CLASS(AVFoundation, AVAudioSession)

SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionInterruptionNotification, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionInterruptionTypeKey, NSString *)
SOFT_LINK_CONSTANT(AVFoundation, AVAudioSessionMediaServicesWereResetNotification, NSString *)

#define AVAudioSession getAVAudioSessionClass()
#define AVAudioSessionInterruptionNotification getAVAudioSessionInterruptionNotification()
#define AVAudioSessionInterruptionTypeKey getAVAudioSessionInterruptionTypeKey()
#define AVAudioSessionMediaServicesWereResetNotification getAVAudioSessionMediaServicesWereResetNotification()

using namespace WebCore;

@interface WebCoreAudioCaptureSourceIOSListener : NSObject {
    CoreAudioCaptureSourceFactoryIOS* _callback;
}

- (void)invalidate;
- (void)handleInterruption:(NSNotification*)notification;
- (void)sessionMediaServicesWereReset:(NSNotification*)notification;
@end

@implementation WebCoreAudioCaptureSourceIOSListener
- (id)initWithCallback:(CoreAudioCaptureSourceFactoryIOS*)callback
{
    self = [super init];
    if (!self)
        return nil;

    _callback = callback;

    NSNotificationCenter* center = [NSNotificationCenter defaultCenter];
    AVAudioSessionType* session = [AVAudioSession sharedInstance];

    [center addObserver:self selector:@selector(handleInterruption:) name:AVAudioSessionInterruptionNotification object:session];
    [center addObserver:self selector:@selector(sessionMediaServicesWereReset:) name:AVAudioSessionMediaServicesWereResetNotification object:session];

    return self;
}

- (void)invalidate
{
    _callback = nullptr;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
}

- (void)handleInterruption:(NSNotification*)notification
{
    ASSERT(_callback);
    if (!_callback)
        return;

    if ([[notification.userInfo valueForKey:AVAudioSessionInterruptionTypeKey] intValue] == AVAudioSessionInterruptionTypeBegan) {
        _callback->beginInterruption();
        return;
    }

    if ([[notification.userInfo valueForKey:AVAudioSessionInterruptionTypeKey] intValue] == AVAudioSessionInterruptionTypeEnded) {
        NSError *error = nil;
        [[AVAudioSession sharedInstance] setActive:YES error:&error];

#if !LOG_DISABLED
        if (error)
            LOG(Media, "-[WebCoreAudioCaptureSourceIOSListener handleInterruption] (%p) - error = %s", self, [[error localizedDescription] UTF8String]);
#endif

        _callback->endInterruption();
    }
}

- (void)sessionMediaServicesWereReset:(NSNotification*)notification
{
    UNUSED_PARAM(notification);
    ASSERT(_callback);

    if (!_callback)
        return;

    // mediaserverd crashed and was relaunched, rebuild everything. See
    // https://developer.apple.com/library/content/qa/qa1749/_index.html.
    _callback->scheduleReconfiguration();
}
@end

namespace WebCore {

CoreAudioCaptureSourceFactoryIOS::CoreAudioCaptureSourceFactoryIOS()
    : m_listener(adoptNS([[WebCoreAudioCaptureSourceIOSListener alloc] initWithCallback:this]))
{
}

CoreAudioCaptureSourceFactoryIOS::~CoreAudioCaptureSourceFactoryIOS()
{
    [m_listener invalidate];
    m_listener = nullptr;
}

CoreAudioCaptureSourceFactory& CoreAudioCaptureSourceFactory::singleton()
{
    static NeverDestroyed<CoreAudioCaptureSourceFactoryIOS> factory;
    return factory.get();
}

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
