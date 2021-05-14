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

#pragma once

#if ENABLE(WIRELESS_PLAYBACK_TARGET) && !PLATFORM(IOS_FAMILY)

#include "AVPlaybackTargetPicker.h"

OBJC_CLASS AVRouteDetector;
OBJC_CLASS AVRoutePickerView;
OBJC_CLASS WebAVRoutePickerViewHelper;

namespace WebCore {

class AVRoutePickerViewTargetPicker final : public AVPlaybackTargetPicker {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(AVRoutePickerViewTargetPicker);
public:
    explicit AVRoutePickerViewTargetPicker(AVPlaybackTargetPicker::Client&);
    virtual ~AVRoutePickerViewTargetPicker();
    
    static bool isAvailable();

    void availableDevicesDidChange();
    void currentDeviceDidChange();
    void devicePickerWasDismissed();

private:
    void showPlaybackTargetPicker(NSView *, const FloatRect&, bool checkActiveRoute, bool useDarkAppearance, bool useiTunesAVOutputContext) final;
    void startingMonitoringPlaybackTargets() final;
    void stopMonitoringPlaybackTargets() final;
    void invalidatePlaybackTargets() final;
    bool externalOutputDeviceAvailable() final;
    AVOutputContext *outputContext() final;

    AVRoutePickerView *devicePicker();
    AVRouteDetector *routeDetector();
    AVOutputContext * outputContextInternal(bool);
    bool hasActiveRoute() const;

    RetainPtr<AVRouteDetector> m_routeDetector;
    RetainPtr<AVRoutePickerView> m_routePickerView;
    RetainPtr<AVOutputContext> m_outputContext;
    RetainPtr<WebAVRoutePickerViewHelper> m_routePickerViewDelegate;
    bool m_hadActiveRoute { false };
    bool m_ignoreNextMultipleRoutesDetectedDidChangeNotification { false };
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
