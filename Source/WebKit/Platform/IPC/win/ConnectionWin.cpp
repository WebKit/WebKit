/*
 * Copyright (C) 2017 Sony Interactive Entertainment Inc.
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
#include <wtf/HexNumber.h>
#include <wtf/RandomNumber.h>

namespace IPC {

// FIXME: Rename this or use a different constant on windows.
static const size_t inlineMessageMaxSize = 4096;

bool Connection::createServerAndClientIdentifiers(HANDLE& serverIdentifier, HANDLE& clientIdentifier)
{
    String pipeName;

    do {
        unsigned uniqueID = randomNumber() * std::numeric_limits<unsigned>::max();
        pipeName = makeString("\\\\.\\pipe\\com.apple.WebKit.", hex(uniqueID));

        serverIdentifier = ::CreateNamedPipe(pipeName.charactersWithNullTermination().data(),
            PIPE_ACCESS_DUPLEX | FILE_FLAG_FIRST_PIPE_INSTANCE | FILE_FLAG_OVERLAPPED,
            PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE, 1, inlineMessageMaxSize, inlineMessageMaxSize,
            0, 0);
    } while (!serverIdentifier && ::GetLastError() == ERROR_PIPE_BUSY);

    if (!serverIdentifier)
        return false;

    clientIdentifier = ::CreateFileW(pipeName.charactersWithNullTermination().data(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, 0);
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
    m_connectionPipe = identifier;
}

void Connection::platformInvalidate()
{
    if (m_connectionPipe == INVALID_HANDLE_VALUE)
        return;

    m_isConnected = false;

    m_readListener.close();
    m_writeListener.close();

    ::CloseHandle(m_connectionPipe);
    m_connectionPipe = INVALID_HANDLE_VALUE;
}

void Connection::readEventHandler()
{
    if (m_connectionPipe == INVALID_HANDLE_VALUE)
        return;

    while (true) {
        // Check if we got some data.
        DWORD numberOfBytesRead = 0;
        if (!::GetOverlappedResult(m_connectionPipe, &m_readListener.state(), &numberOfBytesRead, FALSE)) {
            DWORD error = ::GetLastError();
            switch (error) {
            case ERROR_BROKEN_PIPE:
                connectionDidClose();
                return;
            case ERROR_MORE_DATA: {
                // Read the rest of the message out of the pipe.

                DWORD bytesToRead = 0;
                if (!::PeekNamedPipe(m_connectionPipe, 0, 0, 0, 0, &bytesToRead)) {
                    DWORD error = ::GetLastError();
                    if (error == ERROR_BROKEN_PIPE) {
                        connectionDidClose();
                        return;
                    }
                    ASSERT_NOT_REACHED();
                    return;
                }

                // ::GetOverlappedResult told us there's more data. ::PeekNamedPipe shouldn't
                // contradict it!
                ASSERT(bytesToRead);
                if (!bytesToRead)
                    break;

                m_readBuffer.grow(m_readBuffer.size() + bytesToRead);
                if (!::ReadFile(m_connectionPipe, m_readBuffer.data() + numberOfBytesRead, bytesToRead, 0, &m_readListener.state())) {
                    DWORD error = ::GetLastError();
                    ASSERT_NOT_REACHED();
                    return;
                }
                continue;
            }

            // FIXME: We should figure out why we're getting this error.
            case ERROR_IO_INCOMPLETE:
                return;
            default:
                ASSERT_NOT_REACHED();
            }
        }

        if (!m_readBuffer.isEmpty()) {
            // We have a message, let's dispatch it.
            Vector<Attachment> attachments(0);
            auto decoder = std::make_unique<Decoder>(m_readBuffer.data(), m_readBuffer.size(), nullptr, WTFMove(attachments));
            processIncomingMessage(WTFMove(decoder));
        }

        // Find out the size of the next message in the pipe (if there is one) so that we can read
        // it all in one operation. (This is just an optimization to avoid an extra pass through the
        // loop (if we chose a buffer size that was too small) or allocating extra memory (if we
        // chose a buffer size that was too large).)
        DWORD bytesToRead = 0;
        if (!::PeekNamedPipe(m_connectionPipe, 0, 0, 0, 0, &bytesToRead)) {
            DWORD error = ::GetLastError();
            if (error == ERROR_BROKEN_PIPE) {
                connectionDidClose();
                return;
            }
            ASSERT_NOT_REACHED();
        }
        if (!bytesToRead) {
            // There's no message waiting in the pipe. Schedule a read of the first byte of the
            // next message. We'll find out the message's actual size when it arrives. (If we
            // change this to read more than a single byte for performance reasons, we'll have to
            // deal with m_readBuffer potentially being larger than the message we read after
            // calling ::GetOverlappedResult above.)
            bytesToRead = 1;
        }

        m_readBuffer.resize(bytesToRead);

        // Either read the next available message (which should occur synchronously), or start an
        // asynchronous read of the next message that becomes available.
        BOOL result = ::ReadFile(m_connectionPipe, m_readBuffer.data(), m_readBuffer.size(), 0, &m_readListener.state());
        if (result) {
            // There was already a message waiting in the pipe, and we read it synchronously.
            // Process it.
            continue;
        }

        DWORD error = ::GetLastError();

        if (error == ERROR_IO_PENDING) {
            // There are no messages in the pipe currently. readEventHandler will be called again once there is a message.
            return;
        }

        if (error == ERROR_MORE_DATA) {
            // Either a message is available when we didn't think one was, or the message is larger
            // than ::PeekNamedPipe told us. The former seems far more likely. Probably the message
            // became available between our calls to ::PeekNamedPipe and ::ReadFile above. Go back
            // to the top of the loop to use ::GetOverlappedResult to retrieve the available data.
            continue;
        }

        if (error == ERROR_BROKEN_PIPE)
            return;

        // FIXME: We need to handle other errors here.
        ASSERT_NOT_REACHED();
    }
}

void Connection::writeEventHandler()
{
    if (m_connectionPipe == INVALID_HANDLE_VALUE)
        return;

    DWORD numberOfBytesWritten = 0;
    if (!::GetOverlappedResult(m_connectionPipe, &m_writeListener.state(), &numberOfBytesWritten, FALSE)) {
        DWORD error = ::GetLastError();

        if (error == ERROR_IO_INCOMPLETE) {
            // FIXME: We should figure out why we're getting this error.
            return;
        }
        if (error == ERROR_BROKEN_PIPE) {
            connectionDidClose();
            return;
        }
        ASSERT_NOT_REACHED();
    }

    // The pending write has finished, so we are now done with its encoder. Clearing this member
    // will allow us to send messages again.
    m_pendingWriteEncoder = nullptr;

    // Now that the pending write has finished, we can try to send a new message.
    sendOutgoingMessages();
}

void Connection::invokeReadEventHandler()
{
    m_connectionQueue->dispatch([this, protectedThis = makeRef(*this)] {
        readEventHandler();
    });
}

void Connection::invokeWriteEventHandler()
{
    m_connectionQueue->dispatch([this, protectedThis = makeRef(*this)] {
        writeEventHandler();
    });
}

bool Connection::open()
{
    // We connected the two ends of the pipe in createServerAndClientIdentifiers.
    m_isConnected = true;

    // Start listening for read and write state events.
    m_readListener.open([this] {
        invokeReadEventHandler();
    });

    m_writeListener.open([this] {
        invokeWriteEventHandler();
    });

    // Schedule a read.
    invokeReadEventHandler();
    return true;
}

bool Connection::platformCanSendOutgoingMessages() const
{
    // We only allow sending one asynchronous message at a time. If we wanted to send more than one
    // at once, we'd have to use multiple OVERLAPPED structures and hold onto multiple pending
    // MessageEncoders (one of each for each simultaneous asynchronous message).
    return !m_pendingWriteEncoder;
}

bool Connection::sendOutgoingMessage(std::unique_ptr<Encoder> encoder)
{
    ASSERT(!m_pendingWriteEncoder);

    // Just bail if the handle has been closed.
    if (m_connectionPipe == INVALID_HANDLE_VALUE)
        return false;

    // We put the message ID last.
    *encoder << 0;

    // Write the outgoing message.

    if (::WriteFile(m_connectionPipe, encoder->buffer(), encoder->bufferSize(), 0, &m_writeListener.state())) {
        // We successfully sent this message.
        return true;
    }

    DWORD error = ::GetLastError();

    if (error == ERROR_NO_DATA) {
        // The pipe is being closed.
        connectionDidClose();
        return false;
    }

    if (error != ERROR_IO_PENDING) {
        ASSERT_NOT_REACHED();
        return false;
    }

    // The message will be sent soon. Hold onto the encoder so that it won't be destroyed
    // before the write completes.
    m_pendingWriteEncoder = WTFMove(encoder);

    // We can only send one asynchronous message at a time (see comment in platformCanSendOutgoingMessages).
    return false;
}

void Connection::willSendSyncMessage(OptionSet<SendSyncOption>)
{
}

void Connection::didReceiveSyncReply(OptionSet<SendSyncOption>)
{
}

void Connection::EventListener::open(Function<void()>&& handler)
{
    m_handler = WTFMove(handler);

    memset(&m_state, 0, sizeof(m_state));
    m_state.hEvent = ::CreateEventW(0, FALSE, FALSE, 0);

    BOOL result;
    result = ::RegisterWaitForSingleObject(&m_waitHandle, m_state.hEvent, callback, this, INFINITE, WT_EXECUTEDEFAULT);
    ASSERT(result);
}

void Connection::EventListener::callback(void* data, BOOLEAN timerOrWaitFired)
{
    ASSERT_ARG(data, data);
    ASSERT_ARG(timerOrWaitFired, !timerOrWaitFired);

    auto* listener = static_cast<Connection::EventListener*>(data);
    listener->m_handler();
}

void Connection::EventListener::close()
{
    // We call ::UnregisterWaitEx directly here. Since ::UnregisterWaitEx drains all the remaining tasks here,
    // it would cause deadlock if this function itself is executed in Windows callback functions. But this call
    // is safe since our callbacks immediately dispatch a task to WorkQueue. And no Windows callbacks call this
    // Connection::EventListener::close().
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms685061(v=vs.85).aspx
    //
    // And do not ::CloseHandle(m_waitHandle).
    // > Note that a wait handle cannot be used in functions that require an object handle, such as CloseHandle.
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ms685061(v=vs.85).aspx
    ::UnregisterWaitEx(m_waitHandle, INVALID_HANDLE_VALUE);
    m_waitHandle = INVALID_HANDLE_VALUE;

    ::CloseHandle(m_state.hEvent);
    m_state.hEvent = 0;

    m_handler = Function<void()>();
}

} // namespace IPC
