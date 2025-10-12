/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2011 Igalia S.L.
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
#include "Connection.h"

#include "IPCUtilities.h"
#include "Logging.h"
#include "UnixMessage.h"
#include <WebCore/SharedMemory.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <wtf/Assertions.h>
#include <wtf/MallocSpan.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/UniStdExtras.h>

#if OS(DARWIN)
#define MSG_NOSIGNAL 0
#endif

// Although it's available on Darwin, SOCK_SEQPACKET seems to work differently
// than in traditional Unix so fallback to DGRAM on that platform.
#if defined(SOCK_SEQPACKET) && !OS(DARWIN)
#define SOCKET_TYPE SOCK_SEQPACKET
#else
#define SOCKET_TYPE SOCK_DGRAM
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // Unix port

namespace IPC {

static const size_t messageMaxSize = 4096;
static const size_t attachmentMaxAmount = 254;

class AttachmentInfo {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(AttachmentInfo);
public:
    AttachmentInfo()
    {
        // The entire AttachmentInfo is passed to write(), so we have to zero our
        // padding bytes to avoid writing uninitialized memory.
        zeroBytes(*this);
    }

    AttachmentInfo(const AttachmentInfo& info)
        : AttachmentInfo()
    {
        *this = info;
    }

    AttachmentInfo& operator=(const AttachmentInfo&) = default;

    // The attachment is not null unless explicitly set.
    void setNull() { m_isNull = true; }
    bool isNull() const { return m_isNull; }

private:
    // The AttachmentInfo will be copied using memcpy, so all members must be trivially copyable.
    bool m_isNull;
};

static_assert(sizeof(MessageInfo) + sizeof(AttachmentInfo) * attachmentMaxAmount <= messageMaxSize, "messageMaxSize is too small.");

int Connection::socketDescriptor() const
{
    return m_socketDescriptor.value();
}

void Connection::platformInitialize(Identifier&& identifier)
{
    m_socketDescriptor = WTFMove(identifier.handle);
    m_readBuffer.reserveInitialCapacity(messageMaxSize);
    m_fileDescriptors.reserveInitialCapacity(attachmentMaxAmount);
}

void Connection::platformInvalidate()
{
    if (m_socketDescriptor.value() != -1)
        closeWithRetry(m_socketDescriptor.release());

    if (!m_isConnected)
        return;

#if PLATFORM(PLAYSTATION)
    if (m_socketMonitor) {
        m_socketMonitor->detach();
        m_socketMonitor = nullptr;
    }
#endif

    m_isConnected = false;
}

bool Connection::processMessage()
{
    if (m_readBuffer.size() < sizeof(MessageInfo))
        return false;

    auto messageData = m_readBuffer.mutableSpan();
    MessageInfo messageInfo;
    memcpySpan(asMutableByteSpan(messageInfo), consumeSpan(messageData, sizeof(messageInfo)));

    if (messageInfo.attachmentCount() > attachmentMaxAmount || (!messageInfo.isBodyOutOfLine() && messageInfo.bodySize() > messageMaxSize)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    size_t messageLength = sizeof(MessageInfo) + messageInfo.attachmentCount() * sizeof(AttachmentInfo) + (messageInfo.isBodyOutOfLine() ? 0 : messageInfo.bodySize());
    if (m_readBuffer.size() < messageLength)
        return false;

    size_t attachmentFileDescriptorCount = 0;
    size_t attachmentCount = messageInfo.attachmentCount();
    Vector<AttachmentInfo> attachmentInfo(attachmentCount);

    if (attachmentCount) {
        memcpySpan(asMutableByteSpan(attachmentInfo.mutableSpan()), consumeSpan(messageData, sizeof(AttachmentInfo) * attachmentCount));
        for (size_t i = 0; i < attachmentCount; ++i) {
            if (!attachmentInfo[i].isNull())
                attachmentFileDescriptorCount++;
        }

        if (messageInfo.isBodyOutOfLine())
            attachmentCount--;
    }

    Vector<Attachment> attachments(attachmentCount);
    RefPtr<WebCore::SharedMemory> oolMessageBody;

    size_t fdIndex = 0;
    for (size_t i = 0; i < attachmentCount; ++i) {
        int fd = !attachmentInfo[i].isNull() ? m_fileDescriptors[fdIndex++] : -1;
        attachments[attachmentCount - i - 1] = UnixFileDescriptor { fd, UnixFileDescriptor::Adopt };
    }

    if (messageInfo.isBodyOutOfLine()) {
        ASSERT(messageInfo.bodySize());

        if (attachmentInfo[attachmentCount].isNull()) {
            ASSERT_NOT_REACHED();
            return false;
        }

        auto fd = UnixFileDescriptor { m_fileDescriptors[attachmentFileDescriptorCount - 1], UnixFileDescriptor::Adopt };
        if (!fd) {
            ASSERT_NOT_REACHED();
            return false;
        }

        auto handle = WebCore::SharedMemory::Handle { WTFMove(fd), messageInfo.bodySize() };
        oolMessageBody = WebCore::SharedMemory::map(WTFMove(handle), WebCore::SharedMemory::Protection::ReadOnly);
        if (!oolMessageBody) {
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    ASSERT(attachments.size() == (messageInfo.isBodyOutOfLine() ? messageInfo.attachmentCount() - 1 : messageInfo.attachmentCount()));

    auto messageBody = messageData;
    if (messageInfo.isBodyOutOfLine())
        messageBody = oolMessageBody->mutableSpan();

    auto decoder = Decoder::create(messageBody.first(messageInfo.bodySize()), WTFMove(attachments));
    ASSERT(decoder);
    if (!decoder)
        return false;

    processIncomingMessage(makeUniqueRefFromNonNullUniquePtr(WTFMove(decoder)));

    if (m_readBuffer.size() > messageLength) {
        memmoveSpan(m_readBuffer.mutableSpan(), m_readBuffer.subspan(messageLength));
        m_readBuffer.shrink(m_readBuffer.size() - messageLength);
    } else
        m_readBuffer.shrink(0);

    if (attachmentFileDescriptorCount) {
        if (m_fileDescriptors.size() > attachmentFileDescriptorCount) {
            memmoveSpan(m_fileDescriptors.mutableSpan(), m_fileDescriptors.subspan(attachmentFileDescriptorCount));
            m_fileDescriptors.shrink(m_fileDescriptors.size() - attachmentFileDescriptorCount);
        } else
            m_fileDescriptors.shrink(0);
    }


    return true;
}

static ssize_t readBytesFromSocket(int socketDescriptor, Vector<uint8_t>& buffer, Vector<int>& fileDescriptors)
{
    struct msghdr message;
    memset(&message, 0, sizeof(message));

    struct iovec iov[1];
    memset(&iov, 0, sizeof(iov));

    auto attachmentDescriptorBuffer = MallocSpan<char>::zeroedMalloc(CMSG_SPACE(sizeof(int) * attachmentMaxAmount));
    auto attachmentDescriptorSpan = attachmentDescriptorBuffer.mutableSpan();
    message.msg_control = attachmentDescriptorSpan.data();
    message.msg_controllen = attachmentDescriptorSpan.size();

    size_t previousBufferSize = buffer.size();
    buffer.grow(buffer.capacity());
    iov[0].iov_base = buffer.mutableSpan().data() + previousBufferSize;
    iov[0].iov_len = buffer.size() - previousBufferSize;

    message.msg_iov = iov;
    message.msg_iovlen = 1;

    while (true) {
        ssize_t bytesRead = recvmsg(socketDescriptor, &message, MSG_NOSIGNAL);

        if (bytesRead < 0) {
            if (errno == EINTR)
                continue;

            buffer.shrink(previousBufferSize);
            return -1;
        }

        if (message.msg_flags & MSG_CTRUNC) {
            // Control data has been discarded, which is expected by processMessage(), so consider this a read failure.
            buffer.shrink(previousBufferSize);
            return -1;
        }

        struct cmsghdr* controlMessage;
        for (controlMessage = CMSG_FIRSTHDR(&message); controlMessage; controlMessage = CMSG_NXTHDR(&message, controlMessage)) {
            if (controlMessage->cmsg_level == SOL_SOCKET && controlMessage->cmsg_type == SCM_RIGHTS) {
                if (controlMessage->cmsg_len < CMSG_LEN(0) || controlMessage->cmsg_len > CMSG_LEN(sizeof(int) * attachmentMaxAmount)) {
                    ASSERT_NOT_REACHED();
                    break;
                }
                size_t previousFileDescriptorsSize = fileDescriptors.size();
                size_t fileDescriptorsCount = (controlMessage->cmsg_len - CMSG_LEN(0)) / sizeof(int);
                fileDescriptors.grow(fileDescriptors.size() + fileDescriptorsCount);
                memcpy(fileDescriptors.mutableSpan().subspan(previousFileDescriptorsSize).data(), CMSG_DATA(controlMessage), sizeof(int) * fileDescriptorsCount);

                for (size_t i = 0; i < fileDescriptorsCount; ++i) {
                    if (!setCloseOnExec(fileDescriptors[previousFileDescriptorsSize + i])) {
                        ASSERT_NOT_REACHED();
                        break;
                    }
                }
                break;
            }
        }

        buffer.shrink(previousBufferSize + bytesRead);
        return bytesRead;
    }

    return -1;
}

void Connection::readyReadHandler()
{
    while (true) {
        ssize_t bytesRead = readBytesFromSocket(socketDescriptor(), m_readBuffer, m_fileDescriptors);

        if (bytesRead < 0) {
            // EINTR was already handled by readBytesFromSocket.
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;

            if (errno == ECONNRESET) {
                connectionDidClose();
                return;
            }

            if (m_isConnected) {
                WTFLogAlways("Error receiving IPC message on socket %d in process %d: %s", socketDescriptor(), getpid(), safeStrerror(errno).data());
                connectionDidClose();
            }
            return;
        }

        if (!bytesRead) {
            connectionDidClose();
            return;
        }

        // Process messages from data received.
        while (true) {
            if (!processMessage())
                break;
        }
    }
}

bool Connection::platformPrepareForOpen()
{
    if (setNonBlock(socketDescriptor()))
        return true;
    ASSERT_NOT_REACHED();
    return false;
}

void Connection::platformOpen()
{
    RefPtr<Connection> protectedThis(this);
    m_isConnected = true;

#if PLATFORM(PLAYSTATION)
    m_socketMonitor = Thread::create("SocketMonitor"_s, [protectedThis] {
        {
            int fd;
            while ((fd = protectedThis->socketDescriptor()) != -1) {
                int maxFd = fd;
                fd_set fdSet;
                FD_ZERO(&fdSet);
                FD_SET(fd, &fdSet);

                if (-1 != select(maxFd + 1, &fdSet, 0, 0, 0)) {
                    if (FD_ISSET(fd, &fdSet))
                        protectedThis->readyReadHandler();
                }
            }

        }
    });
    return;
#endif

    // Schedule a call to readyReadHandler. Data may have arrived before installation of the signal handler.
    m_connectionQueue->dispatch([protectedThis] {
        protectedThis->readyReadHandler();
    });
}

bool Connection::platformCanSendOutgoingMessages() const
{
    return true;
}

bool Connection::sendOutgoingMessage(UniqueRef<Encoder>&& encoder)
{
    static_assert(sizeof(MessageInfo) + attachmentMaxAmount * sizeof(size_t) <= messageMaxSize, "Attachments fit to message inline");

    UnixMessage outputMessage(encoder.get());
    if (outputMessage.attachments().size() > (attachmentMaxAmount - 1)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    size_t messageSizeWithBodyInline = sizeof(MessageInfo) + (outputMessage.attachments().size() * sizeof(AttachmentInfo)) + outputMessage.bodySize();
    if (messageSizeWithBodyInline > messageMaxSize && outputMessage.bodySize()) {
        if (!outputMessage.setBodyOutOfLine())
            return false;
    }

    return sendOutputMessage(WTFMove(outputMessage));
}

bool Connection::sendOutputMessage(UnixMessage&& outputMessage)
{
    auto& messageInfo = outputMessage.messageInfo();
    struct msghdr message;
    memset(&message, 0, sizeof(message));

    struct iovec iov[3];
    memset(&iov, 0, sizeof(iov));

    message.msg_iov = iov;
    int iovLength = 1;

    iov[0].iov_base = reinterpret_cast<void*>(&messageInfo);
    iov[0].iov_len = sizeof(messageInfo);

    Vector<AttachmentInfo> attachmentInfo;
    MallocSpan<char> attachmentFDBuffer;

    auto& attachments = outputMessage.attachments();
    if (!attachments.isEmpty()) {
        int* fdPtr = 0;

        size_t attachmentFDBufferLength = std::count_if(attachments.begin(), attachments.end(),
            [](const Attachment& attachment) {
                return !!attachment;
            });

        if (attachmentFDBufferLength) {
            attachmentFDBuffer = MallocSpan<char>::zeroedMalloc(CMSG_SPACE(sizeof(int) * attachmentFDBufferLength));
            auto span = attachmentFDBuffer.mutableSpan();
            message.msg_control = span.data();
            message.msg_controllen = span.size();

            struct cmsghdr* cmsg = CMSG_FIRSTHDR(&message);
            cmsg->cmsg_level = SOL_SOCKET;
            cmsg->cmsg_type = SCM_RIGHTS;
            cmsg->cmsg_len = CMSG_LEN(sizeof(int) * attachmentFDBufferLength);

            fdPtr = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        }

        attachmentInfo.resize(attachments.size());
        int fdIndex = 0;
        for (size_t i = 0; i < attachments.size(); ++i) {
            if (!!attachments[i]) {
                ASSERT(fdPtr);
                fdPtr[fdIndex++] = attachments[i].value();
            } else
                attachmentInfo[i].setNull();
        }

        iov[iovLength].iov_base = attachmentInfo.mutableSpan().data();
        iov[iovLength].iov_len = sizeof(AttachmentInfo) * attachments.size();
        ++iovLength;
    }

    if (!messageInfo.isBodyOutOfLine() && outputMessage.bodySize()) {
        iov[iovLength].iov_base = reinterpret_cast<void*>(outputMessage.body().data());
        iov[iovLength].iov_len = outputMessage.bodySize();
        ++iovLength;
    }

    message.msg_iovlen = iovLength;

    while (sendmsg(socketDescriptor(), &message, MSG_NOSIGNAL) == -1) {
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            struct pollfd pollfd;

            pollfd.fd = socketDescriptor();
            pollfd.events = POLLOUT;
            pollfd.revents = 0;
            poll(&pollfd, 1, -1);
            continue;
        }

#if OS(LINUX)
        // Linux can return EPIPE instead of ECONNRESET
        if (errno == EPIPE || errno == ECONNRESET)
#else
        if (errno == ECONNRESET)
#endif
        {
            connectionDidClose();
            return false;
        }

        if (m_isConnected)
            WTFLogAlways("Error sending IPC message: %s", safeStrerror(errno).data());
        return false;
    }

#if OS(ANDROID)
    RELEASE_ASSERT(m_outgoingHardwareBuffers.isEmpty());
    m_outgoingHardwareBuffers = WTFMove(hardwareBuffers);
    return sendOutgoingHardwareBuffers();
#else
    return true;
#endif
}

#if OS(ANDROID)
bool Connection::sendOutgoingHardwareBuffers()
{
    while (!m_outgoingHardwareBuffers.isEmpty()) {
        auto& buffer = m_outgoingHardwareBuffers.first();
        RELEASE_ASSERT(buffer);

        // There is no need to check for EINTR, it is handled internally.
        int result = AHardwareBuffer_sendHandleToUnixSocket(buffer.get(), socketDescriptor());
        if (!result) {
            m_outgoingHardwareBuffers.removeAt(0);
            continue;
        }

        if (result == -EAGAIN || result == -EWOULDBLOCK) {
            m_writeSocketMonitor.start(m_socket.get(), G_IO_OUT, m_connectionQueue->runLoop(), [this, protectedThis = Ref { *this }] (GIOCondition condition) -> gboolean {
                if (condition & G_IO_OUT) {
                    RELEASE_ASSERT(!m_outgoingHardwareBuffers.isEmpty());
                    // We can't stop the monitor from this lambda, because stop destroys the lambda.
                    m_connectionQueue->dispatch([this, protectedThis = Ref { *this }] {
                        m_writeSocketMonitor.stop();
                        if (m_isConnected) {
                            if (sendOutgoingHardwareBuffers())
                                sendOutgoingMessages();
                        }
                    });
                }
                return G_SOURCE_REMOVE;
            });
            return false;
        }

        if (result == -EPIPE || result == -ECONNRESET) {
            connectionDidClose();
            return false;
        }

        if (m_isConnected) {
            LOG_ERROR("Error sending AHardwareBuffer on socket %d in process %d: %s", socketDescriptor(), getpid(), safeStrerror(-result).data());
            connectionDidClose();
        }
        return false;
    }

    RELEASE_ASSERT(m_outgoingHardwareBuffers.isEmpty());
    return true;
}

bool Connection::receiveIncomingHardwareBuffers()
{
    while (m_pendingIncomingHardwareBufferCount) {
        AHardwareBuffer* buffer { nullptr };
        int result = AHardwareBuffer_recvHandleFromUnixSocket(socketDescriptor(), &buffer);
        if (!result) {
            m_pendingIncomingHardwareBufferCount--;
            auto hardwareBuffer = adoptRef(buffer);
            m_incomingHardwareBuffers.append(WTFMove(hardwareBuffer));
            continue;
        }

        if (result == -EAGAIN || result == -EWOULDBLOCK)
            return false;

        if (result == -ECONNRESET)
            connectionDidClose();

        if (m_isConnected) {
            LOG_ERROR("Error receiving AHardwareBuffer on socket %d in process %d: %s", socketDescriptor(), getpid(), safeStrerror(-result).data());
            connectionDidClose();
        }
        return false;
    }

    return true;
}
#endif // OS(ANDROID)

std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
{
    SocketPair socketPair = createPlatformConnection(SOCKET_TYPE);
    return { { Identifier { WTFMove(socketPair.server) }, ConnectionHandle { WTFMove(socketPair.client) } } };
}

} // namespace IPC

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
