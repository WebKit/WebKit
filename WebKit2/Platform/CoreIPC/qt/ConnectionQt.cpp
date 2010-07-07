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

#include "Connection.h"

#include "ArgumentEncoder.h"
#include "WebPageProxyMessageKinds.h"
#include "WorkItem.h"
#include <QApplication>
#include <QLocalServer>
#include <QLocalSocket>

using namespace std;

namespace CoreIPC {

// This is what other ports use...
static const size_t messageMaxSize = 4096;

void Connection::platformInitialize(Identifier identifier)
{
    m_serverName = "QtWebKit" + identifier;
    m_server = 0;
    m_socket = 0;
    m_readBuffer.resize(messageMaxSize);
    m_currentMessageSize = 0;
}

void Connection::platformInvalidate()
{
    delete m_server;
    delete m_socket;
}

void Connection::newConnectionHandler()
{
    m_socket = m_server->nextPendingConnection();
    m_isConnected = m_socket;
    if (m_isConnected)
        m_connectionQueue.connectSignal(m_socket, SIGNAL(readyRead()), WorkItem::create(this, &Connection::readyReadHandler));
}

void Connection::readyReadHandler()
{
    while (m_socket->bytesAvailable()) {
        if (!m_currentMessageSize) {
            size_t numberOfBytesRead = m_socket->read(reinterpret_cast<char*>(m_readBuffer.data()), sizeof(size_t));
            ASSERT(numberOfBytesRead);
            m_currentMessageSize = *reinterpret_cast<size_t*>(m_readBuffer.data());
        }

        if (m_socket->bytesAvailable() < m_currentMessageSize) {
#if 0
            printf("received message size=%d, waiting for more data\n", (int)m_currentMessageSize);
#endif
            return;
        }

        size_t numberOfBytesRead = m_socket->read(reinterpret_cast<char*>(m_readBuffer.data()), m_currentMessageSize);
        ASSERT(numberOfBytesRead);

        // The messageID is encoded at the end of the buffer.
        size_t realBufferSize = m_currentMessageSize - sizeof(MessageID);
        unsigned messageID = *reinterpret_cast<unsigned*>(m_readBuffer.data() + realBufferSize);

        std::auto_ptr<ArgumentDecoder> arguments(new ArgumentDecoder(m_readBuffer.data(), realBufferSize));
        processIncomingMessage(MessageID::fromInt(messageID), arguments);

        m_currentMessageSize = 0;
    }
}

void Connection::openConnectionHandler()
{
    ASSERT(!m_server);
    ASSERT(!m_socket);

    if (m_isServer) {
        m_server = new QLocalServer();
        if (!m_server->listen(m_serverName)) {
            qDebug() << "failed to create server " << m_serverName;
            return;
        }
        m_connectionQueue.connectSignal(m_server, SIGNAL(newConnection()), WorkItem::create(this, &Connection::newConnectionHandler));
    } else {
        m_socket = new QLocalSocket();
        m_socket->connectToServer(m_serverName);
        m_connectionQueue.connectSignal(m_socket, SIGNAL(readyRead()), WorkItem::create(this, &Connection::readyReadHandler));
        m_isConnected = m_socket->waitForConnected();
    }
}

bool Connection::open()
{
    m_connectionQueue.scheduleWork(WorkItem::create(this, &Connection::openConnectionHandler));
    return false;
}

void Connection::sendOutgoingMessage(MessageID messageID, auto_ptr<ArgumentEncoder> arguments)
{
    if (!m_socket)
        return;

    // We put the message ID last.
    arguments->encodeUInt32(messageID.toInt());

    size_t bufferSize = arguments->bufferSize();

    // Write message size first
    // FIXME: Should  just do a single write.
    m_socket->write(reinterpret_cast<char*>(&bufferSize), sizeof(bufferSize));

    qint64 bytesWritten = m_socket->write(reinterpret_cast<char*>(arguments->buffer()), arguments->bufferSize());

    ASSERT(bytesWritten == arguments->bufferSize());
}

} // namespace CoreIPC
