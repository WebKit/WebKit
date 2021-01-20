/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
#import "AVOutputDeviceMenuControllerTargetPicker.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#import "Logging.h"
#import <WebCore/FloatRect.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/MainThread.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVOutputDeviceMenuController)

using namespace WebCore;

static NSString *externalOutputDeviceAvailableKeyName = @"externalOutputDeviceAvailable";
static NSString *externalOutputDevicePickedKeyName = @"externalOutputDevicePicked";

@interface WebAVOutputDeviceMenuControllerHelper : NSObject {
    WeakPtr<AVOutputDeviceMenuControllerTargetPicker> m_callback;
}

- (instancetype)initWithCallback:(WeakPtr<AVOutputDeviceMenuControllerTargetPicker>&&)callback;
- (void)clearCallback;
- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
@end

namespace WebCore {

AVOutputDeviceMenuControllerTargetPicker::AVOutputDeviceMenuControllerTargetPicker(AVPlaybackTargetPicker::Client& client)
    : AVPlaybackTargetPicker(client)
    , m_outputDeviceMenuControllerDelegate(adoptNS([[WebAVOutputDeviceMenuControllerHelper alloc] initWithCallback:makeWeakPtr(*this)]))
{
}

AVOutputDeviceMenuControllerTargetPicker::~AVOutputDeviceMenuControllerTargetPicker()
{
    [m_outputDeviceMenuControllerDelegate clearCallback];
}

AVOutputDeviceMenuController *AVOutputDeviceMenuControllerTargetPicker::devicePicker()
{
    if (!getAVOutputDeviceMenuControllerClass())
        return nullptr;

    if (!m_outputDeviceMenuController) {
        RetainPtr<AVOutputContext> context = adoptNS([PAL::allocAVOutputContextInstance() init]);
        m_outputDeviceMenuController = adoptNS([allocAVOutputDeviceMenuControllerInstance() initWithOutputContext:context.get()]);

        [m_outputDeviceMenuController.get() addObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDeviceAvailableKeyName options:NSKeyValueObservingOptionNew context:nullptr];
        [m_outputDeviceMenuController.get() addObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDevicePickedKeyName options:NSKeyValueObservingOptionNew context:nullptr];

        ALLOW_DEPRECATED_DECLARATIONS_BEGIN
        if (m_outputDeviceMenuController.get().externalOutputDeviceAvailable)
            availableDevicesDidChange();
        ALLOW_DEPRECATED_DECLARATIONS_END
    }

    return m_outputDeviceMenuController.get();
}

void AVOutputDeviceMenuControllerTargetPicker::availableDevicesDidChange()
{
    if (client())
        client()->availableDevicesChanged();
}

void AVOutputDeviceMenuControllerTargetPicker::currentDeviceDidChange()
{
    if (client())
        client()->currentDeviceChanged();
}

void AVOutputDeviceMenuControllerTargetPicker::showPlaybackTargetPicker(NSView *, const FloatRect& location, bool hasActiveRoute, bool useDarkAppearance)
{
    if (!client() || m_showingMenu)
        return;

    m_showingMenu = true;

    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    bool targetSelected = [devicePicker() showMenuForRect:location appearanceName:(useDarkAppearance ? NSAppearanceNameVibrantDark : NSAppearanceNameVibrantLight) allowReselectionOfSelectedOutputDevice:!hasActiveRoute];
    ALLOW_DEPRECATED_DECLARATIONS_END

    if (!client())
        return;

    if (targetSelected != hasActiveRoute)
        currentDeviceDidChange();
    else if (!targetSelected && !hasActiveRoute)
        client()->pickerWasDismissed();

    m_showingMenu = false;
}

void AVOutputDeviceMenuControllerTargetPicker::startingMonitoringPlaybackTargets()
{
    devicePicker();
}

void AVOutputDeviceMenuControllerTargetPicker::stopMonitoringPlaybackTargets()
{
    // Nothing to do, AirPlay takes care of this automatically.
}

void AVOutputDeviceMenuControllerTargetPicker::invalidatePlaybackTargets()
{
    if (m_outputDeviceMenuController) {
        [m_outputDeviceMenuController removeObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDeviceAvailableKeyName];
        [m_outputDeviceMenuController removeObserver:m_outputDeviceMenuControllerDelegate.get() forKeyPath:externalOutputDevicePickedKeyName];
        m_outputDeviceMenuController = nullptr;
    }
    currentDeviceDidChange();
}

bool AVOutputDeviceMenuControllerTargetPicker::externalOutputDeviceAvailable()
{
    ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    return devicePicker().externalOutputDeviceAvailable;
    ALLOW_DEPRECATED_DECLARATIONS_END
}

AVOutputContext * AVOutputDeviceMenuControllerTargetPicker::outputContext()
{
    return m_outputDeviceMenuController ? [m_outputDeviceMenuController.get() outputContext] : nullptr;
}

} // namespace WebCore

@implementation WebAVOutputDeviceMenuControllerHelper
- (instancetype)initWithCallback:(WeakPtr<AVOutputDeviceMenuControllerTargetPicker>&&)callback
{
    if (!(self = [super init]))
        return nil;

    m_callback = WTFMove(callback);

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

    if (![keyPath isEqualToString:externalOutputDeviceAvailableKeyName] && ![keyPath isEqualToString:externalOutputDevicePickedKeyName])
        return;

    callOnMainThread([self, protectedSelf = retainPtr(self), keyPath = retainPtr(keyPath)] {
        if (!m_callback)
            return;

        if ([keyPath isEqualToString:externalOutputDeviceAvailableKeyName])
            m_callback->availableDevicesDidChange();
        else if ([keyPath isEqualToString:externalOutputDevicePickedKeyName])
            m_callback->currentDeviceDidChange();
    });
}
@end

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
