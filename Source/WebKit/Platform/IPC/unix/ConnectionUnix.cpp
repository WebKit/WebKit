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

#include "DataReference.h"
#include "IPCUtilities.h"
#include "SharedMemory.h"
#include "UnixMessage.h"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <wtf/Assertions.h>
#include <wtf/SafeStrerror.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniStdExtras.h>

#if USE(GLIB)
#include <gio/gio.h>
#endif

#if OS(DARWIN)
#define MSG_NOSIGNAL 0
#endif

// Although it's available on Darwin, SOCK_SEQPACKET seems to work differently
// than in traditional Unix so fallback to STREAM on that platform.
#if defined(SOCK_SEQPACKET) && !OS(DARWIN)
#define SOCKET_TYPE SOCK_SEQPACKET
#else
#if USE(GLIB)
#define SOCKET_TYPE SOCK_STREAM
#else
#define SOCKET_TYPE SOCK_DGRAM
#endif
#endif // SOCK_SEQPACKET

namespace IPC {

static const size_t messageMaxSize = 4096;
static const size_t attachmentMaxAmount = 254;

class AttachmentInfo {
    WTF_MAKE_FAST_ALLOCATED;
public:
    AttachmentInfo()
    {
        // The entire AttachmentInfo is passed to write(), so we have to zero our
        // padding bytes to avoid writing uninitialized memory.
        memset(static_cast<void*>(this), 0, sizeof(*this));
    }

    AttachmentInfo(const AttachmentInfo& info)
    {
        memset(static_cast<void*>(this), 0, sizeof(*this));
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

void Connection::platformInitialize(Identifier identifier)
{
    m_socketDescriptor = identifier.handle;
#if USE(GLIB)
    m_socket = adoptGRef(g_socket_new_from_fd(m_socketDescriptor, nullptr));
#endif
    m_readBuffer.reserveInitialCapacity(messageMaxSize);
    m_fileDescriptors.reserveInitialCapacity(attachmentMaxAmount);
}

void Connection::platformInvalidate()
{
#if USE(GLIB)
    // In the GLib platform the socket descriptor is owned by GSocket.
    m_socket = nullptr;
#else
    if (m_socketDescriptor != -1)
        closeWithRetry(m_socketDescriptor);
#endif

    if (!m_isConnected)
        return;

#if USE(GLIB)
    m_readSocketMonitor.stop();
    m_writeSocketMonitor.stop();
#endif

#if PLATFORM(PLAYSTATION)
    if (m_socketMonitor) {
        m_socketMonitor->detach();
        m_socketMonitor = nullptr;
    }
#endif

    m_socketDescriptor = -1;
    m_isConnected = false;
}

bool Connection::processMessage()
{
    if (m_readBuffer.size() < sizeof(MessageInfo))
        return false;

    uint8_t* messageData = m_readBuffer.data();
    MessageInfo messageInfo;
    memcpy(static_cast<void*>(&messageInfo), messageData, sizeof(messageInfo));
    messageData += sizeof(messageInfo);

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
        memcpy(static_cast<void*>(attachmentInfo.data()), messageData, sizeof(AttachmentInfo) * attachmentCount);
        messageData += sizeof(AttachmentInfo) * attachmentCount;

        for (size_t i = 0; i < attachmentCount; ++i) {
            if (!attachmentInfo[i].isNull())
                attachmentFileDescriptorCount++;
        }

        if (messageInfo.isBodyOutOfLine())
            attachmentCount--;
    }

    Vector<Attachment> attachments(attachmentCount);
    RefPtr<WebKit::SharedMemory> oolMessageBody;

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

        WebKit::SharedMemory::Handle handle;
        handle.m_size = messageInfo.bodySize();
        handle.m_handle = UnixFileDescriptor { m_fileDescriptors[attachmentFileDescriptorCount - 1], UnixFileDescriptor::Adopt };

        oolMessageBody = WebKit::SharedMemory::map(handle, WebKit::SharedMemory::Protection::ReadOnly);
        if (!oolMessageBody) {
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    ASSERT(attachments.size() == (messageInfo.isBodyOutOfLine() ? messageInfo.attachmentCount() - 1 : messageInfo.attachmentCount()));

    uint8_t* messageBody = messageData;
    if (messageInfo.isBodyOutOfLine())
        messageBody = reinterpret_cast<uint8_t*>(oolMessageBody->data());

    auto decoder = Decoder::create(messageBody, messageInfo.bodySize(), WTFMove(attachments));
    ASSERT(decoder);
    if (!decoder)
        return false;

    processIncomingMessage(WTFMove(decoder));

    if (m_readBuffer.size() > messageLength) {
        memmove(m_readBuffer.data(), m_readBuffer.data() + messageLength, m_readBuffer.size() - messageLength);
        m_readBuffer.shrink(m_readBuffer.size() - messageLength);
    } else
        m_readBuffer.shrink(0);

    if (attachmentFileDescriptorCount) {
        if (m_fileDescriptors.size() > attachmentFileDescriptorCount) {
            memmove(m_fileDescriptors.data(), m_fileDescriptors.data() + attachmentFileDescriptorCount, (m_fileDescriptors.size() - attachmentFileDescriptorCount) * sizeof(int));
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

    message.msg_controllen = CMSG_SPACE(sizeof(int) * attachmentMaxAmount);
    MallocPtr<char> attachmentDescriptorBuffer = MallocPtr<char>::malloc(sizeof(char) * message.msg_controllen);
    memset(attachmentDescriptorBuffer.get(), 0, sizeof(char) * message.msg_controllen);
    message.msg_control = attachmentDescriptorBuffer.get();

    size_t previousBufferSize = buffer.size();
    buffer.grow(buffer.capacity());
    iov[0].iov_base = buffer.data() + previousBufferSize;
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
                memcpy(fileDescriptors.data() + previousFileDescriptorsSize, CMSG_DATA(controlMessage), sizeof(int) * fileDescriptorsCount);

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
        ssize_t bytesRead = readBytesFromSocket(m_socketDescriptor, m_readBuffer, m_fileDescriptors);

        if (bytesRead < 0) {
            // EINTR was already handled by readBytesFromSocket.
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                return;

            if (errno == ECONNRESET) {
                connectionDidClose();
                return;
            }

            if (m_isConnected) {
                WTFLogAlways("Error receiving IPC message on socket %d in process %d: %s", m_socketDescriptor, getpid(), safeStrerror(errno).data());
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
    if (setNonBlock(m_socketDescriptor))
        return true;
    ASSERT_NOT_REACHED();
    return false;
}

void Connection::platformOpen()
{
    RefPtr<Connection> protectedThis(this);
    m_isConnected = true;
#if USE(GLIB)
    m_readSocketMonitor.start(m_socket.get(), G_IO_IN, m_connectionQueue->runLoop(), [protectedThis] (GIOCondition condition) -> gboolean {
        if (condition & G_IO_HUP || condition & G_IO_ERR || condition & G_IO_NVAL) {
            protectedThis->connectionDidClose();
            return G_SOURCE_REMOVE;
        }

        if (condition & G_IO_IN) {
            protectedThis->readyReadHandler();
            return G_SOURCE_CONTINUE;
        }

        ASSERT_NOT_REACHED();
        return G_SOURCE_REMOVE;
    });
#endif

#if PLATFORM(PLAYSTATION)
    m_socketMonitor = Thread::create("SocketMonitor", [protectedThis] {
        {
            int fd;
            while ((fd = protectedThis->m_socketDescriptor) != -1) {
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
    return !m_pendingOutputMessage;
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
        RefPtr<WebKit::SharedMemory> oolMessageBody = WebKit::SharedMemory::allocate(outputMessage.bodySize());
        if (!oolMessageBody)
            return false;

        auto handle = oolMessageBody->createHandle(WebKit::SharedMemory::Protection::ReadOnly);
        if (!handle)
            return false;

        outputMessage.messageInfo().setBodyOutOfLine();

        memcpy(oolMessageBody->data(), outputMessage.body(), outputMessage.bodySize());

        outputMessage.appendAttachment(handle->releaseHandle());
    }

    return sendOutputMessage(outputMessage);
}

bool Connection::sendOutputMessage(UnixMessage& outputMessage)
{
    ASSERT(!m_pendingOutputMessage);

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
    MallocPtr<char> attachmentFDBuffer;

    auto& attachments = outputMessage.attachments();
    if (!attachments.isEmpty()) {
        int* fdPtr = 0;

        size_t attachmentFDBufferLength = std::count_if(attachments.begin(), attachments.end(),
            [](const Attachment& attachment) {
                return !!attachment;
            });

        if (attachmentFDBufferLength) {
            attachmentFDBuffer = MallocPtr<char>::malloc(sizeof(char) * CMSG_SPACE(sizeof(int) * attachmentFDBufferLength));

            message.msg_control = attachmentFDBuffer.get();
            message.msg_controllen = CMSG_SPACE(sizeof(int) * attachmentFDBufferLength);
            memset(message.msg_control, 0, message.msg_controllen);

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

        iov[iovLength].iov_base = attachmentInfo.data();
        iov[iovLength].iov_len = sizeof(AttachmentInfo) * attachments.size();
        ++iovLength;
    }

    if (!messageInfo.isBodyOutOfLine() && outputMessage.bodySize()) {
        iov[iovLength].iov_base = reinterpret_cast<void*>(outputMessage.body());
        iov[iovLength].iov_len = outputMessage.bodySize();
        ++iovLength;
    }

    message.msg_iovlen = iovLength;

    while (sendmsg(m_socketDescriptor, &message, MSG_NOSIGNAL) == -1) {
        if (errno == EINTR)
            continue;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
#if USE(GLIB)
            m_pendingOutputMessage = makeUnique<UnixMessage>(WTFMove(outputMessage));
            m_writeSocketMonitor.start(m_socket.get(), G_IO_OUT, m_connectionQueue->runLoop(), [this, protectedThis = Ref { *this }] (GIOCondition condition) -> gboolean {
                if (condition & G_IO_OUT) {
                    ASSERT(m_pendingOutputMessage);
                    // We can't stop the monitor from this lambda, because stop destroys the lambda.
                    m_connectionQueue->dispatch([this, protectedThis = Ref { *this }] {
                        m_writeSocketMonitor.stop();
                        auto message = WTFMove(m_pendingOutputMessage);
                        if (m_isConnected) {
                            sendOutputMessage(*message);
                            sendOutgoingMessages();
                        }
                    });
                }
                return G_SOURCE_REMOVE;
            });
            return false;
#else
            struct pollfd pollfd;

            pollfd.fd = m_socketDescriptor;
            pollfd.events = POLLOUT;
            pollfd.revents = 0;
            poll(&pollfd, 1, -1);
            continue;
#endif
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

    return true;
}

SocketPair createPlatformConnection(unsigned options)
{
    int sockets[2];
    RELEASE_ASSERT(socketpair(AF_UNIX, SOCKET_TYPE, 0, sockets) != -1);

    if (options & SetCloexecOnServer) {
        // Don't expose the child socket to the parent process.
        if (!setCloseOnExec(sockets[1]))
            RELEASE_ASSERT_NOT_REACHED();
    }

    if (options & SetCloexecOnClient) {
        // Don't expose the parent socket to potential future children.
        if (!setCloseOnExec(sockets[0]))
            RELEASE_ASSERT_NOT_REACHED();
    }

    SocketPair socketPair = { sockets[0], sockets[1] };
    return socketPair;
}

void Connection::willSendSyncMessage(OptionSet<SendSyncOption>)
{
}

void Connection::didReceiveSyncReply(OptionSet<SendSyncOption>)
{
}

std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
{
    SocketPair socketPair = createPlatformConnection();
    return ConnectionIdentifierPair { Identifier { UnixFileDescriptor { socketPair.server,  UnixFileDescriptor::Adopt } }, UnixFileDescriptor { socketPair.client, UnixFileDescriptor::Adopt } };
}
} // namespace IPC
