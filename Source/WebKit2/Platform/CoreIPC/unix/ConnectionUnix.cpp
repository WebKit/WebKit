/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
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

#include "ArgumentEncoder.h"
#include "ProcessLauncher.h"
#include "WorkItem.h"
#include "SharedMemory.h"
#include "WebProcessProxy.h"
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <wtf/Assertions.h>

#if PLATFORM(QT)
#include <QSocketNotifier>
#elif PLATFORM(GTK)
#include <glib.h>
#endif

using namespace std;

namespace CoreIPC {

static const size_t messageMaxSize = 4096;
static const size_t attachmentMaxAmount = 255;

enum {
    MessageBodyIsOOL = 1U << 31
};

class MessageInfo {
public:
    MessageInfo() { }

    MessageInfo(MessageID messageID, size_t bodySize, size_t initialAttachmentCount)
        : m_messageID(messageID.toInt())
        , m_bodySize(bodySize)
        , m_attachmentCount(initialAttachmentCount)
    {
        ASSERT(!(m_messageID & MessageBodyIsOOL));
    }

    void setMessageBodyOOL()
    {
        ASSERT(!isMessageBodyOOL());

        m_messageID |= MessageBodyIsOOL;
        m_attachmentCount++;
    }

    bool isMessageBodyOOL() const { return m_messageID & MessageBodyIsOOL; }

    size_t bodySize() const { return m_bodySize; }

    MessageID messageID() const { return MessageID::fromInt(m_messageID & ~MessageBodyIsOOL); }

    size_t attachmentCount() const { return m_attachmentCount; }

private:
    uint32_t m_messageID;
    size_t m_bodySize;
    size_t m_attachmentCount;
};

void Connection::platformInitialize(Identifier identifier)
{
    m_socketDescriptor = identifier;
    m_readBuffer.resize(messageMaxSize);
    m_currentMessageSize = 0;

#if PLATFORM(QT)
    m_socketNotifier = 0;
#endif
}

void Connection::platformInvalidate()
{
    if (m_socketDescriptor != -1)
        while (close(m_socketDescriptor) == -1 && errno == EINTR) { }

    if (!m_isConnected)
        return;

#if PLATFORM(GTK)
    m_connectionQueue.unregisterEventSourceHandler(m_socketDescriptor);
#endif

#if PLATFORM(QT)
    delete m_socketNotifier;
    m_socketNotifier = 0;
#endif

    m_socketDescriptor = -1;
    m_isConnected = false;
}

#if PLATFORM(QT)
class SocketNotifierResourceGuard {
public:
    SocketNotifierResourceGuard(QSocketNotifier* socketNotifier)
        : m_socketNotifier(socketNotifier)
    {
        m_socketNotifier->setEnabled(false);
    }

    ~SocketNotifierResourceGuard()
    {
        m_socketNotifier->setEnabled(true);
    }

private:
    QSocketNotifier* const m_socketNotifier;
};
#endif

template<class T, class iterator>
class AttachmentResourceGuard {
public:
    AttachmentResourceGuard(T& attachments)
        : m_attachments(attachments)
    {
    }
    ~AttachmentResourceGuard()
    {
        iterator end = m_attachments.end();
        for (iterator i = m_attachments.begin(); i != end; ++i)
            i->dispose();
    }
private:
    T& m_attachments;
};

void Connection::readyReadHandler()
{
    Deque<Attachment> attachments;
#if PLATFORM(QT)
    SocketNotifierResourceGuard socketNotifierEnabler(m_socketNotifier);
#endif
    AttachmentResourceGuard<Deque<Attachment>, Deque<Attachment>::iterator> attachementDisposer(attachments);

    OwnArrayPtr<char> attachmentDescriptorBuffer = adoptArrayPtr(new char[CMSG_SPACE(sizeof(int) * (attachmentMaxAmount))]);
    struct msghdr message;
    memset(&message, 0, sizeof(message));

    struct iovec iov[1];
    memset(&iov, 0, sizeof(iov));

    message.msg_control = attachmentDescriptorBuffer.get();
    message.msg_controllen = CMSG_SPACE(sizeof(int) * (attachmentMaxAmount));

    iov[0].iov_base = m_readBuffer.data();
    iov[0].iov_len = m_readBuffer.size();

    message.msg_iov = iov;
    message.msg_iovlen = 1;


    int messageLength = 0;
    while ((messageLength = recvmsg(m_socketDescriptor, &message, 0)) == -1) {
        if (errno != EINTR)
            return;
    }

    struct cmsghdr* controlMessage = CMSG_FIRSTHDR(&message);

    MessageInfo messageInfo;
    unsigned char* messageData = m_readBuffer.data();

    memcpy(&messageInfo, messageData, sizeof(messageInfo));
    ASSERT(messageLength == sizeof(messageInfo) + messageInfo.attachmentCount() * sizeof(size_t) + (messageInfo.isMessageBodyOOL() ? 0 : messageInfo.bodySize()));

    messageData += sizeof(messageInfo);

    RefPtr<WebKit::SharedMemory> oolMessageBody;

    if (messageInfo.attachmentCount()) {
        if (controlMessage && controlMessage->cmsg_level == SOL_SOCKET && controlMessage->cmsg_type == SCM_RIGHTS) {
            OwnArrayPtr<size_t> attachmentSizes = adoptArrayPtr(new size_t[messageInfo.attachmentCount()]);
            memcpy(attachmentSizes.get(), messageData, sizeof(size_t) * messageInfo.attachmentCount());

            messageData += sizeof(attachmentSizes);

            OwnArrayPtr<int> fileDescriptors = adoptArrayPtr(new int[messageInfo.attachmentCount()]);
            memcpy(fileDescriptors.get(), CMSG_DATA(controlMessage), sizeof(int) * messageInfo.attachmentCount());

            int attachmentCount = messageInfo.attachmentCount();

            if (messageInfo.isMessageBodyOOL())
                attachmentCount--;

            for (int i = 0; i < attachmentCount; ++i) {
                while (fcntl(fileDescriptors[i], F_SETFL, FD_CLOEXEC) == -1) {
                    if (errno != EINTR) {
                        ASSERT_NOT_REACHED();
                        return;
                    }
                }
            }

            for (int i = 0; i < attachmentCount; ++i)
                attachments.append(Attachment(fileDescriptors[i], attachmentSizes[i]));

            if (messageInfo.isMessageBodyOOL()) {
                ASSERT(messageInfo.bodySize());

                WebKit::SharedMemory::Handle handle;
                handle.adoptFromAttachment(fileDescriptors[attachmentCount], attachmentSizes[attachmentCount]);
                if (handle.isNull()) {
                    ASSERT_NOT_REACHED();
                    return;
                }

                oolMessageBody = WebKit::SharedMemory::create(handle, WebKit::SharedMemory::ReadOnly);
                if (!oolMessageBody) {
                    ASSERT_NOT_REACHED();
                    return;
                }
            }

            controlMessage = CMSG_NXTHDR(&message, controlMessage);
        } else {
            ASSERT_NOT_REACHED();
            return;
        }
    }

    ASSERT(attachments.size() == messageInfo.isMessageBodyOOL() ? messageInfo.attachmentCount() - 1 : messageInfo.attachmentCount());

    unsigned char* messageBody = messageData;

    if (messageInfo.isMessageBodyOOL())
        messageBody = reinterpret_cast<unsigned char*>(oolMessageBody->data());

    ArgumentDecoder* argumentDecoder;
    if (attachments.isEmpty())
        argumentDecoder = new ArgumentDecoder(messageBody, messageInfo.bodySize());
    else
        argumentDecoder = new ArgumentDecoder(messageBody, messageInfo.bodySize(), attachments);

    processIncomingMessage(messageInfo.messageID(), adoptPtr(argumentDecoder));

    ASSERT(!controlMessage);
}

bool Connection::open()
{
#if PLATFORM(QT)
    ASSERT(!m_socketNotifier);
#endif

    int flags = fcntl(m_socketDescriptor, F_GETFL, 0);
    while (fcntl(m_socketDescriptor, F_SETFL, flags | O_NONBLOCK) == -1) {
        if (errno != EINTR) {
            ASSERT_NOT_REACHED();
            return false;
        }
    }

    m_isConnected = true;
#if PLATFORM(QT)
    m_socketNotifier = m_connectionQueue.registerSocketEventHandler(m_socketDescriptor, QSocketNotifier::Read, WorkItem::create(this, &Connection::readyReadHandler));
#elif PLATFORM(GTK)
    m_connectionQueue.registerEventSourceHandler(m_socketDescriptor, (G_IO_HUP | G_IO_ERR), WorkItem::create(this, &Connection::connectionDidClose));
    m_connectionQueue.registerEventSourceHandler(m_socketDescriptor, G_IO_IN, WorkItem::create(this, &Connection::readyReadHandler));
#endif

    // Schedule a call to readyReadHandler. Data may have arrived before installation of the signal
    // handler.
    m_connectionQueue.scheduleWork(WorkItem::create(this, &Connection::readyReadHandler));

    return true;
}

bool Connection::platformCanSendOutgoingMessages() const
{
    return m_isConnected;
}

bool Connection::sendOutgoingMessage(MessageID messageID, PassOwnPtr<ArgumentEncoder> arguments)
{
#if PLATFORM(QT)
    ASSERT(m_socketNotifier);
#endif

    COMPILE_ASSERT(sizeof(MessageInfo) + attachmentMaxAmount * sizeof(size_t) <= messageMaxSize, AttachmentsFitToMessageInline);

    Vector<Attachment> attachments = arguments->releaseAttachments();
    AttachmentResourceGuard<Vector<Attachment>, Vector<Attachment>::iterator> attachementDisposer(attachments);

    if (attachments.size() > (attachmentMaxAmount - 1)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    MessageInfo messageInfo(messageID, arguments->bufferSize(), attachments.size());
    size_t messageSizeWithBodyInline = sizeof(messageInfo) + (attachments.size() * sizeof(size_t)) + arguments->bufferSize();
    if (messageSizeWithBodyInline > messageMaxSize && arguments->bufferSize()) {
        RefPtr<WebKit::SharedMemory> oolMessageBody = WebKit::SharedMemory::create(arguments->bufferSize());
        if (!oolMessageBody)
            return false;

        WebKit::SharedMemory::Handle handle;
        if (!oolMessageBody->createHandle(handle, WebKit::SharedMemory::ReadOnly))
            return false;

        messageInfo.setMessageBodyOOL();

        memcpy(oolMessageBody->data(), arguments->buffer(), arguments->bufferSize());

        attachments.append(handle.releaseToAttachment());
    }

    struct msghdr message;
    memset(&message, 0, sizeof(message));

    struct iovec iov[3];
    memset(&iov, 0, sizeof(iov));

    message.msg_iov = iov;
    int iovLength = 1;

    iov[0].iov_base = reinterpret_cast<void*>(&messageInfo);
    iov[0].iov_len = sizeof(messageInfo);

    OwnArrayPtr<char> attachmentFDBuffer = adoptArrayPtr(new char[CMSG_SPACE(sizeof(int) * attachments.size())]);
    OwnArrayPtr<size_t> attachmentSizes = adoptArrayPtr(new size_t[attachments.size()]);

    if (!attachments.isEmpty()) {
        message.msg_control = attachmentFDBuffer.get();
        message.msg_controllen = sizeof(char) * CMSG_SPACE(sizeof(int) * attachments.size());

        struct cmsghdr* cmsg = CMSG_FIRSTHDR(&message);
        cmsg->cmsg_level = SOL_SOCKET;
        cmsg->cmsg_type = SCM_RIGHTS;
        cmsg->cmsg_len = CMSG_LEN(sizeof(int) * attachments.size());

        int* fdptr = reinterpret_cast<int*>(CMSG_DATA(cmsg));
        for (size_t i = 0; i < attachments.size(); ++i) {
            attachmentSizes[i] = attachments[i].size();
            fdptr[i] = attachments[i].fileDescriptor();
        }

        message.msg_controllen = cmsg->cmsg_len;

        iov[iovLength].iov_base = attachmentSizes.get();
        iov[iovLength].iov_len = sizeof(size_t) * attachments.size();
        ++iovLength;
    }

    if (!messageInfo.isMessageBodyOOL() && arguments->bufferSize()) {
        iov[iovLength].iov_base = reinterpret_cast<void*>(arguments->buffer());
        iov[iovLength].iov_len = arguments->bufferSize();
        ++iovLength;
    }

    message.msg_iovlen = iovLength;

    int bytesSent = 0;
    while ((bytesSent = sendmsg(m_socketDescriptor, &message, 0)) == -1) {
        if (errno != EINTR)
            return false;
    }
    return true;
}

#if PLATFORM(QT) || PLATFORM(GTK)
void Connection::setShouldCloseConnectionOnProcessTermination(WebKit::PlatformProcessIdentifier process)
{
    m_connectionQueue.scheduleWorkOnTermination(process, WorkItem::create(this, &Connection::connectionDidClose));
}
#endif

} // namespace CoreIPC
