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

#import "config.h"
#import "SpatialAudioPlaybackHelper.h"

#if PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import "PlatformMediaConfiguration.h"
#import "WebCoreThreadRun.h"
#import <AVFoundation/AVAudioSession.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <wtf/BlockObjCExceptions.h>
#import <wtf/CheckedPtr.h>
#import <wtf/NeverDestroyed.h>
#import <wtf/TZoneMalloc.h>
#import <wtf/WeakPtr.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

namespace WebCore {
class SpatialAudioPlaybackHelperIOS;
}

using namespace WebCore;

@interface WebSpatialAudioPlaybackObserver : NSObject {
    WeakPtr<WebCore::SpatialAudioPlaybackHelperIOS> _callback;
}
- (id)initWithCallback:(WebCore::SpatialAudioPlaybackHelperIOS&)callback;
- (void)spatialPlaybackCapabilitiesChanged:(NSNotification *)notification;
- (void)shutdown;
@end

namespace WebCore {

class SpatialAudioPlaybackHelperIOS final
    : public CanMakeWeakPtr<SpatialAudioPlaybackHelperIOS>
    , public CanMakeCheckedPtr<SpatialAudioPlaybackHelperIOS> {
    WTF_MAKE_TZONE_ALLOCATED(SpatialAudioPlaybackHelperIOS);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(SpatialAudioPlaybackHelperIOS);
public:
    static SpatialAudioPlaybackHelperIOS& singleton();

    SpatialAudioPlaybackHelperIOS();
    ~SpatialAudioPlaybackHelperIOS();

    void updateActiveAudioRouteSupportsSpatialPlayback();
    bool activeAudioRouteSupportsSpatialPlayback() const { return m_activeAudioRouteSupportsSpatialPlayback; }

private:
    RetainPtr<WebSpatialAudioPlaybackObserver> m_observer;
    bool m_activeAudioRouteSupportsSpatialPlayback { false };
};

WTF_MAKE_TZONE_ALLOCATED_IMPL(SpatialAudioPlaybackHelperIOS);

SpatialAudioPlaybackHelperIOS& SpatialAudioPlaybackHelperIOS::singleton()
{
    static const NeverDestroyed<std::unique_ptr<SpatialAudioPlaybackHelperIOS>> instance = makeUnique<SpatialAudioPlaybackHelperIOS>();

    return *instance.get();
}

SpatialAudioPlaybackHelperIOS::SpatialAudioPlaybackHelperIOS()
{
    m_observer = adoptNS([[WebSpatialAudioPlaybackObserver alloc] initWithCallback:*this]);

    // Initialize with current state
    updateActiveAudioRouteSupportsSpatialPlayback();
}

SpatialAudioPlaybackHelperIOS::~SpatialAudioPlaybackHelperIOS()
{
    [m_observer shutdown];
    m_observer = nil;
}

void SpatialAudioPlaybackHelperIOS::updateActiveAudioRouteSupportsSpatialPlayback()
{
    AVAudioSession* audioSession = [PAL::getAVAudioSessionClassSingleton() sharedInstance];
    for (AVAudioSessionPortDescription* output in audioSession.currentRoute.outputs) {
        if (output.spatialAudioEnabled) {
            m_activeAudioRouteSupportsSpatialPlayback = true;
            return;
        }
    }

    m_activeAudioRouteSupportsSpatialPlayback = false;
}

bool SpatialAudioPlaybackHelper::supportsSpatialAudioPlaybackForConfiguration(const PlatformMediaConfiguration& configuration)
{
    ASSERT(configuration.audio);

    // Only multichannel audio can be spatially rendered on iOS.
    if (!configuration.audio || configuration.audio->channels.toDouble() <= 2)
        return false;

    return SpatialAudioPlaybackHelperIOS::singleton().activeAudioRouteSupportsSpatialPlayback();
}

} // namespace WebCore

@implementation WebSpatialAudioPlaybackObserver

- (id)initWithCallback:(WebCore::SpatialAudioPlaybackHelperIOS&)callback
{
    assertIsMainThread();
    LOG(Media, "-[WebSpatialAudioPlaybackObserver initWithCallback]");

    if (!(self = [super init]))
        return nil;

    _callback = &callback;

    RetainPtr center = [NSNotificationCenter defaultCenter];
    [center addObserver:self selector:@selector(spatialPlaybackCapabilitiesChanged:) name:PAL::get_AVFoundation_AVAudioSessionSpatialPlaybackCapabilitiesChangedNotificationSingleton() object:nil];

    return self;
}

- (void)shutdown
{
    LOG(Media, "-[WebSpatialAudioPlaybackObserver shutdown]");
    assertIsMainThread();
    RetainPtr center = [NSNotificationCenter defaultCenter];
    [center removeObserver:self];
    _callback = nil;
}

- (void)dealloc
{
    LOG(Media, "-[WebSpatialAudioPlaybackObserver dealloc]");
    [super dealloc];
}

- (void)spatialPlaybackCapabilitiesChanged:(NSNotification *)notification
{
    UNUSED_PARAM(notification);
    LOG(Media, "-[WebSpatialAudioPlaybackObserver spatialPlaybackCapabilitiesChanged:]");
    callOnWebThreadOrDispatchAsyncOnMainThread([protectedSelf = retainPtr(self)]() {
        if (CheckedPtr callback = protectedSelf->_callback.get())
            callback->updateActiveAudioRouteSupportsSpatialPlayback();
    });
}

@end

#endif // PLATFORM(IOS_FAMILY)
