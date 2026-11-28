/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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

#if HAVE(SCREEN_CAPTURE_KIT)

#include <WebCore/CaptureDevice.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/text/WTFString.h>

OBJC_CLASS SCContentFilter;

namespace WebCore {

class ScreenCaptureKitSelection {
    WTF_MAKE_TZONE_ALLOCATED(ScreenCaptureKitSelection);
    WTF_MAKE_NONCOPYABLE(ScreenCaptureKitSelection);
    WTF_MAKE_NONMOVABLE(ScreenCaptureKitSelection);
public:
    using UnregisterHandler = Function<void()>;

    static std::unique_ptr<ScreenCaptureKitSelection> create(RetainPtr<SCContentFilter>&& filter, String&& deviceID, CaptureDevice::DeviceType deviceType, UnregisterHandler&& unregisterHandler)
    {
        return std::unique_ptr<ScreenCaptureKitSelection>(new ScreenCaptureKitSelection(WTF::move(filter), WTF::move(deviceID), deviceType, WTF::move(unregisterHandler)));
    }

    ~ScreenCaptureKitSelection() { unregister(); }

    void unregister()
    {
        if (auto unregisterHandler = std::exchange(m_unregisterHandler, nullptr))
            unregisterHandler();
    }

    SCContentFilter* contentFilter() const { return m_filter.get(); }
    const String& deviceID() const { return m_deviceID; }
    CaptureDevice::DeviceType deviceType() const { return m_deviceType; }

    void setFilter(RetainPtr<SCContentFilter>&& filter) { m_filter = WTF::move(filter); }

private:
    ScreenCaptureKitSelection(RetainPtr<SCContentFilter>&& filter, String&& deviceID, CaptureDevice::DeviceType deviceType, UnregisterHandler&& unregisterHandler)
        : m_filter(WTF::move(filter))
        , m_deviceID(WTF::move(deviceID))
        , m_deviceType(deviceType)
        , m_unregisterHandler(WTF::move(unregisterHandler))
    {
    }

    RetainPtr<SCContentFilter> m_filter;
    const String m_deviceID;
    const CaptureDevice::DeviceType m_deviceType { CaptureDevice::DeviceType::Unknown };
    UnregisterHandler m_unregisterHandler;
};

} // namespace WebCore

#endif // HAVE(SCREEN_CAPTURE_KIT)
