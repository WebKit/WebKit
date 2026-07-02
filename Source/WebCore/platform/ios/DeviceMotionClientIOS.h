/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)

#include "DeviceMotionClient.h"
#include "DeviceMotionController.h"
#include "DeviceMotionData.h"
#include "DeviceOrientationUpdateProvider.h"
#include "MotionManagerClient.h"
#include <wtf/CheckedRef.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS WebCoreMotionManager;

namespace WebCore {

class DeviceMotionClientIOS : public DeviceMotionClient, public MotionManagerClient, public CanMakeCheckedPtr<DeviceMotionClientIOS> {
    WTF_MAKE_TZONE_ALLOCATED(DeviceMotionClientIOS);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DeviceMotionClientIOS);
public:
    DeviceMotionClientIOS(RefPtr<DeviceOrientationUpdateProvider>&&);
    ~DeviceMotionClientIOS() override;
    void setController(DeviceMotionController*) override;
    void startUpdating() override;
    void stopUpdating() override;
    DeviceMotionData* lastMotion() const override;
    void deviceMotionControllerDestroyed() override;

    void motionChanged(double, double, double, double, double, double, std::optional<double>, std::optional<double>, std::optional<double>) override;

    // DeviceMotionClient, MotionManagerClient.
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

private:
    WebCoreMotionManager* m_motionManager { nullptr };
    DeviceMotionController* m_controller { nullptr };
    RefPtr<DeviceMotionData> m_currentDeviceMotionData;
    RefPtr<DeviceOrientationUpdateProvider> m_deviceOrientationUpdateProvider;
    bool m_updating { false };
};

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
