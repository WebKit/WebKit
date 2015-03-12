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
#import "WebMediaPlaybackTargetPickerProxyMac.h"

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

#import <AVFoundation/AVOutputDevicePickerContext.h>
#import <AVKit/AVOutputDevicePickerMenuController.h>
#import <WebCore/FloatRect.h>
#import <WebCore/MediaPlaybackTarget.h>
#import <WebCore/SoftLinking.h>
#import <objc/runtime.h>
#import <wtf/MainThread.h>

typedef AVOutputDevicePickerContext AVOutputDevicePickerContextType;
typedef AVOutputDevicePickerMenuController AVOutputDevicePickerMenuControllerType;

SOFT_LINK_FRAMEWORK_OPTIONAL(AVFoundation)
SOFT_LINK_FRAMEWORK_OPTIONAL(AVKit)

SOFT_LINK_CLASS(AVFoundation, AVOutputDevicePickerContext)
SOFT_LINK_CLASS(AVKit, AVOutputDevicePickerMenuController)
SOFT_LINK_CLASS(AVKit, AVOutputDevicePickerDelegate)

using namespace WebKit;

static NSString *externalOutputDeviceAvailableKeyName = @"externalOutputDeviceAvailable";
static NSString *externalOutputDevicePickedKeyName = @"externalOutputDevicePicked";

@interface WebAVOutputDevicePickerMenuControllerHelper : NSObject {
    WebMediaPlaybackTargetPickerProxyMac* m_callback;
}

- (instancetype)initWithCallback:(WebMediaPlaybackTargetPickerProxyMac*)callback;
- (void)clearCallback;
- (void)observeValueForKeyPath:(id)keyPath ofObject:(id)object change:(NSDictionary *)change context:(void *)context;
@end

namespace WebKit {

std::unique_ptr<WebMediaPlaybackTargetPickerProxyMac> WebMediaPlaybackTargetPickerProxyMac::create(WebMediaPlaybackTargetPickerProxy::Client& client)
{
    return std::unique_ptr<WebMediaPlaybackTargetPickerProxyMac>(new WebMediaPlaybackTargetPickerProxyMac(client));
}

WebMediaPlaybackTargetPickerProxyMac::WebMediaPlaybackTargetPickerProxyMac(WebMediaPlaybackTargetPickerProxy::Client& client)
    : WebMediaPlaybackTargetPickerProxy(client)
    , m_devicePickerMenuControllerDelegate(adoptNS([[WebAVOutputDevicePickerMenuControllerHelper alloc] initWithCallback:this]))
    , m_deviceChangeTimer(RunLoop::main(), this, &WebMediaPlaybackTargetPickerProxyMac::outputeDeviceAvailabilityChangedTimerFired)
{
}

WebMediaPlaybackTargetPickerProxyMac::~WebMediaPlaybackTargetPickerProxyMac()
{
    m_deviceChangeTimer.stop();
    [m_devicePickerMenuControllerDelegate clearCallback];

    if (m_devicePickerMenuController) {
        [m_devicePickerMenuController.get() removeObserver:m_devicePickerMenuControllerDelegate.get() forKeyPath:externalOutputDeviceAvailableKeyName];
        [m_devicePickerMenuController.get() removeObserver:m_devicePickerMenuControllerDelegate.get() forKeyPath:externalOutputDevicePickedKeyName];
        m_devicePickerMenuController = nil;
    }
}

void WebMediaPlaybackTargetPickerProxyMac::outputeDeviceAvailabilityChangedTimerFired()
{
    if (!m_devicePickerMenuController || !m_client)
        return;

    m_client->externalOutputDeviceAvailableDidChange(devicePicker().externalOutputDeviceAvailable);
}

void WebMediaPlaybackTargetPickerProxyMac::availableDevicesDidChange()
{
    if (!m_client)
        return;

    m_deviceChangeTimer.stop();
    m_deviceChangeTimer.startOneShot(0);
}

AVOutputDevicePickerMenuControllerType *WebMediaPlaybackTargetPickerProxyMac::devicePicker()
{
    if (!m_devicePickerMenuController) {
        RetainPtr<AVOutputDevicePickerContextType> context = adoptNS([[getAVOutputDevicePickerContextClass() alloc] init]);
        m_devicePickerMenuController = adoptNS([[getAVOutputDevicePickerMenuControllerClass() alloc] initWithOutputDevicePickerContext:context.get()]);

        [m_devicePickerMenuController.get() addObserver:m_devicePickerMenuControllerDelegate.get() forKeyPath:externalOutputDeviceAvailableKeyName options:NSKeyValueObservingOptionNew context:nullptr];
        [m_devicePickerMenuController.get() addObserver:m_devicePickerMenuControllerDelegate.get() forKeyPath:externalOutputDevicePickedKeyName options:NSKeyValueObservingOptionNew context:nullptr];

        if (devicePicker().externalOutputDeviceAvailable)
            availableDevicesDidChange();
    }

    return m_devicePickerMenuController.get();
}

void WebMediaPlaybackTargetPickerProxyMac::showPlaybackTargetPicker(const WebCore::FloatRect& location, bool)
{
    if (!m_client)
        return;

    [devicePicker() showMenuForRect:location appearanceName:NSAppearanceNameVibrantLight];
}

void WebMediaPlaybackTargetPickerProxyMac::currentDeviceDidChange()
{
    if (!m_client)
        return;

    m_client->didChoosePlaybackTarget(WebCore::MediaPlaybackTarget([devicePicker() outputDevicePickerContext]));
}

void WebMediaPlaybackTargetPickerProxyMac::startingMonitoringPlaybackTargets()
{
    devicePicker();
}

void WebMediaPlaybackTargetPickerProxyMac::stopMonitoringPlaybackTargets()
{
}

} // namespace WebKit

@implementation WebAVOutputDevicePickerMenuControllerHelper
- (instancetype)initWithCallback:(WebMediaPlaybackTargetPickerProxyMac*)callback
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
    if (!m_callback)
        return;

    if (![keyPath isEqualToString:externalOutputDeviceAvailableKeyName] && ![keyPath isEqualToString:externalOutputDevicePickedKeyName])
        return;

    RetainPtr<WebAVOutputDevicePickerMenuControllerHelper> strongSelf = self;
    RetainPtr<NSString> strongKeyPath = keyPath;
    callOnMainThread([strongSelf, strongKeyPath] {
        WebMediaPlaybackTargetPickerProxyMac* callback = strongSelf->m_callback;
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
