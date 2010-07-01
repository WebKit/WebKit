/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include <wtf/RandomNumber.h>
#include <wtf/text/WTFString.h>

using namespace std;
// We explicitly don't use the WebCore namespace here because CoreIPC should only use WTF types and
// WebCore::String is really in WTF.
using WebCore::String;
 
namespace CoreIPC {

// FIXME: Rename this or use a different constant on windows.
static const size_t inlineMessageMaxSize = 4096;

bool Connection::createServerAndClientIdentifiers(HANDLE& serverIdentifier, HANDLE& clientIdentifier)
{
    String pipeName;

    while (true) {
        unsigned uniqueID = randomNumber() * std::numeric_limits<unsigned>::max();
        pipeName = String::format("\\\\.\\pipe\\com.apple.WebKit.%x", uniqueID);

        serverIdentifier = ::CreateNamedPipe(pipeName.charactersWithNullTermination(),
                                             PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
                                             PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 1, inlineMessageMaxSize, inlineMessageMaxSize,
                                             0, 0);
        if (!serverIdentifier && ::GetLastError() == ERROR_PIPE_BUSY) {
            // There was already a pipe with this name, try again.
            continue;
        }

        break;
    }

    if (!serverIdentifier)
        return false;

    clientIdentifier = ::CreateFileW(pipeName.charactersWithNullTermination(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
    if (!clientIdentifier) {
        ::CloseHandle(serverIdentifier);
        return false;
    }

    DWORD mode = PIPE_READMODE_MESSAGE;
    if (!::SetNamedPipeHandleState(clientIdentifier, &mode, 0, 0)) {
        ::CloseHandle(serverIdentifier);
        ::CloseHandle(clientIdentifier);
        return false;
    }

    return true;
}

void Connection::platformInitialize(Identifier identifier)
{
    memset(&m_readState, 0, sizeof(m_readState));
    m_readState.hEvent = ::CreateEventW(0, false, false, 0);

    m_connectionPipe = identifier;
}

void Connection::platformInvalidate()
{
    if (m_connectionPipe == INVALID_HANDLE_VALUE)
        return;

    // FIXME: Unregister the handle.

    ::CloseHandle(m_connectionPipe);
    m_connectionPipe = INVALID_HANDLE_VALUE;
}

void Connection::readEventHandler()
{
    bool wasConnected = m_isConnected;
    if (!m_isConnected) {
        m_isConnected = true;

        // We're now connected, send any outgoing messages we might have.
        sendOutgoingMessages();
    }

    DWORD numberOfBytesRead = 0;
    if (wasConnected) {
        // Check if we got some data.
        if (!::GetOverlappedResult(m_connectionPipe, &m_readState, &numberOfBytesRead, FALSE)) {
            DWORD error = ::GetLastError();

            switch (error) {
            case ERROR_BROKEN_PIPE:
                connectionDidClose();
                return;

            // FIXME: We should figure out why we're getting this error.
            case ERROR_IO_INCOMPLETE:
                break;
            default:
                ASSERT_NOT_REACHED();
            }
        }
    }

    while (true) {
        if (numberOfBytesRead) {
            // We have a message, let's dispatch it.

            // The messageID is encoded at the end of the buffer.
            size_t realBufferSize = numberOfBytesRead - sizeof(MessageID);

            unsigned messageID = *reinterpret_cast<unsigned*>(m_readBuffer.data() + realBufferSize);

            std::auto_ptr<ArgumentDecoder> arguments(new ArgumentDecoder(m_readBuffer.data(), realBufferSize));
            processIncomingMessage(MessageID::fromInt(messageID), arguments);
        }

        // FIXME: Do this somewhere else.
        m_readBuffer.resize(inlineMessageMaxSize);

        // Read from the pipe.
        BOOL result = ::ReadFile(m_connectionPipe, m_readBuffer.data(), m_readBuffer.size(), &numberOfBytesRead, &m_readState);

        if (!result) {
            DWORD error = ::GetLastError();

            if (error == ERROR_IO_PENDING) {
                // There's pending data - readEventHandler will be called again once the read is complete.
                return;
            }

            // FIXME: We need to handle other errors here.
            ASSERT_NOT_REACHED();
        }

        ASSERT(numberOfBytesRead);
    }
}

bool Connection::open()
{
    // Start listening for read state events.
    m_connectionQueue.registerHandle(m_readState.hEvent, WorkItem::create(this, &Connection::readEventHandler));

    if (m_isServer) {
        // Wait for a connection.
        if (::ConnectNamedPipe(m_connectionPipe, &m_readState))
            m_isConnected = true;
        else {
            // Even though the call to ConnectNamedPipe failed, we might still have a valid connection.
            DWORD error = ::GetLastError();
            if (error == ERROR_PIPE_CONNECTED) {
                // The client connected to the named pipe before we opened the connection.
                m_isConnected = true;
            } else if (error != ERROR_IO_PENDING) {
                // Something went wrong.
                // FIXME: Close the pipe here.
                return false;
            }
        }

        if (m_isConnected) {
            // Signal the read event handle.
            ::SetEvent(m_readState.hEvent);
        }
    } else {
        // Schedule a read.
        m_connectionQueue.scheduleWork(WorkItem::create(this, &Connection::readEventHandler));
    }

    return true;
}

void Connection::sendOutgoingMessage(MessageID messageID, auto_ptr<ArgumentEncoder> arguments)
{
    // Just bail if the handle has been closed.
    if (m_connectionPipe == INVALID_HANDLE_VALUE)
        return;

    // We put the message ID last.
    arguments->encodeUInt32(messageID.toInt());

    // Write the outgoing message.
    OVERLAPPED overlapped = { 0 };

    DWORD bytesWritten;
    if (::WriteFile(m_connectionPipe, arguments->buffer(), arguments->bufferSize(), &bytesWritten, &overlapped)) {
        // We successfully sent this message.
        return;
    }

    DWORD error = ::GetLastError();
    ASSERT_NOT_REACHED();
}

} // namespace CoreIPC
