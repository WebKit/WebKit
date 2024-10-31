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

#if ENABLE(WEB_AUTHN)

#import <WebCore/FidoConstants.h>
#import <wtf/BlockPtr.h>
#import <wtf/RunLoop.h>
#import <wtf/TZoneMallocInlines.h>

namespace WebKit {
using namespace fido;

#if HAVE(SECURITY_KEY_API)
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

    connection->receiveReport(unsafeMakeSpan(report, reportLength));
}
#endif // HAVE(SECURITY_KEY_API)

Ref<HidConnection> HidConnection::create(IOHIDDeviceRef device)
{
    return adoptRef(*new HidConnection(device));
}

HidConnection::HidConnection(IOHIDDeviceRef device)
    : m_device(device)
{
}

HidConnection::~HidConnection()
{
    ASSERT(!m_isInitialized);
}

void HidConnection::initialize()
{
#if HAVE(SECURITY_KEY_API)
    IOHIDDeviceOpen(m_device.get(), kIOHIDOptionsTypeSeizeDevice);
    IOHIDDeviceScheduleWithRunLoop(m_device.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    m_inputBuffer.resize(kHidMaxPacketSize);
    IOHIDDeviceRegisterInputReportCallback(m_device.get(), m_inputBuffer.data(), m_inputBuffer.size(), &reportReceived, this);
#endif
    m_isInitialized = true;
}

void HidConnection::terminate()
{
#if HAVE(SECURITY_KEY_API)
    IOHIDDeviceRegisterInputReportCallback(m_device.get(), m_inputBuffer.data(), m_inputBuffer.size(), nullptr, nullptr);
    IOHIDDeviceUnscheduleFromRunLoop(m_device.get(), CFRunLoopGetCurrent(), kCFRunLoopDefaultMode);
    IOHIDDeviceClose(m_device.get(), kIOHIDOptionsTypeNone);
#endif
    m_isInitialized = false;
}

auto HidConnection::sendSync(const Vector<uint8_t>& data) -> DataSent
{
#if HAVE(SECURITY_KEY_API)
    ASSERT(m_isInitialized);
    auto status = IOHIDDeviceSetReport(m_device.get(), kIOHIDReportTypeOutput, kHidReportId, data.data(), data.size());
    if (status) {
        LOG_ERROR("Couldn't send report to the authenticator: %d", status);
        return DataSent::No;
    }
    return DataSent::Yes;
#else
    return DataSent::No;
#endif
}

void HidConnection::send(Vector<uint8_t>&& data, DataSentCallback&& callback)
{
    ASSERT(m_isInitialized);
    auto task = makeBlockPtr([device = m_device, data = WTFMove(data), callback = WTFMove(callback)]() mutable {
        ASSERT(!RunLoop::isMain());

#if HAVE(SECURITY_KEY_API)
        DataSent sent = DataSent::Yes;
        auto status = IOHIDDeviceSetReport(device.get(), kIOHIDReportTypeOutput, kHidReportId, data.data(), data.size());
        if (status) {
            LOG_ERROR("Couldn't send report to the authenticator: %d", status);
            sent = DataSent::No;
        }
#else
        DataSent sent = DataSent::No;
#endif
        RunLoop::main().dispatch([callback = WTFMove(callback), sent]() mutable {
            callback(sent);
        });
    });
    dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), task.get());
}

void HidConnection::registerDataReceivedCallback(DataReceivedCallback&& callback)
{
    ASSERT(m_isInitialized);
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

#endif // ENABLE(WEB_AUTHN)
