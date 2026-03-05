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

#include "DeviceOrientationClient.h"
#include "DeviceOrientationController.h"
#include "DeviceOrientationData.h"
#include "DeviceOrientationUpdateProvider.h"
#include "MotionManagerClient.h"
#include <wtf/CheckedRef.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

OBJC_CLASS WebCoreMotionManager;

namespace WebCore {

class DeviceOrientationClientIOS : public DeviceOrientationClient, public MotionManagerClient, public CanMakeCheckedPtr<DeviceOrientationClientIOS> {
    WTF_MAKE_TZONE_ALLOCATED(DeviceOrientationClientIOS);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(DeviceOrientationClientIOS);
public:
    DeviceOrientationClientIOS(RefPtr<DeviceOrientationUpdateProvider>&&);
    ~DeviceOrientationClientIOS() override;
    void setController(DeviceOrientationController*) override;
    void startUpdating() override;
    void stopUpdating() override;
    DeviceOrientationData* lastOrientation() const override;
    void deviceOrientationControllerDestroyed() override;

    void orientationChanged(double, double, double, double, double) override;

    // DeviceOrientationClient, MotionManagerClient.
    uint32_t checkedPtrCount() const final { return CanMakeCheckedPtr::checkedPtrCount(); }
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return CanMakeCheckedPtr::checkedPtrCountWithoutThreadCheck(); }
    void incrementCheckedPtrCount() const final { CanMakeCheckedPtr::incrementCheckedPtrCount(); }
    void decrementCheckedPtrCount() const final { CanMakeCheckedPtr::decrementCheckedPtrCount(); }
    void setDidBeginCheckedPtrDeletion() final { CanMakeCheckedPtr::setDidBeginCheckedPtrDeletion(); }

private:
    WebCoreMotionManager* m_motionManager  { nullptr };
    DeviceOrientationController* m_controller  { nullptr };
    RefPtr<DeviceOrientationData> m_currentDeviceOrientation;
    RefPtr<DeviceOrientationUpdateProvider> m_deviceOrientationUpdateProvider;
    bool m_updating { false };
};

} // namespace WebCore

#endif // PLATFORM(IOS_FAMILY) && ENABLE(DEVICE_ORIENTATION)
