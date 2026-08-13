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

#include "config.h"

#if PLATFORM(COCOA)

#include "ArgumentCoders.h"
#include "Decoder.h"
#include "Encoder.h"
#include "Helpers/Test.h"
#include "MachMessage.h"
#include "MessageNames.h"
#include <mach/mach.h>
#include <wtf/MachSendRight.h>
#include <wtf/MathExtras.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/spi/cocoa/MachVMSPI.h>

namespace TestWebKitAPI {

namespace {

// Sends to and receives from a port owned by this process, so that the mach message that
// MachMessage::sendEncoder() builds is the one MachMessage::receiveDecoder() decodes.
class MachMessageTest : public ::testing::Test {
public:
    void SetUp() override
    {
        m_port = allocatePortWithSendRight();
        m_attachmentPort = allocatePortWithSendRight();
    }

    void TearDown() override
    {
        deallocatePort(m_port);
        deallocatePort(m_attachmentPort);
    }

    static mach_port_t allocatePortWithSendRight()
    {
        mach_port_t port = MACH_PORT_NULL;
        EXPECT_EQ(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port), KERN_SUCCESS);
        EXPECT_EQ(mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND), KERN_SUCCESS);
        return port;
    }

    static void deallocatePort(mach_port_t port)
    {
        if (!MACH_PORT_VALID(port))
            return;
        mach_port_deallocate(mach_task_self(), port);
        mach_port_mod_refs(mach_task_self(), port, MACH_PORT_RIGHT_RECEIVE, -1);
    }

    static void setQueueLength(mach_port_t port, mach_port_msgcount_t length)
    {
        mach_port_limits_t limits { };
        limits.mpl_qlimit = length;
        EXPECT_EQ(mach_port_set_attributes(mach_task_self(), port, MACH_PORT_LIMITS_INFO, reinterpret_cast<mach_port_info_t>(&limits), MACH_PORT_LIMITS_INFO_COUNT), KERN_SUCCESS);
    }

    static mach_port_urefs_t sendRightCount(mach_port_t port)
    {
        mach_port_urefs_t refs = 0;
        EXPECT_EQ(mach_port_get_refs(mach_task_self(), port, MACH_PORT_RIGHT_SEND, &refs), KERN_SUCCESS);
        return refs;
    }

    // Whether `address` is still mapped in this process, so that a test can tell whether
    // mach_msg_destroy() released what the kernel copied out.
    static bool isMapped(void* address)
    {
        auto queried = reinterpret_cast<mach_vm_address_t>(address);
        mach_vm_address_t regionStart = queried;
        mach_vm_size_t regionSize = 0;
        vm_region_basic_info_data_64_t info { };
        mach_msg_type_number_t infoCount = VM_REGION_BASIC_INFO_COUNT_64;
        mach_port_t object = MACH_PORT_NULL;
        if (mach_vm_region(mach_task_self(), &regionStart, &regionSize, VM_REGION_BASIC_INFO_64, reinterpret_cast<vm_region_info_t>(&info), &infoCount, &object) != KERN_SUCCESS)
            return false;
        return regionStart <= queried && queried < regionStart + regionSize;
    }

    // Receives the message the test just sent and expects receiveDecoder() to refuse it, with nothing left behind.
    void expectInvalidMessage()
    {
        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_FALSE(!!received);
        EXPECT_EQ(received.error(), IPC::MachMessage::ReceiveResult::InvalidMessage);
        // The message was destroyed with everything it carried, so the queue is empty and nothing is held.
        EXPECT_FALSE(!!IPC::MachMessage::receiveDecoder(m_port));
    }

    static Vector<uint8_t> makeBody(size_t size)
    {
        return Vector<uint8_t>(size, [](size_t i) {
            return static_cast<uint8_t>(i * 7 + 1);
        });
    }

    static constexpr IPC::MessageName testMessageName = IPC::MessageName::IPCTester_EmptyMessage;
    static constexpr uint64_t testDestinationID = 77;

    UniqueRef<IPC::Encoder> makeEncoder(const Vector<uint8_t>& body)
    {
        auto encoder = makeUniqueRef<IPC::Encoder>(testMessageName, testDestinationID);
        encoder.get() << body;
        return encoder;
    }

protected:
    mach_port_t m_port { MACH_PORT_NULL };
    mach_port_t m_attachmentPort { MACH_PORT_NULL };
};

// The message ID MachMessage uses. It is private to MachMessage.cpp, so the raw messages below carry a
// copy of it: a message with any other ID is a notification rather than an invalid message.
constexpr mach_msg_id_t webkitMachMessageID = 0xdba0dba;

// A message built by hand, so that receiveDecoder() can be given the messages a sender could send rather than
// only the ones sendEncoder() builds, and received by hand, so that a test can inspect the descriptors
// the kernel copied out instead of letting receiveDecoder() adopt them and destroy the message.
class RawMessage {
    WTF_MAKE_NONCOPYABLE(RawMessage);
public:
    explicit RawMessage(mach_msg_id_t messageID = webkitMachMessageID)
    {
        zeroSpan(std::span<uint8_t> { m_storage });
        header().msgh_bits = MACH_MSGH_BITS(MACH_MSG_TYPE_COPY_SEND, 0);
        header().msgh_id = messageID;
        m_size = sizeof(mach_msg_header_t);
    }

    void setDescriptorCount(mach_msg_size_t descriptorCount)
    {
        header().msgh_bits |= MACH_MSGH_BITS_COMPLEX;
        append(mach_msg_body_t { .msgh_descriptor_count = descriptorCount });
    }

    template<typename T> void append(const T& value)
    {
        memcpySpan(std::span<uint8_t> { m_storage }.subspan(m_size, sizeof(value)), asByteSpan(value));
        m_size += sizeof(value);
    }

    // Leaves zeroes in the message, for a body that does not have to decode.
    void grow(size_t size) { m_size += size; }

    kern_return_t send(mach_port_t destination)
    {
        prepareToSend(destination);
        return mach_msg(&header(), MACH_SEND_MSG, header().msgh_size, 0, MACH_PORT_NULL, MACH_MSG_TIMEOUT_NONE, MACH_PORT_NULL);
    }

    kern_return_t sendNoBlock(mach_port_t destination)
    {
        prepareToSend(destination);
        return mach_msg(&header(), MACH_SEND_MSG | MACH_SEND_TIMEOUT, header().msgh_size, 0, MACH_PORT_NULL, 0, MACH_PORT_NULL);
    }

    // Receives over this message, so that what the kernel copied out can be read out of the same storage
    // the message was built in.
    kern_return_t receive(mach_port_t port)
    {
        auto result = mach_msg(&header(), MACH_RCV_MSG | MACH_RCV_VOUCHER | MACH_RCV_TIMEOUT, 0, static_cast<mach_msg_size_t>(m_storage.size()), port, 0, MACH_PORT_NULL);
        if (result == MACH_MSG_SUCCESS)
            m_size = header().msgh_size;
        return result;
    }

    // The first descriptor of a complex message, which is the only one the tests below need.
    template<typename T> T& descriptor()
    {
        return reinterpretCastSpanStartTo<T>(std::span<uint8_t> { m_storage }.subspan(sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t)));
    }

    // Releases everything the message names, as the destroy handler of receiveDecoder() does.
    void destroy() { ::mach_msg_destroy(&header()); }

private:
    void prepareToSend(mach_port_t destination)
    {
        header().msgh_remote_port = destination;
        header().msgh_local_port = MACH_PORT_NULL;
        header().msgh_voucher_port = MACH_PORT_NULL;
        header().msgh_size = static_cast<mach_msg_size_t>(round_msg(m_size));
    }

    mach_msg_header_t& header() { return reinterpretCastSpanStartTo<mach_msg_header_t>(std::span<uint8_t> { m_storage }); }

    alignas(mach_msg_header_t) std::array<uint8_t, 2 * 4096> m_storage;
    size_t m_size { 0 };
};

} // namespace

TEST_F(MachMessageTest, ReceiveWithoutMessageIsNoMessage)
{
    auto received = IPC::MachMessage::receiveDecoder(m_port);
    ASSERT_FALSE(!!received);
    EXPECT_EQ(received.error(), IPC::MachMessage::ReceiveResult::NoMessage);
}

// A body that fits in the message is sent inline.
TEST_F(MachMessageTest, SendReceiveInlineBody)
{
    auto body = makeBody(128);
    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(makeEncoder(body), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    auto received = IPC::MachMessage::receiveDecoder(m_port);
    ASSERT_TRUE(!!received);
    auto decoder = WTF::move(received.value());
    EXPECT_EQ(decoder->messageName(), testMessageName);
    EXPECT_EQ(decoder->destinationID(), testDestinationID);
    auto decodedBody = decoder->decode<Vector<uint8_t>>();
    ASSERT_TRUE(!!decodedBody);
    EXPECT_TRUE(*decodedBody == body);
}

// A body that does not fit in the message is sent out-of-line, and the message refers to the body of
// the encoder it holds.
TEST_F(MachMessageTest, SendReceiveOutOfLineBody)
{
    auto body = makeBody(100 * KB);
    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(makeEncoder(body), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    auto received = IPC::MachMessage::receiveDecoder(m_port);
    ASSERT_TRUE(!!received);
    auto decoder = WTF::move(received.value());
    EXPECT_EQ(decoder->messageName(), testMessageName);
    EXPECT_EQ(decoder->destinationID(), testDestinationID);
    auto decodedBody = decoder->decode<Vector<uint8_t>>();
    ASSERT_TRUE(!!decodedBody);
    EXPECT_TRUE(*decodedBody == body);
}

// The attachments of the encoder are sent as port descriptors.
TEST_F(MachMessageTest, SendReceiveAttachment)
{
    auto body = makeBody(128);
    auto encoder = makeEncoder(body);
    encoder.get() << MachSendRight::create(m_attachmentPort);
    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    auto received = IPC::MachMessage::receiveDecoder(m_port);
    ASSERT_TRUE(!!received);
    auto decoder = WTF::move(received.value());
    auto decodedBody = decoder->decode<Vector<uint8_t>>();
    ASSERT_TRUE(!!decodedBody);
    EXPECT_TRUE(*decodedBody == body);
    auto sendRight = decoder->decode<MachSendRight>();
    ASSERT_TRUE(!!sendRight);
    // The receiver is in the same task as the sender, so the right has the same name.
    EXPECT_EQ(sendRight->sendRight(), m_attachmentPort);
}

// A null send right is a valid attachment. The kernel accepts it and delivers it as a port descriptor
// with a null name and a MACH_MSG_TYPE_PORT_SEND disposition, and it decodes as an invalid
// MachSendRight. Refusing it would drop the whole message rather than just the right, which is what
// broke window resizing: the messages that carry an unvalidated null right went missing.
TEST_F(MachMessageTest, SendReceiveNullAttachment)
{
    auto body = makeBody(128);
    auto encoder = makeEncoder(body);
    encoder.get() << MachSendRight { };
    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    auto received = IPC::MachMessage::receiveDecoder(m_port);
    ASSERT_TRUE(!!received);
    auto decoder = WTF::move(received.value());
    auto decodedBody = decoder->decode<Vector<uint8_t>>();
    ASSERT_TRUE(!!decodedBody);
    EXPECT_TRUE(*decodedBody == body);
    auto sendRight = decoder->decode<MachSendRight>();
    ASSERT_TRUE(!!sendRight);
    EXPECT_EQ(sendRight->sendRight(), mach_port_t { MACH_PORT_NULL });
}

// A null send right among the attachments that go in an out-of-line array of port names, where a null
// name must not stop the rest of the array from being received either.
TEST_F(MachMessageTest, SendReceiveOutOfLineNullAttachment)
{
    constexpr size_t attachmentCount = IPC::MachMessage::inlinePortDescriptorMaxCount + 1;
    static_assert(attachmentCount > IPC::MachMessage::inlinePortDescriptorMaxCount, "The attachments go in an out-of-line array.");
    auto attachmentSendRights = sendRightCount(m_attachmentPort);

    auto body = makeBody(128);
    auto encoder = makeEncoder(body);
    encoder.get() << MachSendRight { };
    for (size_t i = 1; i < attachmentCount; ++i)
        encoder.get() << MachSendRight::create(m_attachmentPort);
    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    auto received = IPC::MachMessage::receiveDecoder(m_port);
    ASSERT_TRUE(!!received);
    auto decoder = WTF::move(received.value());
    auto decodedBody = decoder->decode<Vector<uint8_t>>();
    ASSERT_TRUE(!!decodedBody);
    EXPECT_TRUE(*decodedBody == body);
    auto nullSendRight = decoder->decode<MachSendRight>();
    ASSERT_TRUE(!!nullSendRight);
    EXPECT_EQ(nullSendRight->sendRight(), mach_port_t { MACH_PORT_NULL });
    for (size_t i = 1; i < attachmentCount; ++i) {
        auto sendRight = decoder->decode<MachSendRight>();
        ASSERT_TRUE(!!sendRight) << "attachment " << i;
        ASSERT_EQ(sendRight->sendRight(), m_attachmentPort) << "attachment " << i;
    }
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// A send that fails because the destination queue is full can be retried once there is room, which is
// what Connection does with the message it holds after MACH_SEND_TIMED_OUT. The kernel returns such a
// message to the sender with the port rights of the message, so the retry has to send the same
// message rather than build it again.
TEST_F(MachMessageTest, SendRetryAfterTimedOutSucceeds)
{
    setQueueLength(m_port, 1);

    auto firstBody = makeBody(128);
    auto [firstResult, firstRetryableMessage] = IPC::MachMessage::sendEncoder(makeEncoder(firstBody), m_port);
    ASSERT_EQ(firstResult, IPC::MachMessage::SendResult::Success);
    ASSERT_FALSE(!!firstRetryableMessage);

    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    {
        // The queue is full, so this send times out.
        auto secondBody = makeBody(256);
        auto secondEncoder = makeEncoder(secondBody);
        secondEncoder.get() << MachSendRight::create(m_attachmentPort);
        auto destinationSendRights = sendRightCount(m_port);
        auto [secondResult, secondRetryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(secondEncoder), m_port);
        ASSERT_EQ(secondResult, IPC::MachMessage::SendResult::Timeout);
        ASSERT_TRUE(!!secondRetryableMessage);
        // The kernel returned the message holding a reference to the destination.
        EXPECT_EQ(sendRightCount(m_port), destinationSendRights + 1);

        // Make room in the queue.
        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto firstDecoder = WTF::move(received.value());
        auto firstDecodedBody = firstDecoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!firstDecodedBody);
        EXPECT_TRUE(*firstDecodedBody == firstBody);

        // The retry sends the message that the timed out send returned.
        ASSERT_EQ(secondRetryableMessage->send(), IPC::MachMessage::SendResult::Success);
        // Sending the message transferred the reference to the destination that the kernel had returned,
        // instead of leaking it and keeping the destination alive.
        EXPECT_EQ(sendRightCount(m_port), destinationSendRights);

        received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto secondDecoder = WTF::move(received.value());
        EXPECT_EQ(secondDecoder->messageName(), testMessageName);
        auto secondDecodedBody = secondDecoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!secondDecodedBody);
        EXPECT_TRUE(*secondDecodedBody == secondBody);
        auto sendRight = secondDecoder->decode<MachSendRight>();
        ASSERT_TRUE(!!sendRight);
        EXPECT_EQ(sendRight->sendRight(), m_attachmentPort);
    }
    // The same for a body too large to send inline. Such a body is sent as an out-of-line descriptor
    // referring to the encoder's buffer, and the kernel copies it out to a mapping of its own when the send
    // fails. The retry has to send that mapping, because the encoder is gone by then.
    {
        auto fillerBody = makeBody(128);
        auto [fillerResult, fillerRetryableMessage] = IPC::MachMessage::sendEncoder(makeEncoder(fillerBody), m_port);
        ASSERT_EQ(fillerResult, IPC::MachMessage::SendResult::Success);
        ASSERT_FALSE(!!fillerRetryableMessage);

        // The queue is full, so this send times out.
        auto largeBody = makeBody(16384);
        auto largeEncoder = makeEncoder(largeBody);
        largeEncoder.get() << MachSendRight::create(m_attachmentPort);
        auto destinationSendRights = sendRightCount(m_port);
        auto [largeResult, largeRetryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(largeEncoder), m_port);
        ASSERT_EQ(largeResult, IPC::MachMessage::SendResult::Timeout);
        ASSERT_TRUE(!!largeRetryableMessage);
        EXPECT_EQ(sendRightCount(m_port), destinationSendRights + 1);

        // Make room in the queue.
        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto fillerDecoder = WTF::move(received.value());
        auto fillerDecodedBody = fillerDecoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!fillerDecodedBody);
        EXPECT_TRUE(*fillerDecodedBody == fillerBody);

        ASSERT_EQ(largeRetryableMessage->send(), IPC::MachMessage::SendResult::Success);
        EXPECT_EQ(sendRightCount(m_port), destinationSendRights);

        received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto largeDecoder = WTF::move(received.value());
        EXPECT_EQ(largeDecoder->messageName(), testMessageName);
        auto largeDecodedBody = largeDecoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!largeDecodedBody);
        // The whole body arrived, so the retry sent the mapping the kernel copied out rather than the
        // encoder's buffer, which no longer exists.
        EXPECT_TRUE(*largeDecodedBody == largeBody);
        auto sendRight = largeDecoder->decode<MachSendRight>();
        ASSERT_TRUE(!!sendRight);
        EXPECT_EQ(sendRight->sendRight(), m_attachmentPort);
    }
    // The attachment rights travelled through the timed out sends and the retries into the received
    // messages, and nothing along the way kept a reference of its own.
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// The same as above, repeated: a reference that the timed out send leaks, or that the retry fails to
// consume, accumulates on the destination or on the attachment port instead of showing up as an
// off-by-one that a single cycle would catch.
TEST_F(MachMessageTest, SendRetryDoesNotLeakSendRightsAcrossManyCycles)
{
    setQueueLength(m_port, 1);

    auto destinationSendRights = sendRightCount(m_port);
    auto attachmentSendRights = sendRightCount(m_attachmentPort);

    auto body = makeBody(128);
    constexpr unsigned iterations = 500;
    for (unsigned i = 0; i < iterations; ++i) {
        // Fill the queue of the destination, so that the next send times out.
        auto [fillerResult, fillerRetryableMessage] = IPC::MachMessage::sendEncoder(makeEncoder({ }), m_port);
        ASSERT_EQ(fillerResult, IPC::MachMessage::SendResult::Success);
        ASSERT_FALSE(!!fillerRetryableMessage);

        auto encoder = makeEncoder(body);
        encoder.get() << MachSendRight::create(m_attachmentPort);
        auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
        ASSERT_EQ(result, IPC::MachMessage::SendResult::Timeout);
        ASSERT_TRUE(!!retryableMessage);

        // Make room in the queue, then send the message that the timed out send returned.
        ASSERT_TRUE(!!IPC::MachMessage::receiveDecoder(m_port));
        ASSERT_EQ(retryableMessage->send(), IPC::MachMessage::SendResult::Success);

        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto decoder = WTF::move(received.value());
        auto decodedBody = decoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!decodedBody);
        auto sendRight = decoder->decode<MachSendRight>();
        ASSERT_TRUE(!!sendRight);
        ASSERT_EQ(sendRight->sendRight(), m_attachmentPort);

        // Everything the cycle took is released again by the end of it, so a leak of even one reference
        // per cycle fails here rather than accumulating silently. The decoded right is still held.
        ASSERT_EQ(sendRightCount(m_port), destinationSendRights) << "iteration " << i;
        ASSERT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + 1) << "iteration " << i;
    }

    EXPECT_EQ(sendRightCount(m_port), destinationSendRights);
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// The retry path with the attachments in an out-of-line array of port names. The kernel copies that array
// out to one of its own when the send times out, so the retry sends a message that names an array this
// process never allocated. Repeated, so that a right leaked per message accumulates into something a single
// cycle could not tell from an off-by-one.
TEST_F(MachMessageTest, SendRetryWithOutOfLineAttachmentsDoesNotLeakSendRights)
{
    constexpr size_t attachmentCount = 1000;
    static_assert(attachmentCount > IPC::MachMessage::inlinePortDescriptorMaxCount, "The attachments go in an out-of-line array.");
    constexpr unsigned iterations = 50;

    setQueueLength(m_port, 1);
    auto body = makeBody(128);
    auto destinationSendRights = sendRightCount(m_port);
    auto attachmentSendRights = sendRightCount(m_attachmentPort);

    for (unsigned i = 0; i < iterations; ++i) {
        // Fill the queue of the destination, so that the next send times out.
        auto [fillerResult, fillerRetryableMessage] = IPC::MachMessage::sendEncoder(makeEncoder({ }), m_port);
        ASSERT_EQ(fillerResult, IPC::MachMessage::SendResult::Success) << "iteration " << i;
        ASSERT_FALSE(!!fillerRetryableMessage) << "iteration " << i;

        auto encoder = makeEncoder(body);
        for (size_t j = 0; j < attachmentCount; ++j)
            encoder.get() << MachSendRight::create(m_attachmentPort);

        auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
        ASSERT_EQ(result, IPC::MachMessage::SendResult::Timeout) << "iteration " << i;
        ASSERT_TRUE(!!retryableMessage) << "iteration " << i;
        // The kernel returned the message holding a reference to the destination, and holding the
        // attachments: the send consumed them and the copy out gave them back.
        ASSERT_EQ(sendRightCount(m_port), destinationSendRights + 1) << "iteration " << i;
        ASSERT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + attachmentCount) << "iteration " << i;

        // Make room in the queue, then send the message that the timed out send returned.
        ASSERT_TRUE(!!IPC::MachMessage::receiveDecoder(m_port)) << "iteration " << i;
        ASSERT_EQ(retryableMessage->send(), IPC::MachMessage::SendResult::Success) << "iteration " << i;
        // Sending the message transferred the reference to the destination that the kernel had returned,
        // instead of leaking it and keeping the destination alive.
        ASSERT_EQ(sendRightCount(m_port), destinationSendRights) << "iteration " << i;

        {
            auto received = IPC::MachMessage::receiveDecoder(m_port);
            ASSERT_TRUE(!!received) << "iteration " << i;
            auto decoder = WTF::move(received.value());
            EXPECT_EQ(decoder->messageName(), testMessageName) << "iteration " << i;
            auto decodedBody = decoder->decode<Vector<uint8_t>>();
            ASSERT_TRUE(!!decodedBody) << "iteration " << i;
            ASSERT_TRUE(*decodedBody == body) << "iteration " << i;
            for (size_t j = 0; j < attachmentCount; ++j) {
                auto sendRight = decoder->decode<MachSendRight>();
                ASSERT_TRUE(!!sendRight) << "iteration " << i << " attachment " << j;
                ASSERT_EQ(sendRight->sendRight(), m_attachmentPort) << "iteration " << i << " attachment " << j;
            }
        }
        // Every right the message carried was released with the message it was decoded from.
        ASSERT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights) << "iteration " << i;
    }

    EXPECT_EQ(sendRightCount(m_port), destinationSendRights);
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// As many attachments as are sent as a port descriptor each, which is the most inline port descriptors
// the kernel takes from a sandboxed process.
TEST_F(MachMessageTest, SendReceiveMaxInlineAttachments)
{
    constexpr size_t attachmentCount = IPC::MachMessage::inlinePortDescriptorMaxCount;
    auto body = makeBody(128);
    auto attachmentSendRights = sendRightCount(m_attachmentPort);

    auto encoder = makeEncoder(body);
    for (size_t i = 0; i < attachmentCount; ++i)
        encoder.get() << MachSendRight::create(m_attachmentPort);

    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    {
        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto decoder = WTF::move(received.value());
        EXPECT_EQ(decoder->messageName(), testMessageName);
        auto decodedBody = decoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!decodedBody);
        EXPECT_TRUE(*decodedBody == body);
        for (size_t i = 0; i < attachmentCount; ++i) {
            auto sendRight = decoder->decode<MachSendRight>();
            ASSERT_TRUE(!!sendRight) << "attachment " << i;
            ASSERT_EQ(sendRight->sendRight(), m_attachmentPort) << "attachment " << i;
        }
    }
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// The attachments are decoded in the order they were encoded. The message holds them in the order the
// decoder takes them from, which is from the end, so they are stored back to front.
TEST_F(MachMessageTest, SendReceiveAttachmentsInEncodedOrder)
{
    mach_port_t otherPort = allocatePortWithSendRight();
    ASSERT_NE(otherPort, m_attachmentPort);

    auto body = makeBody(128);
    auto encoder = makeEncoder(body);
    encoder.get() << MachSendRight::create(m_attachmentPort);
    encoder.get() << MachSendRight::create(otherPort);
    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    {
        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto decoder = WTF::move(received.value());
        auto decodedBody = decoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!decodedBody);
        EXPECT_TRUE(*decodedBody == body);
        auto first = decoder->decode<MachSendRight>();
        ASSERT_TRUE(!!first);
        EXPECT_EQ(first->sendRight(), m_attachmentPort);
        auto second = decoder->decode<MachSendRight>();
        ASSERT_TRUE(!!second);
        EXPECT_EQ(second->sendRight(), otherPort);
    }
    deallocatePort(otherPort);
}

// More attachments than fit as a port descriptor each are sent as an out-of-line array of port names, so
// that the message stays small however many there are.
TEST_F(MachMessageTest, SendReceiveOutOfLineAttachments)
{
    constexpr size_t attachmentCount = IPC::MachMessage::inlinePortDescriptorMaxCount + 1;
    auto body = makeBody(128);
    // The attachments alternate between two ports, so that they are decoded in the order they were
    // encoded rather than merely all being there.
    mach_port_t otherPort = allocatePortWithSendRight();
    ASSERT_NE(otherPort, m_attachmentPort);
    auto portFor = [&](size_t i) {
        return i % 2 ? otherPort : m_attachmentPort;
    };
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    auto otherSendRights = sendRightCount(otherPort);

    auto encoder = makeEncoder(body);
    for (size_t i = 0; i < attachmentCount; ++i)
        encoder.get() << MachSendRight::create(portFor(i));

    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    {
        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto decoder = WTF::move(received.value());
        EXPECT_EQ(decoder->messageName(), testMessageName);
        auto decodedBody = decoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!decodedBody);
        EXPECT_TRUE(*decodedBody == body);
        for (size_t i = 0; i < attachmentCount; ++i) {
            auto sendRight = decoder->decode<MachSendRight>();
            ASSERT_TRUE(!!sendRight) << "attachment " << i;
            ASSERT_EQ(sendRight->sendRight(), portFor(i)) << "attachment " << i;
        }
    }
    // Every right the message carried was released with the message it was decoded from.
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
    EXPECT_EQ(sendRightCount(otherPort), otherSendRights);
    deallocatePort(otherPort);
}

// As many attachments as the kernel takes at all, which is far more than fit as a port descriptor each.
TEST_F(MachMessageTest, SendReceiveMaxAttachments)
{
    constexpr size_t attachmentCount = IPC::MachMessage::maxAttachmentCount;
    auto attachmentSendRights = sendRightCount(m_attachmentPort);

    auto encoder = makeEncoder(makeBody(128));
    for (size_t i = 0; i < attachmentCount; ++i)
        encoder.get() << MachSendRight::create(m_attachmentPort);

    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    ASSERT_EQ(result, IPC::MachMessage::SendResult::Success);
    EXPECT_FALSE(!!retryableMessage);

    {
        auto received = IPC::MachMessage::receiveDecoder(m_port);
        ASSERT_TRUE(!!received);
        auto decoder = WTF::move(received.value());
        auto decodedBody = decoder->decode<Vector<uint8_t>>();
        ASSERT_TRUE(!!decodedBody);
        size_t decodedCount = 0;
        for (; decodedCount < attachmentCount; ++decodedCount) {
            auto sendRight = decoder->decode<MachSendRight>();
            if (!sendRight)
                break;
            if (sendRight->sendRight() != m_attachmentPort)
                break;
        }
        EXPECT_EQ(decodedCount, attachmentCount);
    }
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// The platform takes at most IPC_KMSG_MAX_OOL_PORT_COUNT (16383) ports in one message, whether they are
// sent as a port descriptor each or as an out-of-line array. A message with more than that cannot be sent
// at all, and there is no fallback that would make it smaller.
TEST_F(MachMessageTest, SendTooManyAttachmentsFails)
{
    constexpr size_t attachmentCount = IPC::MachMessage::maxAttachmentCount + 1;
    auto attachmentSendRights = sendRightCount(m_attachmentPort);

    auto encoder = makeEncoder(makeBody(128));
    for (size_t i = 0; i < attachmentCount; ++i)
        encoder.get() << MachSendRight::create(m_attachmentPort);
    ASSERT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + attachmentCount);

    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), m_port);
    EXPECT_EQ(result, IPC::MachMessage::SendResult::InvalidEncoder);
    EXPECT_FALSE(!!retryableMessage);
    // Nothing was sent, so nothing arrives.
    EXPECT_FALSE(!!IPC::MachMessage::receiveDecoder(m_port));
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// The messages below are built by hand to reach each way receiveDecoder() refuses a message. They are what a
// sender that is not this code, or one that has been compromised, could put on the port, so none of them
// may assert, crash or leak.

// A message too large for the buffer is discarded by the kernel, because MACH_RCV_LARGE is not requested.
TEST_F(MachMessageTest, ReceiveOversizedMessageIsInvalid)
{
    RawMessage message;
    message.grow(4096 + 500);
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
}

// A complex message has to have descriptors.
TEST_F(MachMessageTest, ReceiveComplexMessageWithoutDescriptorsIsInvalid)
{
    RawMessage message;
    message.setDescriptorCount(0);
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
}

// An attachment has to be a send right. A send-once right is not one.
TEST_F(MachMessageTest, ReceiveAttachmentThatIsNotASendRightIsInvalid)
{
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    RawMessage message;
    message.setDescriptorCount(1);
    mach_msg_port_descriptor_t portDescriptor { };
    portDescriptor.name = m_attachmentPort;
    portDescriptor.disposition = MACH_MSG_TYPE_MAKE_SEND_ONCE;
    portDescriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    message.append(portDescriptor);
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// The same, for the attachments of an out-of-line array.
TEST_F(MachMessageTest, ReceiveOutOfLineAttachmentsThatAreNotSendRightsIsInvalid)
{
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    std::array<mach_port_name_t, 1> names { m_attachmentPort };
    RawMessage message;
    message.setDescriptorCount(1);
    mach_msg_ool_ports_descriptor_t oolPortsDescriptor { };
    oolPortsDescriptor.address = names.data();
    oolPortsDescriptor.count = names.size();
    oolPortsDescriptor.deallocate = false;
    oolPortsDescriptor.copy = MACH_MSG_PHYSICAL_COPY;
    oolPortsDescriptor.disposition = MACH_MSG_TYPE_MAKE_SEND_ONCE;
    oolPortsDescriptor.type = MACH_MSG_OOL_PORTS_DESCRIPTOR;
    message.append(oolPortsDescriptor);
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// An out-of-line body has to have something in it.
TEST_F(MachMessageTest, ReceiveEmptyOutOfLineBodyIsInvalid)
{
    RawMessage message;
    message.setDescriptorCount(1);
    mach_msg_ool_descriptor_t oolDescriptor { };
    oolDescriptor.address = nullptr;
    oolDescriptor.size = 0;
    oolDescriptor.deallocate = false;
    oolDescriptor.copy = MACH_MSG_VIRTUAL_COPY;
    oolDescriptor.type = MACH_MSG_OOL_DESCRIPTOR;
    message.append(oolDescriptor);
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
}

// Only one descriptor can follow the attachments, and it has to be the body.
TEST_F(MachMessageTest, ReceiveTwoOutOfLineBodiesIsInvalid)
{
    auto body = makeBody(128);
    RawMessage message;
    message.setDescriptorCount(2);
    for (unsigned i = 0; i < 2; ++i) {
        mach_msg_ool_descriptor_t oolDescriptor { };
        oolDescriptor.address = const_cast<uint8_t*>(body.span().data());
        oolDescriptor.size = static_cast<mach_msg_size_t>(body.size());
        oolDescriptor.deallocate = false;
        oolDescriptor.copy = MACH_MSG_VIRTUAL_COPY;
        oolDescriptor.type = MACH_MSG_OOL_DESCRIPTOR;
        message.append(oolDescriptor);
    }
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
}

// The descriptor that follows the attachments has to be a memory descriptor, not more attachments.
TEST_F(MachMessageTest, ReceiveOutOfLineAttachmentsAfterInlineOnesIsInvalid)
{
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    std::array<mach_port_name_t, 1> names { m_attachmentPort };
    RawMessage message;
    message.setDescriptorCount(2);
    mach_msg_port_descriptor_t portDescriptor { };
    portDescriptor.name = m_attachmentPort;
    portDescriptor.disposition = MACH_MSG_TYPE_COPY_SEND;
    portDescriptor.type = MACH_MSG_PORT_DESCRIPTOR;
    message.append(portDescriptor);
    mach_msg_ool_ports_descriptor_t oolPortsDescriptor { };
    oolPortsDescriptor.address = names.data();
    oolPortsDescriptor.count = names.size();
    oolPortsDescriptor.deallocate = false;
    oolPortsDescriptor.copy = MACH_MSG_PHYSICAL_COPY;
    oolPortsDescriptor.disposition = MACH_MSG_TYPE_COPY_SEND;
    oolPortsDescriptor.type = MACH_MSG_OOL_PORTS_DESCRIPTOR;
    message.append(oolPortsDescriptor);
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// A body the decoder cannot read is not a message.
TEST_F(MachMessageTest, ReceiveBodyTooShortToDecodeIsInvalid)
{
    RawMessage message;
    message.grow(4);
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    expectInvalidMessage();
}

// A message with an ID of its own is a notification rather than something to decode.
TEST_F(MachMessageTest, ReceiveUnknownMessageIDIsNotification)
{
    RawMessage message { webkitMachMessageID + 1 };
    ASSERT_EQ(message.send(m_port), KERN_SUCCESS);
    auto received = IPC::MachMessage::receiveDecoder(m_port);
    ASSERT_FALSE(!!received);
    EXPECT_EQ(received.error(), IPC::MachMessage::ReceiveResult::Notification);
}

// A send to a port whose receive right is gone fails. The message is dropped as it was built, which
// owns the port rights of its attachments but only names the destination and the body of the encoder,
// so exactly the attachments have to be released.
TEST_F(MachMessageTest, SendToDeadPortFails)
{
    auto body = makeBody(128);
    mach_port_t deadPort = allocatePortWithSendRight();
    mach_port_mod_refs(mach_task_self(), deadPort, MACH_PORT_RIGHT_RECEIVE, -1);
    auto deadPortSendRights = sendRightCount(deadPort);
    auto attachmentSendRights = sendRightCount(m_attachmentPort);

    auto encoder = makeEncoder(body);
    encoder.get() << MachSendRight::create(m_attachmentPort);
    auto [result, retryableMessage] = IPC::MachMessage::sendEncoder(WTF::move(encoder), deadPort);
    EXPECT_EQ(result, IPC::MachMessage::SendResult::InvalidDestinationPort);
    EXPECT_FALSE(!!retryableMessage);

    // The attachment right was the message's, so dropping the message released it.
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
    // The destination right was the sender's, so dropping the message left it alone.
    EXPECT_EQ(sendRightCount(deadPort), deadPortSendRights);
    mach_port_deallocate(mach_task_self(), deadPort);
}

// Proof of XNU behavior:
// The kernel sets mach_msg_ool_ports_descriptor_t::deallocate == true for received and pseudo-received
// messages, whatever the sender asked for. Proves that MachMessage::receiveDecoder implementation does not
// leak mach_msg_ool_ports_descriptor_t vm allocations, and that it must not deallocate the array itself.
TEST_F(MachMessageTest, XNUOutOfLinePortsDescriptorIsAlwaysCopiedOutAsVirtualCopyToDeallocate)
{
    constexpr mach_msg_size_t nameCount = 4;
    for (auto senderCopy : { static_cast<mach_msg_copy_options_t>(MACH_MSG_VIRTUAL_COPY), static_cast<mach_msg_copy_options_t>(MACH_MSG_PHYSICAL_COPY) }) {
        for (auto senderDeallocate : { false, true }) {
            for (auto pseudoReceive : { false, true }) {
                SCOPED_TRACE(::testing::Message() << "copy " << senderCopy << " deallocate " << senderDeallocate << " pseudo receive " << pseudoReceive);
                auto port = allocatePortWithSendRight();

                vm_address_t buffer = 0;
                ASSERT_EQ(vm_allocate(mach_task_self(), &buffer, vm_page_size, VM_FLAGS_ANYWHERE), KERN_SUCCESS);
                RawMessage message;
                message.setDescriptorCount(1);
                mach_msg_ool_ports_descriptor_t descriptor { };
                descriptor.address = reinterpret_cast<void*>(buffer);
                descriptor.count = nameCount;
                descriptor.deallocate = senderDeallocate;
                descriptor.copy = senderCopy;
                descriptor.disposition = MACH_MSG_TYPE_MOVE_SEND;
                descriptor.type = MACH_MSG_OOL_PORTS_DESCRIPTOR;
                message.append(descriptor);

                // Either way what the kernel copied out ends up in this message's own storage: a pseudo
                // receive leaves it there, and a receive comes back over it.
                if (pseudoReceive) {
                    setQueueLength(port, MACH_PORT_QLIMIT_ZERO);
                    ASSERT_EQ(message.sendNoBlock(port), MACH_SEND_TIMED_OUT);
                } else {
                    ASSERT_EQ(message.send(port), KERN_SUCCESS);
                    EXPECT_EQ(senderDeallocate, !isMapped(reinterpret_cast<void*>(buffer)));
                    ASSERT_EQ(message.receive(port), KERN_SUCCESS);
                }

                if (!senderDeallocate)
                    EXPECT_EQ(vm_deallocate(mach_task_self(), buffer, vm_page_size), KERN_SUCCESS);

                // This is what is being tested: mach_msg_ool_ports_descriptor_t::deallocate == true
                // always.
                auto& copiedOutDescriptor = message.descriptor<mach_msg_ool_ports_descriptor_t>();
                EXPECT_EQ(copiedOutDescriptor.count, nameCount);
                EXPECT_EQ(static_cast<mach_msg_copy_options_t>(copiedOutDescriptor.copy), static_cast<mach_msg_copy_options_t>(MACH_MSG_VIRTUAL_COPY));
                EXPECT_TRUE(copiedOutDescriptor.deallocate);
                auto* address = copiedOutDescriptor.address;
                ASSERT_TRUE(isMapped(address));
                message.destroy();
                EXPECT_FALSE(isMapped(address));

                deallocatePort(port);
            }
        }
    }
}

// Proof of XNU behavior:
// The kernel DOES NOT set mach_msg_ool_descriptor_t::deallocate == true for received and pseudo-received
// messages iff copy == MACH_MSG_PHYSICAL_COPY.
// Proves that MachMessage::receiveDecoder implementation does not leak vm allocations in normal case, because
// send is copy == MACH_MSG_VIRTUAL_COPY.
// Proves that MachMessage::receiveDecoder implementation currently leaks vm allocations in case malicious sender sends
// copy == MACH_MSG_PHYSICAL_COPY.
TEST_F(MachMessageTest, XNUOutOfLineBodyDescriptorDeallocateFollowsTheSenderCopyOption)
{
    constexpr mach_msg_size_t bodySize = 16384;
    for (auto senderCopy : { static_cast<mach_msg_copy_options_t>(MACH_MSG_VIRTUAL_COPY), static_cast<mach_msg_copy_options_t>(MACH_MSG_PHYSICAL_COPY) }) {
        for (auto senderDeallocate : { false, true }) {
            for (auto pseudoReceive : { false, true }) {
                SCOPED_TRACE(::testing::Message() << "copy " << senderCopy << " deallocate " << senderDeallocate << " pseudo receive " << pseudoReceive);
                auto port = allocatePortWithSendRight();

                vm_address_t buffer = 0;
                ASSERT_EQ(vm_allocate(mach_task_self(), &buffer, bodySize, VM_FLAGS_ANYWHERE), KERN_SUCCESS);
                RawMessage message;
                message.setDescriptorCount(1);
                mach_msg_ool_descriptor_t descriptor { };
                descriptor.address = reinterpret_cast<void*>(buffer);
                descriptor.size = bodySize;
                descriptor.deallocate = senderDeallocate;
                descriptor.copy = senderCopy;
                descriptor.type = MACH_MSG_OOL_DESCRIPTOR;
                message.append(descriptor);

                if (pseudoReceive) {
                    setQueueLength(port, MACH_PORT_QLIMIT_ZERO);
                    ASSERT_EQ(message.sendNoBlock(port), MACH_SEND_TIMED_OUT);
                } else {
                    ASSERT_EQ(message.send(port), KERN_SUCCESS);
                    EXPECT_EQ(senderDeallocate, !isMapped(reinterpret_cast<void*>(buffer)));
                    ASSERT_EQ(message.receive(port), KERN_SUCCESS);
                }

                if (!senderDeallocate)
                    EXPECT_EQ(vm_deallocate(mach_task_self(), buffer, bodySize), KERN_SUCCESS);

                // This is being tested: iff copy == MACH_MSG_PHYSICAL_COPY, deallocate does not get set.
                // This causes memory leaks if used. MachMessage::send does not use it.
                auto& copiedOutDescriptor = message.descriptor<mach_msg_ool_descriptor_t>();
                EXPECT_EQ(copiedOutDescriptor.size, bodySize);
                EXPECT_EQ(static_cast<mach_msg_copy_options_t>(copiedOutDescriptor.copy), senderCopy);
                bool senderAskedForVirtualCopy = senderCopy == static_cast<mach_msg_copy_options_t>(MACH_MSG_VIRTUAL_COPY);
                // deallocate follows the copy option, whatever the sender asked to have deallocated.
                EXPECT_EQ(!!copiedOutDescriptor.deallocate, senderAskedForVirtualCopy);
                auto* address = copiedOutDescriptor.address;
                ASSERT_TRUE(isMapped(address));
                message.destroy();
                EXPECT_EQ(isMapped(address), !senderAskedForVirtualCopy);
                // The mapping a physical copy leaves behind is this test's to release, as it is the leak.
                if (isMapped(address))
                    EXPECT_EQ(vm_deallocate(mach_task_self(), reinterpret_cast<vm_address_t>(address), bodySize), KERN_SUCCESS);

                deallocatePort(port);
            }
        }
    }
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
