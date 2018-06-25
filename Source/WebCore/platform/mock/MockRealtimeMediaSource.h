/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer
 *    in the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of Ericsson nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(MEDIA_STREAM)

#include "CaptureDevice.h"
#include "MockMediaDevice.h"
#include "RealtimeMediaSource.h"

namespace WebCore {

class MockRealtimeMediaSource : public RealtimeMediaSource {
public:
    virtual ~MockRealtimeMediaSource() = default;

    static void setDevices(Vector<MockMediaDevice>&&);
    static void addDevice(const MockMediaDevice&);
    static void removeDevice(const String& persistentId);
    WEBCORE_EXPORT static void resetDevices();

    static Vector<CaptureDevice>& audioDevices();
    static Vector<CaptureDevice>& videoDevices();
    static Vector<CaptureDevice>& displayDevices();

    static std::optional<CaptureDevice> captureDeviceWithPersistentID(CaptureDevice::DeviceType, const String&);

protected:
    MockRealtimeMediaSource(const String& id, Type, const String& name);

    static void createCaptureDevice(const MockMediaDevice&);

    virtual void updateSettings(RealtimeMediaSourceSettings&) = 0;
    virtual void initializeCapabilities(RealtimeMediaSourceCapabilities&) = 0;
    virtual void initializeSupportedConstraints(RealtimeMediaSourceSupportedConstraints&) = 0;

    const RealtimeMediaSourceCapabilities& capabilities() const override;
    const RealtimeMediaSourceSettings& settings() const override;

    RealtimeMediaSourceSupportedConstraints& supportedConstraints();

    const MockMediaDevice& device() const { return m_device; }
    MockMediaDevice m_device;

private:
    void initializeCapabilities();
    void initializeSettings();

    RealtimeMediaSourceSettings m_currentSettings;
    RealtimeMediaSourceSupportedConstraints m_supportedConstraints;
    std::unique_ptr<RealtimeMediaSourceCapabilities> m_capabilities;
};

} // namespace WebCore

#endif // ENABLE(MEDIA_STREAM)
