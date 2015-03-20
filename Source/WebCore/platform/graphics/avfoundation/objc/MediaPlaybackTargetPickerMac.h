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

#ifndef MediaPlaybackTargetPickerMac_h
#define MediaPlaybackTargetPickerMac_h

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS)

#import "MediaPlaybackTargetPicker.h"
#include <wtf/RetainPtr.h>
#include <wtf/RunLoop.h>

OBJC_CLASS AVOutputDevicePickerMenuController;
OBJC_CLASS WebAVOutputDevicePickerMenuControllerHelper;

namespace WebCore {

class MediaPlaybackTargetPickerMac final : public MediaPlaybackTargetPicker {
    WTF_MAKE_NONCOPYABLE(MediaPlaybackTargetPickerMac);
public:
    virtual ~MediaPlaybackTargetPickerMac();

    WEBCORE_EXPORT static std::unique_ptr<MediaPlaybackTargetPickerMac> create(MediaPlaybackTargetPicker::Client&);

    virtual void showPlaybackTargetPicker(const FloatRect&, bool) override;
    virtual void startingMonitoringPlaybackTargets() override;
    virtual void stopMonitoringPlaybackTargets() override;
    
    void availableDevicesDidChange();
    void currentDeviceDidChange();

private:
    explicit MediaPlaybackTargetPickerMac(MediaPlaybackTargetPicker::Client&);

    AVOutputDevicePickerMenuController *devicePicker();
    void outputeDeviceAvailabilityChangedTimerFired();

    RetainPtr<AVOutputDevicePickerMenuController> m_devicePickerMenuController;
    RetainPtr<WebAVOutputDevicePickerMenuControllerHelper> m_devicePickerMenuControllerDelegate;
    RunLoop::Timer<MediaPlaybackTargetPickerMac> m_deviceChangeTimer;
};

} // namespace WebKit

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)

#endif // WebContextMenuProxyMac_h
