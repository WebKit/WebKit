/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#include <IOKit/hid/IOHIDDevice.h>
#include <wtf/CompletionHandler.h>
#include <wtf/Deque.h>
#include <wtf/Forward.h>
#include <wtf/Function.h>
#include <wtf/Noncopyable.h>
#include <wtf/RetainPtr.h>

namespace WebKit {

class HidConnection {
    WTF_MAKE_FAST_ALLOCATED;
    WTF_MAKE_NONCOPYABLE(HidConnection);
public:
    enum class DataSent {
        No,
        Yes
    };

    using DataSentCallback = CompletionHandler<void(DataSent)>;
    using DataReceivedCallback = Function<void(Vector<uint8_t>&&)>;

    explicit HidConnection(IOHIDDeviceRef);
    virtual ~HidConnection();

    // Overrided by MockHidConnection.
    virtual void initialize();
    virtual void terminate();
    // Caller should send data again after callback is invoked to control flow.
    virtual void send(Vector<uint8_t>&& data, DataSentCallback&&);
    void registerDataReceivedCallback(DataReceivedCallback&&);
    void unregisterDataReceivedCallback();

    void receiveReport(Vector<uint8_t>&&);

protected:
    bool m_initialized { false };
    bool m_terminated { false };

private:
    void consumeReports();

    // Overrided by MockHidConnection.
    virtual void registerDataReceivedCallbackInternal();

    RetainPtr<IOHIDDeviceRef> m_device;
    Vector<uint8_t> m_inputBuffer;
    // Could queue data requested by other applications.
    Deque<Vector<uint8_t>> m_inputReports;
    DataReceivedCallback m_inputCallback;
};

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)
