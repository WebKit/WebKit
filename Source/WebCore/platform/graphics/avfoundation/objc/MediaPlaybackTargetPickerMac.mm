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
#import "MediaPlaybackTargetPickerMac.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

#import "Logging.h"
#import <WebCore/AVFoundationSPI.h>
#import <WebCore/AVKitSPI.h>
#import <WebCore/FloatRect.h>
#import <WebCore/MediaPlaybackTargetMac.h>
#import <WebCore/SoftLinking.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>

typedef AVOutputContext AVOutputContextType;
typedef AVOutputDeviceMenuController AVOutputDeviceMenuControllerType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)

SOFT_LINK_CLASS(AVFoundation, AVOutputContext)
SOFT_LINK_CLASS(AVKit, AVOutputDeviceMenuController)

using namespace WebCore;

static NSString *externalOutputDeviceAvailableKeyName = @"externalOutputDeviceAvailable";
static NSString *externalOutputDevicePickedKeyName = @"externalOutputDevicePicked";

// FIXME: remove this once the headers are available.
@interface AVOutputDeviceMenuController (ForwardDeclaration)
- (BOOL)showMenuForRect:(NSRect)screenRect appearanceName:(NSString *)appearanceName allowReselectionOfSelectedOutputDevice:(BOOL)allowReselectionOfSelectedOutputDevice;
@end

@interface WebAVOutputDeviceMenuControllerHelper : NSObject {
    MediaPlaybackTargetPickerMac* m_callback;
}

- (instancetype)initWithCallback:(MediaPlaybackTargetPickerMac*)callback;
- (void)clearCallback;
- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
@end

namespace WebCore {

static const double pendingActionInterval = 1.0 / 10.0;

std::unique_ptr<MediaPlaybackTargetPickerMac> MediaPlaybackTargetPickerMac::create(MediaPlaybackTargetPicker::Client& client)
{
    return std::unique_ptr<MediaPlaybackTargetPickerMac>(new MediaPlaybackTargetPickerMac(client));
}

MediaPlaybackTargetPickerMac::MediaPlaybackTargetPickerMac(MediaPlaybackTargetPicker::Client& client)
    : MediaPlaybackTargetPicker(client)
    , m_outputDeviceMenuControllerDelegate(adoptNS([[WebAVOutputDeviceMenuControllerHelper alloc] initWithCallback:this]))
    , m_pendingActionTimer(RunLoop::main(), this, &MediaPlaybackTargetPickerMac::pendingActionTimerFired)
{
}

MediaPlaybackTargetPickerMac::~MediaPlaybackTargetPickerMac()
{
    m_client = nullptr;
    m_pendingActionTimer.stop();
    [m_outputDeviceMenuControllerDelegate clearCallback];

    stopMonitoringPlaybackTargets();
}

void MediaPlaybackTargetPickerMac::pendingActionTimerFired()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::pendingActionTimerFired - flags = 0x%x", m_pendingActionFlags);

    if (!m_client)
        return;

    PendingActionFlags pendingActions = m_pendingActionFlags;
    m_pendingActionFlags = 0;

    if (pendingActions & CurrentDeviceDidChange) {
        AVOutputContext* context = m_outputDeviceMenuController ? [m_outputDeviceMenuController.get() outputContext] : nullptr;
        m_client->setPlaybackTarget(WebCore::MediaPlaybackTargetMac::create(context));
    }

    if (pendingActions & OutputDeviceAvailabilityChanged)
        m_client->externalOutputDeviceAvailableDidChange(devicePicker().externalOutputDeviceAvailable);

}

void MediaPlaybackTargetPickerMac::availableDevicesDidChange()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::availableDevicesDidChange - available = %i", (int)devicePicker().externalOutputDeviceAvailable);

    if (!m_client)
        return;

    addPendingAction(OutputDeviceAvailabilityChanged);
}

AVOutputDeviceMenuControllerType *MediaPlaybackTargetPickerMac::devicePicker()
{
    if (!getAVOutputDeviceMenuControllerClass())
        return nullptr;

    if (!m_outputDeviceMenuController) {
        LOG(Media, "MediaPlaybackTargetPickerMac::devicePicker - allocating picker");

        RetainPtr<AVOutputContextType> context = adoptNS([[getAVOutputContextClass() alloc] init]);
        m_outputDeviceMenuController = adoptNS([[getAVOutputDeviceMenuControllerClass() alloc] initWithOutputContext:context.get()]);

        [m_outputDeviceMenuController.get() addObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDeviceAvailableKeyName options:NSKeyValueObservingOptionNew context:nullptr];
        [m_outputDeviceMenuController.get() addObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDevicePickedKeyName options:NSKeyValueObservingOptionNew context:nullptr];

        LOG(Media, "MediaPlaybackTargetPickerMac::devicePicker - allocated menu controller %p", m_outputDeviceMenuController.get());

        if (m_outputDeviceMenuController.get().externalOutputDeviceAvailable)
            availableDevicesDidChange();
    }

    return m_outputDeviceMenuController.get();
}

void MediaPlaybackTargetPickerMac::showPlaybackTargetPicker(const FloatRect& location, bool checkActiveRoute)
{
    if (!m_client || m_showingMenu)
        return;

    LOG(Media, "MediaPlaybackTargetPickerMac::showPlaybackTargetPicker - checkActiveRoute = %i", (int)checkActiveRoute);

    AVOutputDeviceMenuControllerType *picker = devicePicker();
    if (![picker respondsToSelector:@selector(showMenuForRect:appearanceName:allowReselectionOfSelectedOutputDevice:)])
        return;

    m_showingMenu = true;
    if ([picker showMenuForRect:location appearanceName:NSAppearanceNameVibrantLight allowReselectionOfSelectedOutputDevice:!checkActiveRoute]) {
        if (!checkActiveRoute)
            currentDeviceDidChange();
    }
    m_showingMenu = false;
}

void MediaPlaybackTargetPickerMac::addPendingAction(PendingActionFlags action)
{
    m_pendingActionFlags |= action;
    m_pendingActionTimer.startOneShot(pendingActionInterval);
}

void MediaPlaybackTargetPickerMac::currentDeviceDidChange()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::currentDeviceDidChange");

    if (!m_client)
        return;

    addPendingAction(CurrentDeviceDidChange);
}

void MediaPlaybackTargetPickerMac::startingMonitoringPlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::startingMonitoringPlaybackTargets");

    devicePicker();
}

void MediaPlaybackTargetPickerMac::stopMonitoringPlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::stopMonitoringPlaybackTargets");
    // Nothing to do, AirPlay takes care of this automatically.
}

void MediaPlaybackTargetPickerMac::invalidatePlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::invalidatePlaybackTargets");

    if (m_outputDeviceMenuController) {
        [m_outputDeviceMenuController removeObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDeviceAvailableKeyName];
        [m_outputDeviceMenuController removeObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDevicePickedKeyName];
        m_outputDeviceMenuController = nullptr;
    }
    currentDeviceDidChange();
}

} // namespace WebCore

@implementation WebAVOutputDeviceMenuControllerHelper
- (instancetype)initWithCallback:(MediaPlaybackTargetPickerMac*)callback
{
    if (!(self = [super init]))
        return nil;

    m_callback = callback;

    return self;
}

- (void)clearCallback
{
    m_callback = nil;
}

- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context
{
    UNUSED_PARAM(object);
    UNUSED_PARAM(change);
    UNUSED_PARAM(context);

    if (!m_callback)
        return;

    LOG(Media, "MediaPlaybackTargetPickerMac::observeValueForKeyPath - key = %s", [keyPath UTF8String]);

    if (![keyPath isEqualToString:externalOutputDeviceAvailableKeyName] && ![keyPath isEqualToString:externalOutputDevicePickedKeyName])
        return;

    RetainPtr<WebAVOutputDeviceMenuControllerHelper> strongSelf = self;
    RetainPtr<NSString> strongKeyPath = keyPath;
    callOnMainThread([strongSelf, strongKeyPath] {
        MediaPlaybackTargetPickerMac* callback = strongSelf->m_callback;
        if (!callback)
            return;

        if ([strongKeyPath isEqualToString:externalOutputDeviceAvailableKeyName])
            callback->availableDevicesDidChange();
        else if ([strongKeyPath isEqualToString:externalOutputDevicePickedKeyName])
            callback->currentDeviceDidChange();
    });
}
@end

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
