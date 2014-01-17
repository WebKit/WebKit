/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#import "config.h"
#import "MediaSessionManagerIOS.h"

#if PLATFORM(IOS)

#import "Logging.h"
#import "MediaSession.h"
#import "SoftLinking.h"
#import "WebCoreThreadRun.h"
#import "WebCoreSystemInterface.h"
#import <AVFoundation/AVAudioSession.h>
#import <objc/runtime.h>
#import <wtf/RetainPtr.h>

SOFT_LINK_FRAMEWORK(AVFoundation)

SOFT_LINK_CLASS(AVFoundation, AVAudioSession)

SOFT_LINK_POINTER(AVFoundation, AVAudioSessionInterruptionNotification, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVAudioSessionInterruptionTypeKey, NSString *)
SOFT_LINK_POINTER(AVFoundation, AVAudioSessionInterruptionOptionKey, NSString *)

#define AVAudioSession getAVAudioSessionClass()
#define AVAudioSessionInterruptionNotification getAVAudioSessionInterruptionNotification()
#define AVAudioSessionInterruptionTypeKey getAVAudioSessionInterruptionTypeKey()
#define AVAudioSessionInterruptionOptionKey getAVAudioSessionInterruptionOptionKey()

using namespace WebCore;

@interface WebMediaSessionHelper : NSObject {
    MediaSessionManageriOS* _callback;
}

- (id)initWithCallback:(MediaSessionManageriOS*)callback;
- (void)interruption:(NSNotification*)notification;
@end


namespace WebCore {

MediaSessionManager& MediaSessionManager::sharedManager()
{
    DEFINE_STATIC_LOCAL(MediaSessionManageriOS, manager, ());
    return manager;
}

MediaSessionManageriOS::MediaSessionManageriOS()
    :MediaSessionManager()
    , m_objcObserver(adoptNS([[WebMediaSessionHelper alloc] initWithCallback:this]))
{
    DEFINE_STATIC_LOCAL(wkDeviceClass, deviceClass, (iosDeviceClass()));

    if (deviceClass == wkDeviceClassiPhone || deviceClass == wkDeviceClassiPod)
        addRestriction(MediaSession::Video, InlineVideoPlaybackRestricted);

    addRestriction(MediaSession::Video, ConcurrentPlaybackNotPermitted);
}

}

@implementation WebMediaSessionHelper

- (id)initWithCallback:(MediaSessionManageriOS*)callback
{
    if (!(self = [super init]))
        return nil;
    
    _callback = callback;

    NSNotificationCenter *center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(interruption:) name:AVAudioSessionInterruptionNotification object:[AVAudioSession sharedInstance]];
    
    return self;
}

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];
    [super dealloc];
}

- (void)interruption:(NSNotification *)notification
{
    if (!_callback)
        return;

    NSUInteger type = [[[notification userInfo] objectForKey:AVAudioSessionInterruptionTypeKey] unsignedIntegerValue];
    MediaSession::EndInterruptionFlags flags = MediaSession::NoFlags;

    if (type == AVAudioSessionInterruptionTypeEnded && [[[notification userInfo] objectForKey:AVAudioSessionInterruptionOptionKey] unsignedIntegerValue] == AVAudioSessionInterruptionOptionShouldResume)
        flags = MediaSession::MayResumePlaying;

    WebThreadRun(^{
        if (!_callback)
            return;

        if (type == AVAudioSessionInterruptionTypeBegan)
            _callback->beginInterruption();
        else
            _callback->endInterruption(flags);

    });
}

@end

#endif // PLATFORM(IOS)
