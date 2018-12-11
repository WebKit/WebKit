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

#import "config.h"
#import "HidConnection.h"

#if ENABLE(WEB_AUTHN) && PLATFORM(MAC)

#import <WebCore/FidoConstants.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>

namespace WebKit {
using namespace fido;

// FIXME(191518)
static void reportReceived(void* context, IOReturn status, void*, IOHIDReportType type, uint32_t reportID, uint8_t* report, CFIndex reportLength)
{
    ASSERT(RunLoop::isMain());
    auto* connection = static_cast<HidConnection*>(context);

    if (status) {
        LOG_ERROR("Couldn't receive report from the authenticator: %d", status);
        connection->receiveReport({ });
        return;
    }
    ASSERT(type == kIOHIDReportTypeInput);
    // FIXME(191519): Determine report id and max report size dynamically.
    ASSERT(reportID == kHidReportId);
    ASSERT(reportLength == kHidMaxPacketSize);

    Vector<uint8_t> reportData;
    reportData.append(report, reportLength);
    connection->receiveReport(WTFMove(reportData));
}

HidConnection::HidConnection(IOHIDDeviceRef device)
    : m_device(device)
{
}

HidConnection::~HidConnection()
{
    ASSERT(m_terminated);
}

void HidConnection::initialize()
{
    IOHIDDeviceOpen(m_device.get(), kIOHIDOptionsTypeNone);
    IOHIDDeviceScheduleWithRunLoop(m_device.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    m_inputBuffer.resize(kHidMaxPacketSize);
    IOHIDDeviceRegisterInputReportCallback(m_device.get(), m_inputBuffer.data(), m_inputBuffer.size(), &reportReceived, this);
    m_initialized = true;
}

void HidConnection::terminate()
{
    IOHIDDeviceRegisterInputReportCallback(m_device.get(), m_inputBuffer.data(), m_inputBuffer.size(), nullptr, nullptr);
    IOHIDDeviceUnscheduleFromRunLoop(m_device.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDDeviceClose(m_device.get(), kIOHIDOptionsTypeNone);
    m_terminated = true;
}

void HidConnection::send(Vector<uint8_t>&& data, DataSentCallback&& callback)
{
    ASSERT(m_initialized);
    auto task = makeBlockPtr([device = m_device, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        ASSERT(!RunLoop::isMain());

        DataSent sent = DataSent::Yes;
        auto status = IOHIDDeviceSetReport(device.get(), kIOHIDReportTypeOutput, kHidReportId, data.data(), data.size());
        if (status) {
            LOG_ERROR("Couldn't send report to the authenticator: %d", status);
            sent = DataSent::No;
        }
        RunLoop::main().dispatch([callback = WTFMove(callback), sent]() mutable {
            callback(sent);
        });
    });
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), task.get());
}

void HidConnection::registerDataReceivedCallback(DataReceivedCallback&& callback)
{
    ASSERT(m_initialized);
    ASSERT(!m_inputCallback);
    m_inputCallback = WTFMove(callback);
    consumeReports();
    registerDataReceivedCallbackInternal();
}

void HidConnection::unregisterDataReceivedCallback()
{
    m_inputCallback = nullptr;
}

void HidConnection::receiveReport(Vector<uint8_t>&& report)
{
    m_inputReports.append(WTFMove(report));
    consumeReports();
}

void HidConnection::consumeReports()
{
    while (!m_inputReports.isEmpty() && m_inputCallback)
        m_inputCallback(m_inputReports.takeFirst());
}

void HidConnection::registerDataReceivedCallbackInternal()
{
}

} // namespace WebKit

#endif // ENABLE(WEB_AUTHN) && PLATFORM(MAC)

