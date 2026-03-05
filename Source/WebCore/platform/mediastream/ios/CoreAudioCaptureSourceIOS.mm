/*
 * Copyright (C) 2017-2022 Apple Inc. All rights reserved.
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
#import "CoreAudioCaptureSourceIOS.h"

#if ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)

#import "AVAudioSessionCaptureDeviceManager.h"
#import "Logging.h"
#import <AVFoundation/AVAudioSession.h>
#import <wtf/MainThread.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

using namespace WebCore;

@interface WebCoreAudioCaptureSourceIOSListener : NSObject {
    CoreAudioCaptureSourceFactoryIOS* _callback;
}

- (void)invalidate;
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
    AVAudioSession* session = [PAL::getAVAudioSessionClassSingleton() sharedInstance];

    [center addObserver:self selector:@selector(sessionMediaServicesWereReset:) name:AVAudioSessionMediaServicesWereResetNotification object:session];

    return self;
}

- (void)invalidate
{
    _callback = nullptr;
    [[NSNotificationCenter defaultCenter] removeObserver:self];
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

Ref<CoreAudioCaptureSourceFactoryIOS> CoreAudioCaptureSourceFactoryIOS::create()
{
    return adoptRef(*new CoreAudioCaptureSourceFactoryIOS);
}

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
    static NeverDestroyed<Ref<CoreAudioCaptureSourceFactoryIOS>> factory = CoreAudioCaptureSourceFactoryIOS::create();
    return factory.get();
}

}

#endif // ENABLE(MEDIA_STREAM) && PLATFORM(IOS_FAMILY)
