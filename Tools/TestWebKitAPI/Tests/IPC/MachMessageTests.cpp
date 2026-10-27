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
#include "Encoder.h"
#include "Helpers/Test.h"
#include "MachMessage.h"
#include "MessageNames.h"
#include <mach/mach.h>
#include <wtf/MachSendRight.h>
#include <wtf/UniqueRef.h>
#include <wtf/Vector.h>
#include <wtf/spi/cocoa/MachVMSPI.h>

namespace TestWebKitAPI {

namespace {

// Sends MachMessages to a port this process receives on, and receives them by hand, so that a test can check what
// the kernel delivered and which rights and memory a message still owns.
class MachMessageTest : public ::testing::Test {
public:
    void SetUp() override
    {
        m_port = allocatePortWithSendRight();
        m_attachmentPort = allocatePortWithSendRight();
    }

    void TearDown() override
    {
        while (receive(m_port)) { }
        deallocatePort(m_port);
        deallocatePort(m_attachmentPort);
    }

protected:
    struct ReceivedMessage {
        mach_msg_id_t messageID { 0 };
        Vector<uint8_t> body;
        size_t portCount { 0 };
    };

    static mach_port_t allocatePortWithSendRight()
    {
        mach_port_t port = MACH_PORT_NULL;
        EXPECT_EQ(mach_port_allocate(mach_task_self(), MACH_PORT_RIGHT_RECEIVE, &port), KERN_SUCCESS);
        EXPECT_EQ(mach_port_insert_right(mach_task_self(), port, port, MACH_MSG_TYPE_MAKE_SEND), KERN_SUCCESS);
        return port;
    }

    static void deallocatePort(mach_port_t port)
    {
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

    static bool isMapped(const void* address)
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

    // An encoder with a body of a bit more than `bodySize` bytes and `attachmentCount` send rights to m_attachmentPort.
    UniqueRef<IPC::Encoder> makeEncoder(size_t bodySize, size_t attachmentCount)
    {
        auto encoder = makeUniqueRef<IPC::Encoder>(IPC::MessageName::IPCTester_EmptyMessage, 77);
        encoder.get() << Vector<uint8_t>(bodySize, [](size_t i) {
            return static_cast<uint8_t>(i * 7 + 1);
        });
        for (size_t i = 0; i < attachmentCount; ++i)
            encoder->addAttachment(MachSendRight::create(m_attachmentPort));
        return encoder;
    }

    // Receives a message without waiting, and releases the rights and memory it carried.
    std::optional<ReceivedMessage> receive(mach_port_t port)
    {
        alignas(mach_msg_header_t) std::array<uint8_t, IPC::MachMessage::inlineMessageMaxSize + MAX_TRAILER_SIZE> storage { };
        auto& header = reinterpretCastSpanStartTo<mach_msg_header_t>(std::span { storage });
        if (mach_msg(&header, MACH_RCV_MSG | MACH_RCV_TIMEOUT, 0, storage.size(), port, 0, MACH_PORT_NULL) != MACH_MSG_SUCCESS)
            return std::nullopt;

        ReceivedMessage message { header.msgh_id, { }, 0 };
        auto remaining = std::span { storage }.first(header.msgh_size).subspan(sizeof(mach_msg_header_t));
        mach_port_t sharedMemoryPort = MACH_PORT_NULL;
        if (header.msgh_bits & MACH_MSGH_BITS_COMPLEX) {
            auto descriptorCount = consumeAndReinterpretCastTo<mach_msg_body_t>(remaining).msgh_descriptor_count;
            for (mach_msg_size_t i = 0; i < descriptorCount; ++i) {
                if (reinterpretCastSpanStartTo<mach_msg_type_descriptor_t>(remaining).type == MACH_MSG_PORT_DESCRIPTOR) {
                    auto& descriptor = consumeAndReinterpretCastTo<mach_msg_port_descriptor_t>(remaining);
                    // The last port descriptor of a shared memory message is its body.
                    if (header.msgh_id == IPC::MachMessage::sharedMemoryBodyMessageID && i == descriptorCount - 1) {
                        sharedMemoryPort = descriptor.name;
                        continue;
                    }
                    // A send right to a port this task has rights to arrives with the same name.
                    EXPECT_EQ(descriptor.name, m_attachmentPort);
                    mach_port_deallocate(mach_task_self(), descriptor.name);
                    ++message.portCount;
                    continue;
                }
                auto& descriptor = consumeAndReinterpretCastTo<mach_msg_ool_descriptor_t>(remaining);
                message.body.append(unsafeMakeSpan(static_cast<const uint8_t*>(descriptor.address), descriptor.size));
                mach_vm_deallocate(mach_task_self(), reinterpret_cast<mach_vm_address_t>(descriptor.address), descriptor.size);
            }
        }
        if (sharedMemoryPort != MACH_PORT_NULL) {
            auto size = consumeAndReinterpretCastTo<uint64_t>(remaining);
            mach_vm_address_t address = 0;
            EXPECT_EQ(mach_vm_map(mach_task_self(), &address, size, 0, VM_FLAGS_ANYWHERE | VM_FLAGS_RETURN_DATA_ADDR, sharedMemoryPort, 0, false, VM_PROT_READ, VM_PROT_READ, VM_INHERIT_NONE), KERN_SUCCESS);
            if (address) {
                message.body.append(unsafeMakeSpan(reinterpret_cast<const uint8_t*>(address), size));
                mach_vm_deallocate(mach_task_self(), address, size);
            }
            mach_port_deallocate(mach_task_self(), sharedMemoryPort);
        }
        if (message.body.isEmpty())
            message.body.append(remaining);
        return message;
    }

    // The body of an inline message is padded to a multiple of 4 bytes.
    static void expectBody(const ReceivedMessage& message, std::span<const uint8_t> expectedBody)
    {
        ASSERT_GE(message.body.size(), expectedBody.size());
        EXPECT_LT(message.body.size(), expectedBody.size() + 4);
        EXPECT_TRUE(equalSpans(message.body.span().first(expectedBody.size()), expectedBody));
    }

    static void fillQueueOfLengthOne(mach_port_t port)
    {
        setQueueLength(port, 1);
        auto filler = makeUniqueRef<IPC::Encoder>(IPC::MessageName::IPCTester_EmptyMessage, 1);
        EXPECT_EQ(IPC::MachMessage::create(WTF::move(filler))->send(port), MACH_MSG_SUCCESS);
    }

    mach_port_t m_port { MACH_PORT_NULL };
    mach_port_t m_attachmentPort { MACH_PORT_NULL };
};

} // namespace

TEST_F(MachMessageTest, InlineBodyAndAttachments)
{
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    auto encoder = makeEncoder(100, 2);
    auto expectedBody = Vector<uint8_t>(encoder->span());

    auto message = IPC::MachMessage::create(WTF::move(encoder));
    ASSERT_TRUE(!!message);
    EXPECT_EQ(message->header()->msgh_id, IPC::MachMessage::inlineBodyMessageID);
    EXPECT_EQ(message->send(m_port), MACH_MSG_SUCCESS);
    message = nullptr;

    auto received = receive(m_port);
    ASSERT_TRUE(!!received);
    EXPECT_EQ(received->messageID, IPC::MachMessage::inlineBodyMessageID);
    EXPECT_EQ(received->portCount, 2u);
    expectBody(*received, expectedBody.span());
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

TEST_F(MachMessageTest, OutOfLineBody)
{
    auto encoder = makeEncoder(64 * 1024, 0);
    auto expectedBody = Vector<uint8_t>(encoder->span());

    auto message = IPC::MachMessage::create(WTF::move(encoder));
    ASSERT_TRUE(!!message);
    EXPECT_EQ(message->header()->msgh_id, IPC::MachMessage::outOfLineBodyMessageID);
    EXPECT_EQ(message->send(m_port), MACH_MSG_SUCCESS);
    message = nullptr;

    auto received = receive(m_port);
    ASSERT_TRUE(!!received);
    EXPECT_EQ(received->messageID, IPC::MachMessage::outOfLineBodyMessageID);
    EXPECT_EQ(received->body, expectedBody);
}

TEST_F(MachMessageTest, SharedMemoryBody)
{
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    auto encoder = makeEncoder(64 * 1024, 2);
    auto expectedBody = Vector<uint8_t>(encoder->span());

    auto message = IPC::MachMessage::createWithSharedMemoryBody(IPC::MachMessage::create(WTF::move(encoder)));
    ASSERT_TRUE(!!message);
    EXPECT_EQ(message->header()->msgh_id, IPC::MachMessage::sharedMemoryBodyMessageID);
    // The new message took the attachments of the message it was built from.
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + 2);
    EXPECT_EQ(message->send(m_port), MACH_MSG_SUCCESS);
    message = nullptr;

    auto received = receive(m_port);
    ASSERT_TRUE(!!received);
    EXPECT_EQ(received->messageID, IPC::MachMessage::sharedMemoryBodyMessageID);
    EXPECT_EQ(received->portCount, 2u);
    EXPECT_EQ(received->body, expectedBody);
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

TEST_F(MachMessageTest, DestroyingUnsentSharedMemoryMessageReleasesWhatItOwns)
{
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    auto message = IPC::MachMessage::createWithSharedMemoryBody(IPC::MachMessage::create(makeEncoder(100, 2)));
    ASSERT_TRUE(!!message);
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + 2);
    message = nullptr;
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

// A send that times out because the destination's queue is full returns the message, which owns its attachments and
// a reference to the destination again, and can be sent as it is once there is room.
TEST_F(MachMessageTest, MessageThatTimedOutCanBeSentAgain)
{
    for (size_t bodySize : { 100, 64 * 1024 }) {
        SCOPED_TRACE(bodySize);
        fillQueueOfLengthOne(m_port);
        auto attachmentSendRights = sendRightCount(m_attachmentPort);
        auto destinationSendRights = sendRightCount(m_port);
        auto encoder = makeEncoder(bodySize, 1);
        auto expectedBody = Vector<uint8_t>(encoder->span());

        auto message = IPC::MachMessage::create(WTF::move(encoder));
        ASSERT_TRUE(!!message);
        EXPECT_FALSE(message->wasReturnedFromSend());
        EXPECT_EQ(message->send(m_port), MACH_SEND_TIMED_OUT);
        EXPECT_TRUE(message->wasReturnedFromSend());
        EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + 1);
        EXPECT_EQ(sendRightCount(m_port), destinationSendRights + 1);

        // Sending it again while there still isn't room returns it again, still owning one reference to the destination.
        EXPECT_EQ(message->send(m_port), MACH_SEND_TIMED_OUT);
        EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + 1);
        EXPECT_EQ(sendRightCount(m_port), destinationSendRights + 1);

        // Make room, and send the message again. It uses the reference to the destination it owns.
        ASSERT_TRUE(!!receive(m_port));
        EXPECT_EQ(message->send(m_port), MACH_MSG_SUCCESS);
        message = nullptr;
        EXPECT_EQ(sendRightCount(m_port), destinationSendRights);

        auto received = receive(m_port);
        ASSERT_TRUE(!!received);
        EXPECT_EQ(received->portCount, 1u);
        expectBody(*received, expectedBody.span());
        EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
    }
}

TEST_F(MachMessageTest, DestroyingUnsentMessageReleasesWhatItOwns)
{
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    auto message = IPC::MachMessage::create(makeEncoder(100, 2));
    ASSERT_TRUE(!!message);
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + 2);
    message = nullptr;
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

TEST_F(MachMessageTest, DestroyingMessageThatTimedOutReleasesWhatItOwns)
{
    fillQueueOfLengthOne(m_port);
    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    auto destinationSendRights = sendRightCount(m_port);

    // A message without attachments, so that its out-of-line body is its only descriptor.
    auto message = IPC::MachMessage::create(makeEncoder(64 * 1024, 0));
    ASSERT_TRUE(!!message);
    EXPECT_EQ(message->send(m_port), MACH_SEND_TIMED_OUT);
    auto descriptors = message->span().subspan(sizeof(mach_msg_header_t) + sizeof(mach_msg_body_t));
    auto& bodyDescriptor = reinterpretCastSpanStartTo<mach_msg_ool_descriptor_t>(descriptors);
    EXPECT_TRUE(bodyDescriptor.deallocate);
    auto* returnedBody = bodyDescriptor.address;
    EXPECT_TRUE(isMapped(returnedBody));
    EXPECT_EQ(sendRightCount(m_port), destinationSendRights + 1);

    message = nullptr;
    EXPECT_FALSE(isMapped(returnedBody));
    EXPECT_EQ(sendRightCount(m_port), destinationSendRights);
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);
}

TEST_F(MachMessageTest, SendToDeadDestinationLeavesMessageUnchanged)
{
    auto deadPort = allocatePortWithSendRight();
    mach_port_mod_refs(mach_task_self(), deadPort, MACH_PORT_RIGHT_RECEIVE, -1);

    auto attachmentSendRights = sendRightCount(m_attachmentPort);
    auto message = IPC::MachMessage::create(makeEncoder(100, 1));
    ASSERT_TRUE(!!message);
    EXPECT_EQ(message->send(deadPort), MACH_SEND_INVALID_DEST);
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights + 1);
    message = nullptr;
    EXPECT_EQ(sendRightCount(m_attachmentPort), attachmentSendRights);

    mach_port_deallocate(mach_task_self(), deadPort);
}

} // namespace TestWebKitAPI

#endif // PLATFORM(COCOA)
