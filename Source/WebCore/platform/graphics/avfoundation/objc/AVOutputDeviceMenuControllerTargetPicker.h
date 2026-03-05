/*
 * Copyright (C) 2020-2022 Apple Inc. All rights reserved.
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
#include <wtf/TZoneMalloc.h>

OBJC_CLASS AVOutputDeviceMenuController;
OBJC_CLASS WebAVOutputDeviceMenuControllerHelper;

namespace WebCore {

class AVOutputDeviceMenuControllerTargetPicker final : public AVPlaybackTargetPicker {
    WTF_MAKE_TZONE_ALLOCATED(AVOutputDeviceMenuControllerTargetPicker);
    WTF_MAKE_NONCOPYABLE(AVOutputDeviceMenuControllerTargetPicker);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(AVOutputDeviceMenuControllerTargetPicker);
public:
    explicit AVOutputDeviceMenuControllerTargetPicker(AVPlaybackTargetPickerClient&);
    virtual ~AVOutputDeviceMenuControllerTargetPicker();

    void availableDevicesDidChange();
    void currentDeviceDidChange();

private:
    void showPlaybackTargetPicker(NSView *, const FloatRect&, bool checkActiveRoute, bool useDarkAppearance) final;
    void startingMonitoringPlaybackTargets() final;
    void stopMonitoringPlaybackTargets() final;
    void invalidatePlaybackTargets() final;
    bool externalOutputDeviceAvailable() final;
    AVOutputContext* outputContext() final;

    AVOutputDeviceMenuController *devicePicker();

    RetainPtr<AVOutputDeviceMenuController> m_outputDeviceMenuController;
    RetainPtr<WebAVOutputDeviceMenuControllerHelper> m_outputDeviceMenuControllerDelegate;
    bool m_showingMenu { false };
};

} // namespace WebCore

#endif // ENABLE(WIRELESS_PLAYBACK_TARGET)
