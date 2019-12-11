/*
 * Copyright (C) 2019 Igalia, S.L.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "SocketConnection.h"

#include <cstring>
#include <gio/gio.h>
#include <wtf/RunLoop.h>

namespace WTF {

static const unsigned defaultBufferSize = 4096;

SocketConnection::SocketConnection(GRefPtr<GSocketConnection>&& connection, const MessageHandlers& messageHandlers, gpointer userData)
    : m_connection(WTFMove(connection))
    , m_messageHandlers(messageHandlers)
    , m_userData(userData)
{
    m_readBuffer.reserveInitialCapacity(defaultBufferSize);
    m_writeBuffer.reserveInitialCapacity(defaultBufferSize);

    auto* socket = g_socket_connection_get_socket(m_connection.get());
    g_socket_set_blocking(socket, FALSE);
    m_readMonitor.start(socket, G_IO_IN, RunLoop::current(), [this, protectedThis = makeRef(*this)](GIOCondition condition) -> gboolean {
        if (isClosed())
            return G_SOURCE_REMOVE;

        if (condition & G_IO_HUP || condition & G_IO_ERR || condition & G_IO_NVAL) {
            didClose();
            return G_SOURCE_REMOVE;
        }

        ASSERT(condition & G_IO_IN);
        return read();
    });
}

SocketConnection::~SocketConnection()
{
}

bool SocketConnection::read()
{
    while (true) {
        size_t previousBufferSize = m_readBuffer.size();
        if (m_readBuffer.capacity() - previousBufferSize <= 0)
            m_readBuffer.reserveCapacity(m_readBuffer.capacity() + defaultBufferSize);
        m_readBuffer.grow(m_readBuffer.capacity());
        GUniqueOutPtr<GError> error;
        auto bytesRead = g_socket_receive(g_socket_connection_get_socket(m_connection.get()), m_readBuffer.data() + previousBufferSize, m_readBuffer.size() - previousBufferSize, nullptr, &error.outPtr());
        if (bytesRead == -1) {
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
                m_readBuffer.shrink(previousBufferSize);
                break;
            }

            g_warning("Error reading from socket connection: %s\n", error->message);
            didClose();
            return G_SOURCE_REMOVE;
        }

        if (!bytesRead) {
            didClose();
            return G_SOURCE_REMOVE;
        }

        m_readBuffer.shrink(previousBufferSize + bytesRead);

        while (readMessage()) { }
        if (isClosed())
            return G_SOURCE_REMOVE;
    }
    return G_SOURCE_CONTINUE;
}

bool SocketConnection::readMessage()
{
    if (m_readBuffer.size() < sizeof(size_t))
        return false;

    auto* messageData = m_readBuffer.data();
    size_t bodySize;
    memcpy(&bodySize, messageData, sizeof(size_t));
    messageData += sizeof(size_t);
    auto messageSize = sizeof(size_t) + bodySize;
    if (m_readBuffer.size() < messageSize)
        return false;

    auto messageNameLength = strlen(messageData) + 1;
    if (m_readBuffer.size() < messageNameLength) {
        ASSERT_NOT_REACHED();
        return false;
    }

    const auto it = m_messageHandlers.find(messageData);
    if (it != m_messageHandlers.end()) {
        messageData += messageNameLength;
        GRefPtr<GVariant> parameters;
        if (!it->value.first.isNull()) {
            GUniquePtr<GVariantType> variantType(g_variant_type_new(it->value.first.data()));
            parameters = g_variant_new_from_data(variantType.get(), messageData, bodySize - messageNameLength, FALSE, nullptr, nullptr);
        }
        it->value.second(*this, parameters.get(), m_userData);
        if (isClosed())
            return false;
    }

    if (m_readBuffer.size() > messageSize) {
        std::memmove(m_readBuffer.data(), m_readBuffer.data() + messageSize, m_readBuffer.size() - messageSize);
        m_readBuffer.shrink(m_readBuffer.size() - messageSize);
    } else
        m_readBuffer.shrink(0);

    if (m_readBuffer.size() < defaultBufferSize)
        m_readBuffer.shrinkCapacity(defaultBufferSize);

    return true;
}

void SocketConnection::sendMessage(const char* messageName, GVariant* parameters)
{
    GRefPtr<GVariant> adoptedParameters = parameters;
    size_t parametersSize = parameters ? g_variant_get_size(parameters) : 0;
    auto messageNameLength = strlen(messageName) + 1;
    size_t bodySize = messageNameLength + parametersSize;
    size_t previousBufferSize = m_writeBuffer.size();
    m_writeBuffer.grow(previousBufferSize + sizeof(size_t) + bodySize);

    auto* messageData = m_writeBuffer.data() + previousBufferSize;
    memcpy(messageData, &bodySize, sizeof(size_t));
    messageData += sizeof(size_t);
    memcpy(messageData, messageName, messageNameLength);
    messageData += messageNameLength;
    if (parameters)
        memcpy(messageData, g_variant_get_data(parameters), parametersSize);

    write();
}

void SocketConnection::write()
{
    if (isClosed())
        return;

    GUniqueOutPtr<GError> error;
    auto bytesWritten = g_socket_send(g_socket_connection_get_socket(m_connection.get()), m_writeBuffer.data(), m_writeBuffer.size(), nullptr, &error.outPtr());
    if (bytesWritten == -1) {
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
            waitForSocketWritability();
            return;
        }

        g_warning("Error sending message on socket connection: %s\n", error->message);
        didClose();
        return;
    }

    if (m_writeBuffer.size() > static_cast<size_t>(bytesWritten)) {
        std::memmove(m_writeBuffer.data(), m_writeBuffer.data() + bytesWritten, m_writeBuffer.size() - bytesWritten);
        m_writeBuffer.shrink(m_writeBuffer.size() - bytesWritten);
    } else
        m_writeBuffer.shrink(0);

    if (m_writeBuffer.size() < defaultBufferSize)
        m_writeBuffer.shrinkCapacity(defaultBufferSize);

    if (!m_writeBuffer.isEmpty())
        waitForSocketWritability();
}

void SocketConnection::waitForSocketWritability()
{
    if (m_writeMonitor.isActive())
        return;

    m_writeMonitor.start(g_socket_connection_get_socket(m_connection.get()), G_IO_OUT, RunLoop::current(), [this, protectedThis = makeRef(*this)] (GIOCondition condition) -> gboolean {
        if (condition & G_IO_OUT) {
            // We can't stop the monitor from this lambda, because stop destroys the lambda.
            RunLoop::current().dispatch([this, protectedThis = protectedThis.copyRef()] {
                m_writeMonitor.stop();
                write();
            });
        }
        return G_SOURCE_REMOVE;
    });
}

void SocketConnection::close()
{
    m_readMonitor.stop();
    m_writeMonitor.stop();
    m_connection = nullptr;
}

void SocketConnection::didClose()
{
    if (isClosed())
        return;

    close();
    ASSERT(m_messageHandlers.contains("DidClose"));
    m_messageHandlers.get("DidClose").second(*this, nullptr, m_userData);
}

} // namespace WTF
