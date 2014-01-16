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

#ifndef DeviceMotionClientIOS_h
#define DeviceMotionClientIOS_h

#include "DeviceMotionClient.h"
#include "DeviceMotionController.h"
#include "DeviceMotionData.h"
#include <wtf/PassOwnPtr.h>
#include <wtf/RefPtr.h>

#ifdef __OBJC__
@class WebCoreMotionManager;
#else
class WebCoreMotionManager;
#endif

namespace WebCore {

class DeviceMotionClientIOS : public DeviceMotionClient {
public:
    static PassOwnPtr<DeviceMotionClientIOS> create()
    {
        return adoptPtr(new DeviceMotionClientIOS());
    }
    DeviceMotionClientIOS();
    virtual ~DeviceMotionClientIOS() override;
    virtual void setController(DeviceMotionController*) override;
    virtual void startUpdating() override;
    virtual void stopUpdating() override;
    virtual DeviceMotionData* lastMotion() const override;
    virtual void deviceMotionControllerDestroyed() override;

    void motionChanged(double, double, double, double, double, double, double, double, double);

private:
    WebCoreMotionManager* m_motionManager;
    DeviceMotionController* m_controller;
    RefPtr<DeviceMotionData> m_currentDeviceMotionData;
    bool m_updating;
};

} // namespace WebCore

#endif // DeviceMotionClientIOS_h
