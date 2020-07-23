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

#if HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)

#include <dispatch/dispatch.h>
#include <wtf/FastMalloc.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/RetainPtr.h>
#include <wtf/Vector.h>


OBJC_CLASS HIDDevice;
OBJC_CLASS HIDUserDevice;
OBJC_CLASS NSString;

namespace TestWebKitAPI {

class VirtualGamepad {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<VirtualGamepad> makeVirtualNimbus();
    enum class Layout {
        Nimbus,
    };
    VirtualGamepad(Layout);
    ~VirtualGamepad();

    size_t buttonCount() const;
    size_t axisCount() const;

    // Acceptable values are 0.0 to 1.0
    void setButtonValue(size_t index, float value);

    // Acceptable values are -1.0 to 1.0
    void setAxisValue(size_t index, float value);

    void publishReport();

private:
    RetainPtr<NSString> m_uniqueID;
    RetainPtr<HIDUserDevice> m_userDevice;
    RetainPtr<HIDDevice> m_device;
    OSObjectPtr<dispatch_queue_t> m_dispatchQueue;

    Layout m_layout;
    Vector<float> m_buttonValues;
    Vector<float> m_axisValues;

};
} // namespace TestWebKitAPI

#endif // HAVE(HID_FRAMEWORK) && USE(APPLE_INTERNAL_SDK)
