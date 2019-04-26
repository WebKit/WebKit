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

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import <WebCore/FloatRect.h>
#import <WebCore/MediaPlaybackTargetMac.h>
#import <objc/runtime.h>
#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <pal/spi/mac/AVFoundationSPI.h>
#import <wtf/MainThread.h>

#import <pal/cocoa/AVFoundationSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVOutputDeviceMenuController)

using namespace WebCore;

static NSString *externalOutputDeviceAvailableKeyName = @"externalOutputDeviceAvailable";
static NSString *externalOutputDevicePickedKeyName = @"externalOutputDevicePicked";

@interface WebAVOutputDeviceMenuControllerHelper : NSObject {
    MediaPlaybackTargetPickerMac* m_callback;
}

- (instancetype)initWithCallback:(MediaPlaybackTargetPickerMac*)callback;
- (void)clearCallback;
- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
@end

namespace WebCore {

MediaPlaybackTargetPickerMac::MediaPlaybackTargetPickerMac(MediaPlaybackTargetPicker::Client& client)
    : MediaPlaybackTargetPicker(client)
    , m_outputDeviceMenuControllerDelegate(adoptNS([[WebAVOutputDeviceMenuControllerHelper alloc] initWithCallback:this]))
{
}

MediaPlaybackTargetPickerMac::~MediaPlaybackTargetPickerMac()
{
    setClient(nullptr);
    [m_outputDeviceMenuControllerDelegate clearCallback];
}

bool MediaPlaybackTargetPickerMac::externalOutputDeviceAvailable()
{
    return devicePicker().externalOutputDeviceAvailable;
}

Ref<MediaPlaybackTarget> MediaPlaybackTargetPickerMac::playbackTarget()
{
    AVOutputContext* context = m_outputDeviceMenuController ? [m_outputDeviceMenuController.get() outputContext] : nullptr;

    return WebCore::MediaPlaybackTargetMac::create(context);
}

AVOutputDeviceMenuController *MediaPlaybackTargetPickerMac::devicePicker()
{
    if (!getAVOutputDeviceMenuControllerClass())
        return nullptr;

    if (!m_outputDeviceMenuController) {
        LOG(Media, "MediaPlaybackTargetPickerMac::devicePicker - allocating picker");

        RetainPtr<AVOutputContext> context = adoptNS([PAL::allocAVOutputContextInstance() init]);
        m_outputDeviceMenuController = adoptNS([allocAVOutputDeviceMenuControllerInstance() initWithOutputContext:context.get()]);

        [m_outputDeviceMenuController.get() addObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDeviceAvailableKeyName options:NSKeyValueObservingOptionNew context:nullptr];
        [m_outputDeviceMenuController.get() addObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDevicePickedKeyName options:NSKeyValueObservingOptionNew context:nullptr];

        LOG(Media, "MediaPlaybackTargetPickerMac::devicePicker - allocated menu controller %p", m_outputDeviceMenuController.get());

        if (m_outputDeviceMenuController.get().externalOutputDeviceAvailable)
            availableDevicesDidChange();
    }

    return m_outputDeviceMenuController.get();
}

void MediaPlaybackTargetPickerMac::showPlaybackTargetPicker(const FloatRect& location, bool checkActiveRoute, bool useDarkAppearance)
{
    if (!client() || m_showingMenu)
        return;

    LOG(Media, "MediaPlaybackTargetPickerMac::showPlaybackTargetPicker - checkActiveRoute = %i", (int)checkActiveRoute);

    m_showingMenu = true;

    if ([devicePicker() showMenuForRect:location appearanceName:(useDarkAppearance ? NSAppearanceNameVibrantDark : NSAppearanceNameVibrantLight) allowReselectionOfSelectedOutputDevice:!checkActiveRoute]) {
        if (!checkActiveRoute)
            currentDeviceDidChange();
    }
    m_showingMenu = false;
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

    RetainPtr<WebAVOutputDeviceMenuControllerHelper> protectedSelf = self;
    RetainPtr<NSString> protectedKeyPath = keyPath;
    callOnMainThread([protectedSelf = WTFMove(protectedSelf), protectedKeyPath = WTFMove(protectedKeyPath)] {
        MediaPlaybackTargetPickerMac* callback = protectedSelf->m_callback;
        if (!callback)
            return;

        if ([protectedKeyPath isEqualToString:externalOutputDeviceAvailableKeyName])
            callback->availableDevicesDidChange();
        else if ([protectedKeyPath isEqualToString:externalOutputDevicePickedKeyName])
            callback->currentDeviceDidChange();
    });
}
@end

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
