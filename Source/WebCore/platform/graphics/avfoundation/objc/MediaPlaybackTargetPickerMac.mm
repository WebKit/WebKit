/*
 * Copyright (C) 2015-2020 Apple Inc. All rights reserved.
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

#import "AVOutputDeviceMenuControllerTargetPicker.h"
#import "AVRoutePickerViewTargetPicker.h"
#import "Logging.h"
#import <WebCore/FloatRect.h>
#import <WebCore/MediaPlaybackTargetCocoa.h>
#import <objc/runtime.h>
#import <pal/spi/cocoa/AVFoundationSPI.h>
#import <pal/spi/cocoa/AVKitSPI.h>
#import <wtf/MainThread.h>

#import <pal/cf/CoreMediaSoftLink.h>
#import <pal/cocoa/AVFoundationSoftLink.h>

SOFTLINK_AVKIT_FRAMEWORK()
SOFT_LINK_CLASS_OPTIONAL(AVKit, AVOutputDeviceMenuController)

using namespace WebCore;

namespace WebCore {

MediaPlaybackTargetPickerMac::MediaPlaybackTargetPickerMac(MediaPlaybackTargetPicker::Client& client)
    : MediaPlaybackTargetPicker(client)
{
}

MediaPlaybackTargetPickerMac::~MediaPlaybackTargetPickerMac()
{
    setClient(nullptr);
}

bool MediaPlaybackTargetPickerMac::externalOutputDeviceAvailable()
{
    return routePicker().externalOutputDeviceAvailable();
}

Ref<MediaPlaybackTarget> MediaPlaybackTargetPickerMac::playbackTarget()
{
    return WebCore::MediaPlaybackTargetCocoa::create(routePicker().outputContext());
}

AVPlaybackTargetPicker& MediaPlaybackTargetPickerMac::routePicker()
{
    if (m_routePicker)
        return *m_routePicker;

#if HAVE(AVROUTEPICKERVIEW)
    if (AVRoutePickerViewTargetPicker::isAvailable())
        m_routePicker = makeUnique<AVRoutePickerViewTargetPicker>(*this);
    else
#endif
        m_routePicker = makeUnique<AVOutputDeviceMenuControllerTargetPicker>(*this);
    
    return *m_routePicker;
}

void MediaPlaybackTargetPickerMac::showPlaybackTargetPicker(PlatformView* view, const FloatRect& location, bool hasActiveRoute, bool useDarkAppearance)
{
    routePicker().showPlaybackTargetPicker(view, location, hasActiveRoute, useDarkAppearance);
}

void MediaPlaybackTargetPickerMac::startingMonitoringPlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::startingMonitoringPlaybackTargets");

    routePicker().startingMonitoringPlaybackTargets();
}

void MediaPlaybackTargetPickerMac::stopMonitoringPlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::stopMonitoringPlaybackTargets");
    routePicker().stopMonitoringPlaybackTargets();
}

void MediaPlaybackTargetPickerMac::invalidatePlaybackTargets()
{
    LOG(Media, "MediaPlaybackTargetPickerMac::invalidatePlaybackTargets");

    m_routePicker = nullptr;
    currentDeviceDidChange();
}

void MediaPlaybackTargetPickerMac::pickerWasDismissed()
{
    playbackTargetPickerWasDismissed();
}

void MediaPlaybackTargetPickerMac::availableDevicesChanged()
{
    availableDevicesDidChange();
}

void MediaPlaybackTargetPickerMac::currentDeviceChanged()
{
    currentDeviceDidChange();
}


} // namespace WebCore


#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
