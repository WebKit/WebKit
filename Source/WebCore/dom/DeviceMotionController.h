/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2012 Samsung Electronics. All rights reserved.
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

#include "DeviceController.h"
#include <wtf/Noncopyable.h>

namespace WebCore {

class DeviceMotionClient;
class DeviceMotionData;

class DeviceMotionController final : public DeviceController {
    WTF_MAKE_NONCOPYABLE(DeviceMotionController);
public:
    explicit DeviceMotionController(DeviceMotionClient&);
    virtual ~DeviceMotionController() = default;

#if PLATFORM(IOS_FAMILY)
    // FIXME: We should look to reconcile the iOS and OpenSource differences with this class
    // so that we can either remove these methods or remove the PLATFORM(IOS_FAMILY)-guard.
    void suspendUpdates();
    void resumeUpdates();
#endif

    void didChangeDeviceMotion(DeviceMotionData*);
    DeviceMotionClient& deviceMotionClient();

    bool hasLastData() override;
    RefPtr<Event> getLastEvent() override;

    static const char* supplementName();
    static DeviceMotionController* from(Page*);
    static bool isActiveAt(Page*);
};

} // namespace WebCore
