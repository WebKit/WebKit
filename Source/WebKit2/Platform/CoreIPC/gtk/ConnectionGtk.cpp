/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Portions Copyright (c) 2010 Motorola Mobility, Inc.  All rights reserved.
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

#include "Connection.h"

#include "ArgumentEncoder.h"
#include "WorkItem.h"
#include <errno.h>
#include <glib.h>
#include <sys/fcntl.h>

using namespace std;

namespace CoreIPC {

static const size_t initialMessageBufferSize = 4096;

static int readBytesFromSocket(int fileDescriptor, uint8_t* ptr, size_t length)
{
    ASSERT(fileDescriptor > 0);
    ASSERT(ptr);
    ASSERT(length > 0);

    ssize_t numberOfBytesRead = 0;
    size_t pendingBytesToRead = length;
    uint8_t* buffer = ptr;

    while (pendingBytesToRead > 0) {
        if ((numberOfBytesRead = read(fileDescriptor, buffer, pendingBytesToRead)) < 0) {
            if (errno == EINTR)
                numberOfBytesRead = 0;
            else if (errno == EAGAIN || errno == EWOULDBLOCK)
                break;
            else
                return 0; 
        } else if (!numberOfBytesRead)
            break;

        buffer += numberOfBytesRead;
        pendingBytesToRead -= numberOfBytesRead;
    }

    return (length - pendingBytesToRead);
}

static bool writeBytesToSocket(int fileDescriptor, uint8_t* ptr, size_t length)
{
    ASSERT(fileDescriptor > 0);
    ASSERT(ptr);
    ASSERT(length > 0);

    ssize_t numberOfBytesWritten = 0;
    size_t pendingBytesToWrite = length;
    uint8_t* buffer = ptr;

    // Keep writing to the socket till the complete message has been written.
    while (pendingBytesToWrite > 0) {
        if ((numberOfBytesWritten = write(fileDescriptor, buffer, pendingBytesToWrite)) < 0) {
            if (errno == EINTR)
                numberOfBytesWritten = 0;
            else
                return false;
        }
        buffer += numberOfBytesWritten;
        pendingBytesToWrite -= numberOfBytesWritten;
    }

    // Write operation failed if complete message is not written.
    return !pendingBytesToWrite;
}

void Connection::platformInitialize(Identifier identifier)
{
    m_currentMessageSize = 0;
    m_pendingBytes = 0;
    m_readBuffer.resize(initialMessageBufferSize);
    m_socket = identifier;
    m_isConnected = true;
}

void Connection::platformInvalidate()
{
    if (!m_isConnected)
        return;

    m_connectionQueue.unregisterEventSourceHandler(m_socket);
    if (m_socket > 0) {
        close(m_socket);
        m_socket = -1;
    }

    m_isConnected = false;
}

void Connection::processCompletedMessage()
{
    size_t realBufferSize = m_currentMessageSize - sizeof(MessageID);
    unsigned messageID = *reinterpret_cast<unsigned*>(m_readBuffer.data() + realBufferSize);

    processIncomingMessage(MessageID::fromInt(messageID), adoptPtr(new ArgumentDecoder(m_readBuffer.data(), realBufferSize)));

    // Prepare for the next message.
    m_currentMessageSize = 0;
    m_pendingBytes = 0;
}

void Connection::readEventHandler()
{
    if (m_socket < 0)
        return;

    // Handle any previously unprocessed message.
    if (!messageProcessingCompleted()) {
        if ((m_pendingBytes -= readBytesFromSocket(m_socket, (m_readBuffer.data() + (m_currentMessageSize - m_pendingBytes)), m_pendingBytes)) > 0)
            return;

        // Message received completely. Process the message now.
        processCompletedMessage();
    }

    // Prepare to read the next message.
    uint8_t sizeBuffer[sizeof(size_t)];
    memset(sizeBuffer, 0, sizeof(size_t));
    
    while (messageProcessingCompleted()) {
        if (readBytesFromSocket(m_socket, sizeBuffer, sizeof(size_t)))
            m_currentMessageSize = *reinterpret_cast<size_t*>(sizeBuffer);

        if (!m_currentMessageSize)
            break;

        if (m_readBuffer.size() < m_currentMessageSize)
            m_readBuffer.grow(m_currentMessageSize);

        m_pendingBytes = m_currentMessageSize - readBytesFromSocket(m_socket, m_readBuffer.data(), m_currentMessageSize);
        if (m_pendingBytes > 0) // Message partially received.
            break;

        // Message received completely. Process the message now.
        processCompletedMessage();

        memset(sizeBuffer, 0, sizeof(size_t));
    }
}

bool Connection::open()
{
    int flags = fcntl(m_socket, F_GETFL, 0);
    fcntl(m_socket, F_SETFL, flags | O_NONBLOCK);

    // Register callbacks for connection termination and input data on the WorkQueue.
    m_connectionQueue.registerEventSourceHandler(m_socket, (G_IO_HUP | G_IO_ERR), WorkItem::create(this, &Connection::connectionDidClose));
    m_connectionQueue.registerEventSourceHandler(m_socket, G_IO_IN, WorkItem::create(this, &Connection::readEventHandler));
    return true;
}

bool Connection::platformCanSendOutgoingMessages() const
{
    return (m_socket > 0);
}

bool Connection::sendOutgoingMessage(MessageID messageID, PassOwnPtr<ArgumentEncoder> arguments)
{
    if (m_socket < 0)
        return false;

    // We put the message ID last.
    arguments->encodeUInt32(messageID.toInt());

    size_t bufferSize = arguments->bufferSize();

    // Send the message size first.
    if (!writeBytesToSocket(m_socket, reinterpret_cast<uint8_t*>(&bufferSize), sizeof(size_t)))
        return false;

    if (!writeBytesToSocket(m_socket, arguments->buffer(), arguments->bufferSize()))
        return false;

    return true;
}

} // namespace CoreIPC
