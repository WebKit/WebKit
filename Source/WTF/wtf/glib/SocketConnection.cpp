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
#include <wtf/ByteOrder.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/FastMalloc.h>
#include <wtf/Logging.h>
#include <wtf/RunLoop.h>
#include <wtf/glib/GSpanExtras.h>
#include <wtf/text/MakeString.h>

namespace WTF {

static const unsigned defaultBufferSize = 4096;

SocketConnection::SocketConnection(GRefPtr<GSocketConnection>&& connection, const MessageHandlers& messageHandlers, gpointer userData)
    : m_connection(WTF::move(connection))
    , m_messageHandlers(messageHandlers)
    , m_userData(userData)
{
    relaxAdoptionRequirement();

    m_readBuffer.reserveInitialCapacity(defaultBufferSize);
    m_writeBuffer.reserveInitialCapacity(defaultBufferSize);

    auto* socket = g_socket_connection_get_socket(m_connection.get());
    g_socket_set_blocking(socket, FALSE);
    m_readMonitor.start(socket, G_IO_IN, RunLoop::currentSingleton(), nullptr, [this, protectedThis = Ref { *this }](GIOCondition condition) -> gboolean {
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

SocketConnection::~SocketConnection() = default;

bool SocketConnection::didReceiveInvalidMessage(const CString& message)
{
    RELEASE_LOG_FAULT_WITH_PAYLOAD(Process, makeString("Received invalid message ("_s, message.span(), "), closing SocketConnection"_s).utf8().data());
    close();
    m_readBuffer.shrink(0);
    return false;
}

bool SocketConnection::read()
{
    while (true) {
        size_t previousBufferSize = m_readBuffer.size();
        if (m_readBuffer.capacity() - previousBufferSize <= 0)
            m_readBuffer.reserveCapacity(m_readBuffer.capacity() + defaultBufferSize);
        m_readBuffer.grow(m_readBuffer.capacity());
        GUniqueOutPtr<GError> error;
        auto bufferSpan = m_readBuffer.mutableSpan().subspan(previousBufferSize);
        auto bytesRead = g_socket_receive(g_socket_connection_get_socket(m_connection.get()), bufferSpan.data(), bufferSpan.size(), nullptr, &error.outPtr());
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

enum {
    ByteOrderLittleEndian = 1 << 0
};
typedef uint8_t MessageFlags;

// The smallest possible message has no parameters, one character for the message
// name (an empty name is invalid), and a null terminator at the end of the name.
static auto constexpr MinimumMessageBodySize = 2;
static auto constexpr MaximumMessageBodySize = 512 * MB;

static inline bool messageIsByteSwapped(MessageFlags flags)
{
#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    return !(flags & ByteOrderLittleEndian);
#else
    return (flags & ByteOrderLittleEndian);
#endif
}

#define MESSAGE_CHECK(assertion, message) do { \
    if (!(assertion)) [[unlikely]] \
        return didReceiveInvalidMessage(message); \
} while (0)

bool SocketConnection::readMessage()
{
    // Ensure we have enough data to read the message size.
    if (m_readBuffer.size() < sizeof(uint32_t))
        return false;

    auto messageData = m_readBuffer.span();
    const size_t bodySize = ntohl(consumeAndReinterpretCastTo<uint32_t>(messageData));

    MESSAGE_CHECK(bodySize >= MinimumMessageBodySize, "message body too small");
    MESSAGE_CHECK(bodySize <= MaximumMessageBodySize, "message body too big");

    // Ensure the whole message has been read from the socket.
    const size_t messageSize = sizeof(uint32_t) + sizeof(MessageFlags) + bodySize;
    if (m_readBuffer.size() < messageSize) {
        m_readBuffer.reserveCapacity(messageSize);
        return false;
    }

    const auto flags = consumeAndReinterpretCastTo<MessageFlags>(messageData);

    // Ensure that the span covers only the first message in the read buffer, and
    // that parsing the message does not step onto the next one in the buffer.
    messageData = messageData.first(bodySize);

    const auto nullIndex = find(messageData, '\0');
    MESSAGE_CHECK(nullIndex != notFound, "message name delimiter missing");

    const CString messageName(consumeSpan(messageData, nullIndex));
    ASSERT(messageData.front() == '\0');
    skip(messageData, 1);

    const auto it = m_messageHandlers.find(messageName);
    if (it != m_messageHandlers.end()) {
        GRefPtr<GVariant> parameters;
        if (!it->value.first.isNull()) {
            GUniquePtr<GVariantType> variantType(g_variant_type_new(it->value.first.data()));
            parameters = g_variant_new_from_data(variantType.get(), messageData.data(), messageData.size(), FALSE, nullptr, nullptr);
            if (messageIsByteSwapped(flags))
                parameters = adoptGRef(g_variant_byteswap(parameters.get()));
        }
        it->value.second(*this, parameters.get(), m_userData);
        if (isClosed())
            return false;
    }

    if (m_readBuffer.size() > messageSize) {
        memmoveSpan(m_readBuffer.mutableSpan(), m_readBuffer.subspan(messageSize));
        m_readBuffer.shrink(m_readBuffer.size() - messageSize);
    } else
        m_readBuffer.shrink(0);

    if (m_readBuffer.size() < defaultBufferSize)
        m_readBuffer.shrinkCapacity(defaultBufferSize);

    return true;
}

#undef MESSAGE_CHECK

void SocketConnection::sendMessage(const CString& messageName, GVariant* parameters)
{
    ASSERT(!messageName.isEmpty());

    GRefPtr<GVariant> adoptedParameters = parameters;
    size_t parametersSize = parameters ? g_variant_get_size(parameters) : 0;
    const auto messageNameAndTerminator = messageName.spanIncludingNullTerminator();
    CheckedUint32 bodySize = messageNameAndTerminator.size();
    bodySize += parametersSize;
    if (bodySize.hasOverflowed() || bodySize > MaximumMessageBodySize) [[unlikely]] {
        g_warning("Trying to send message '%s' with invalid too long body", messageName.data());
        return;
    }
    ASSERT(bodySize >= MinimumMessageBodySize);

    size_t previousBufferSize = m_writeBuffer.size();
    m_writeBuffer.grow(previousBufferSize + sizeof(uint32_t) + sizeof(MessageFlags) + bodySize.value());

    auto messageData = m_writeBuffer.mutableSpan().subspan(previousBufferSize);
    consumeAndReinterpretCastTo<uint32_t>(messageData) = htonl(bodySize);

#if G_BYTE_ORDER == G_LITTLE_ENDIAN
    consumeAndReinterpretCastTo<MessageFlags>(messageData) = ByteOrderLittleEndian;
#else
    consumeAndReinterpretCastTo<MessageFlags>(messageData) = 0;
#endif

    memcpySpan(consumeSpan(messageData, messageNameAndTerminator.size()), messageNameAndTerminator);

    ASSERT(parametersSize == messageData.size());
    if (parameters)
        memcpySpan(messageData, span(parameters));

    write();
}

void SocketConnection::write()
{
    if (isClosed())
        return;

    GUniqueOutPtr<GError> error;
    auto bytesWritten = g_socket_send(g_socket_connection_get_socket(m_connection.get()), m_writeBuffer.mutableSpan().data(), m_writeBuffer.size(), nullptr, &error.outPtr());
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
        memmoveSpan(m_writeBuffer.mutableSpan(), m_writeBuffer.subspan(bytesWritten));
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

    m_writeMonitor.start(g_socket_connection_get_socket(m_connection.get()), G_IO_OUT, RunLoop::currentSingleton(), nullptr, [this, protectedThis = Ref { *this }] (GIOCondition condition) -> gboolean {
        if (condition & G_IO_OUT) {
            // We can't stop the monitor from this lambda, because stop destroys the lambda.
            RunLoop::currentSingleton().dispatch([this, protectedThis] {
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
