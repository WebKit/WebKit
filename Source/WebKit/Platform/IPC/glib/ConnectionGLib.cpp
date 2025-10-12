/*
 * Copyright (C) 2025 Igalia S.L.
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

#if USE(GLIB)
#include "IPCUtilities.h"
#include "Logging.h"
#include "UnixMessage.h"
#include <gio/gio.h>
#include <gio/gunixconnection.h>
#include <gio/gunixfdmessage.h>
#include <sys/socket.h>
#include <wtf/StdLibExtras.h>
#include <wtf/UniStdExtras.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

#if OS(ANDROID)
#include <android/hardware_buffer.h>
#include <wtf/SafeStrerror.h>
#endif

namespace IPC {

static constexpr size_t s_messageMaxSize = 4096;
static constexpr size_t s_attachmentMaxAmount = 254;

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

#if OS(ANDROID)
    enum class Type : uint8_t {
        Unset = 0,
        FileDescriptor,
        HardwareBuffer,
    };

    Type type() const { return m_type; }
    void setType(Type type) { m_type = type; }
#endif // OS(ANDROID)

private:
    // The AttachmentInfo will be copied using memcpy, so all members must be trivially copyable.
    bool m_isNull;
#if OS(ANDROID)
    Type m_type;
#endif
};

static_assert(sizeof(MessageInfo) + sizeof(AttachmentInfo) * s_attachmentMaxAmount <= s_messageMaxSize, "s_messageMaxSize is too small.");

void Connection::platformInitialize(Identifier&& identifier)
{
    GUniqueOutPtr<GError> error;
    m_socket = adoptGRef(g_socket_new_from_fd(identifier.handle.release(), &error.outPtr()));
    if (!m_socket) {
        // Note: g_socket_new_from_fd() takes ownership of the fd only on success, so if this error
        // were not fatal, we would need to close it here.
        g_error("Failed to adopt IPC::Connection socket: %s", error->message);
    }
    g_socket_set_blocking(m_socket.get(), FALSE);

    m_cancellable = adoptGRef(g_cancellable_new());
    m_readBuffer.reserveInitialCapacity(s_messageMaxSize);
    m_fileDescriptors.reserveInitialCapacity(s_attachmentMaxAmount);
}

void Connection::platformInvalidate()
{
    GUniqueOutPtr<GError> error;
    g_socket_close(m_socket.get(), &error.outPtr());
    if (error)
        RELEASE_LOG_ERROR(IPC, "Failed to close WebKit IPC socket: %s", error->message);

    if (!m_isConnected)
        return;

    g_cancellable_cancel(m_cancellable.get());
    m_readSocketMonitor.stop();
    m_writeSocketMonitor.stop();

    m_isConnected = false;
}

std::unique_ptr<Decoder> Connection::createMessageDecoder()
{
    if (m_readBuffer.size() < sizeof(MessageInfo)) {
        RELEASE_LOG_FAULT(IPC, "createMessageDecoder: read buffer size is smaller than MessageInfo");
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto messageData = m_readBuffer.mutableSpan();
    auto& messageInfo = consumeAndReinterpretCastTo<MessageInfo>(messageData);
    if (messageInfo.attachmentCount() > s_attachmentMaxAmount || (!messageInfo.isBodyOutOfLine() && messageInfo.bodySize() > s_messageMaxSize)) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto attachmentCount = messageInfo.attachmentCount();
    if (!attachmentCount)
        return Decoder::create(messageData.first(messageInfo.bodySize()), { });

    if (messageInfo.isBodyOutOfLine())
        attachmentCount--;

    Vector<Attachment> attachments(attachmentCount);
    size_t fdIndex = 0;
    for (size_t i = 0; i < attachmentCount; ++i) {
        auto& attachmentInfo = consumeAndReinterpretCastTo<AttachmentInfo>(messageData);
        size_t attachmentIndex = attachmentCount - i - 1;
#if OS(ANDROID)
        switch (attachmentInfo.type()) {
        case AttachmentInfo::Type::FileDescriptor:
            if (attachmentInfo.isNull())
                attachments[attachmentIndex] = UnixFileDescriptor();
            else
                attachments[attachmentIndex] = WTFMove(m_fileDescriptors[fdIndex++]);
            break;
        case AttachmentInfo::Type::HardwareBuffer:
            if (attachmentInfo.isNull())
                attachments[attachmentIndex] = nullptr;
            else {
                RELEASE_ASSERT(!m_incomingHardwareBuffers.isEmpty());
                attachments[attachmentIndex] = WTFMove(m_incomingHardwareBuffers.first());
                m_incomingHardwareBuffers.removeAt(0);
            }
            break;
        case AttachmentInfo::Type::Unset:
            RELEASE_ASSERT_NOT_REACHED();
        }
#else
        if (!attachmentInfo.isNull())
            attachments[attachmentIndex] = WTFMove(m_fileDescriptors[fdIndex++]);
#endif
    }

    if (!messageInfo.isBodyOutOfLine())
        return Decoder::create(messageData.first(messageInfo.bodySize()), WTFMove(attachments));

    ASSERT(messageInfo.bodySize());
    auto& attachmentInfo = reinterpretCastSpanStartTo<AttachmentInfo>(messageData);
    if (attachmentInfo.isNull()) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    auto handle = WebCore::SharedMemory::Handle { WTFMove(m_fileDescriptors[fdIndex]), messageInfo.bodySize() };
    auto messageBody = WebCore::SharedMemory::map(WTFMove(handle), WebCore::SharedMemory::Protection::ReadOnly);
    if (!messageBody) {
        ASSERT_NOT_REACHED();
        return nullptr;
    }

    return Decoder::create(messageBody->mutableSpan().first(messageInfo.bodySize()), WTFMove(attachments));
}

IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage")
static ssize_t readBytesFromSocket(GSocket* socket, Vector<uint8_t>& buffer, Vector<UnixFileDescriptor>& fileDescriptors, GCancellable* cancellable, GError** error)
{
    GUniqueOutPtr<GSocketControlMessage*> messages;
    gint messagesCount = 0;
    gint flags = 0;
    GInputVector inputVector = { buffer.mutableSpan().data(), buffer.size() };
    auto bytesRead = g_socket_receive_message(socket, nullptr, &inputVector, 1, &messages.outPtr(), &messagesCount, &flags, cancellable, error);
    if (bytesRead <= 0)
        return bytesRead;

    if (flags & MSG_CTRUNC) {
        // Control data has been discarded, so consider this a read failure.
        return -1;
    }

    buffer.shrink(bytesRead);
    for (int i = 0; i < messagesCount; ++i) {
        GRefPtr<GSocketControlMessage> controlMessage = adoptGRef(G_SOCKET_CONTROL_MESSAGE(messages.get()[i]));
        if (!G_IS_UNIX_FD_MESSAGE(controlMessage.get()))
            continue;

        gint fdsCount;
        GUniquePtr<gint> fds(g_unix_fd_message_steal_fds(G_UNIX_FD_MESSAGE(controlMessage.get()), &fdsCount));
        for (int i = 0; i < fdsCount; ++i) {
            int fd = fds.get()[i];
            if (!setCloseOnExec(fd)) {
                ASSERT_NOT_REACHED();
                break;
            }

            fileDescriptors.append(UnixFileDescriptor { fd, UnixFileDescriptor::Adopt });
        }
    }

    return bytesRead;
}
IGNORE_CLANG_WARNINGS_END

void Connection::readyReadHandler()
{
#if OS(ANDROID)
    if (m_pendingIncomingHardwareBufferCount) {
        if (!receiveIncomingHardwareBuffers())
            return;

        if (auto decoder = createMessageDecoder())
            processIncomingMessage(makeUniqueRefFromNonNullUniquePtr(WTFMove(decoder)));
    }
#endif

    while (true) {
        m_readBuffer.grow(m_readBuffer.capacity());
        m_fileDescriptors.shrink(0);

        GUniqueOutPtr<GError> error;
        auto bytesRead = readBytesFromSocket(m_socket.get(), m_readBuffer, m_fileDescriptors, m_cancellable.get(), &error.outPtr());
        if (bytesRead < 0) {
            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK))
                return;

            if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED) || g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
                connectionDidClose();
                return;
            }

            if (m_isConnected) {
                RELEASE_LOG_ERROR(IPC, "Error receiving IPC message on socket %d in process %d: %s", g_socket_get_fd(m_socket.get()), getpid(), error->message);
                connectionDidClose();
            }
            return;
        }

        if (!bytesRead) {
            connectionDidClose();
            return;
        }

#if OS(ANDROID)
        RELEASE_ASSERT(m_readBuffer.size() >= sizeof(MessageInfo));
        const auto& messageInfo = reinterpretCastSpanStartTo<MessageInfo>(m_readBuffer.span());
        if (auto hardwareBufferCount = messageInfo.hardwareBufferCount()) {
            RELEASE_ASSERT(m_incomingHardwareBuffers.isEmpty());
            RELEASE_ASSERT(!m_pendingIncomingHardwareBufferCount);
            m_pendingIncomingHardwareBufferCount = hardwareBufferCount;
            if (!receiveIncomingHardwareBuffers())
                return;
        }
#endif // OS(ANDROID)

        if (auto decoder = createMessageDecoder())
            processIncomingMessage(makeUniqueRefFromNonNullUniquePtr(WTFMove(decoder)));
    }
}

bool Connection::platformPrepareForOpen()
{
    return true;
}

void Connection::platformOpen()
{
    RefPtr<Connection> protectedThis(this);
    m_isConnected = true;

    m_readSocketMonitor.start(m_socket.get(), G_IO_IN, m_connectionQueue->runLoop(), m_cancellable.get(), [protectedThis] (GIOCondition condition) -> gboolean {
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

    // Schedule a call to readyReadHandler. Data may have arrived before installation of the signal handler.
    m_connectionQueue->dispatch([protectedThis] {
        protectedThis->readyReadHandler();
    });
}

bool Connection::platformCanSendOutgoingMessages() const
{
#if OS(ANDROID)
    return !m_hasPendingOutputMessage && m_outgoingHardwareBuffers.isEmpty();
#else
    return !m_hasPendingOutputMessage;
#endif
}

bool Connection::sendOutgoingMessage(UniqueRef<Encoder>&& encoder)
{
    static_assert(sizeof(MessageInfo) + s_attachmentMaxAmount * sizeof(size_t) <= s_messageMaxSize, "Attachments fit to message inline");

    UnixMessage outputMessage(encoder.get());
    if (outputMessage.attachments().size() > (s_attachmentMaxAmount - 1)) {
        ASSERT_NOT_REACHED();
        return false;
    }

    size_t messageSizeWithBodyInline = sizeof(MessageInfo) + (outputMessage.attachments().size() * sizeof(AttachmentInfo)) + outputMessage.bodySize();
    if (messageSizeWithBodyInline > s_messageMaxSize && outputMessage.bodySize()) {
        if (!outputMessage.setBodyOutOfLine())
            return false;
    }

    return sendOutputMessage(WTFMove(outputMessage));
}

IGNORE_CLANG_WARNINGS_BEGIN("unsafe-buffer-usage")
bool Connection::sendOutputMessage(UnixMessage&& outputMessage)
{
#if OS(ANDROID)
    RELEASE_ASSERT(m_outgoingHardwareBuffers.isEmpty());
    Vector<RefPtr<AHardwareBuffer>, 2> hardwareBuffers;
#endif
    ASSERT(!m_hasPendingOutputMessage);

    auto& messageInfo = outputMessage.messageInfo();
    const auto& attachments = outputMessage.attachments();
    GOutputVector outputVector[3];
    int outputVectorLength = 0;

    outputVector[outputVectorLength++] = { reinterpret_cast<void*>(&messageInfo), sizeof(messageInfo) };
    GRefPtr<GSocketControlMessage> controlMessage;
    Vector<AttachmentInfo> attachmentInfo;
    if (!attachments.isEmpty()) {
        attachmentInfo.resize(attachments.size());
        Vector<int> fds;
        fds.reserveInitialCapacity(attachments.size());
        for (size_t i = 0; i < attachments.size(); ++i) {
#if OS(ANDROID)
            RELEASE_ASSERT(attachmentInfo[i].type() == AttachmentInfo::Type::Unset);
            switchOn(attachments[i],
                [&](const UnixFileDescriptor& fd) {
                    attachmentInfo[i].setType(AttachmentInfo::Type::FileDescriptor);
                    if (fd)
                        fds.append(fd.value());
                    else
                        attachmentInfo[i].setNull();
                },
                [&](const RefPtr<AHardwareBuffer>& buffer) {
                    attachmentInfo[i].setType(AttachmentInfo::Type::HardwareBuffer);
                    if (buffer)
                        hardwareBuffers.append(buffer);
                    else
                        attachmentInfo[i].setNull();
                }
            );
#else
            if (attachments[i])
                fds.append(attachments[i].value());
            else
                attachmentInfo[i].setNull();
#endif
        }

        if (!fds.isEmpty()) {
            // Use g_unix_fd_message_new_with_fd_list() to create the message without duplicating the file descriptors.
            GRefPtr<GUnixFDList> fdList = adoptGRef(g_unix_fd_list_new_from_array(fds.span().data(), fds.size()));
            controlMessage = adoptGRef(g_unix_fd_message_new_with_fd_list(fdList.get()));
        }

        outputVector[outputVectorLength++] = { attachmentInfo.mutableSpan().data(), sizeof(AttachmentInfo) * attachments.size() };
    }

    if (!messageInfo.isBodyOutOfLine() && outputMessage.bodySize())
        outputVector[outputVectorLength++] = { reinterpret_cast<void*>(outputMessage.body().data()), outputMessage.bodySize() };

    auto* controlMessagePtr = controlMessage.get();
    GUniqueOutPtr<GError> error;
    auto bytesWritten = g_socket_send_message(m_socket.get(), nullptr, outputVector, outputVectorLength,
        controlMessagePtr ? &controlMessagePtr : nullptr, controlMessagePtr ? 1 : 0, 0, m_cancellable.get(), &error.outPtr());
    if (controlMessage) {
        // File descriptors are owned by UnixMessage, so steal them from the control message to avoid a double close.
        g_free(g_unix_fd_message_steal_fds(G_UNIX_FD_MESSAGE(controlMessage.get()), nullptr));
    }
    if (bytesWritten >= 0) {
#if OS(ANDROID)
        RELEASE_ASSERT(m_outgoingHardwareBuffers.isEmpty());
        m_outgoingHardwareBuffers = WTFMove(hardwareBuffers);
        if (!sendOutgoingHardwareBuffers())
            return false;
#endif
        return true;
    }

    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_WOULD_BLOCK)) {
        m_hasPendingOutputMessage = true;
        m_writeSocketMonitor.start(m_socket.get(), G_IO_OUT, m_connectionQueue->runLoop(), m_cancellable.get(), [this, protectedThis = Ref { *this }, message = WTFMove(outputMessage)] (GIOCondition condition) mutable -> gboolean {
            if (condition & G_IO_OUT) {
                ASSERT(m_hasPendingOutputMessage);
                // We can't stop the monitor from this lambda, because stop destroys the lambda.
                m_connectionQueue->dispatch([this, protectedThis = Ref { *this }, message = WTFMove(message)]() mutable {
                    m_writeSocketMonitor.stop();
                    m_hasPendingOutputMessage = false;
                    if (m_isConnected) {
                        sendOutputMessage(WTFMove(message));
                        sendOutgoingMessages();
                    }
                });
            }
            return G_SOURCE_REMOVE;
        });
        return false;
    }

    if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED) || g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED)) {
        connectionDidClose();
        return false;
    }

    if (m_isConnected)
        RELEASE_LOG_ERROR(IPC, "Error sending IPC message on socket %d in process %d: %s", g_socket_get_fd(m_socket.get()), getpid(), error->message);
    return false;
}
IGNORE_CLANG_WARNINGS_END

std::optional<Connection::ConnectionIdentifierPair> Connection::createConnectionIdentifierPair()
{
    SocketPair socketPair = createPlatformConnection(SOCK_SEQPACKET);
    return { { Identifier { WTFMove(socketPair.server) }, ConnectionHandle { WTFMove(socketPair.client) } } };
}

void Connection::sendCredentials() const
{
    ASSERT(m_socket);
    g_socket_set_blocking(m_socket.get(), TRUE);
    GRefPtr<GUnixConnection> connection = adoptGRef(G_UNIX_CONNECTION(g_object_new(G_TYPE_UNIX_CONNECTION, "socket", m_socket.get(), nullptr)));
    GUniqueOutPtr<GError> error;
    if (!g_unix_connection_send_credentials(connection.get(), m_cancellable.get(), &error.outPtr())) {
        if (g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CONNECTION_CLOSED) || g_error_matches(error.get(), G_IO_ERROR, G_IO_ERROR_CANCELLED))
            return;

        g_error("Connection: Failed to send crendentials: %s", error->message);
    }
    g_socket_set_blocking(m_socket.get(), FALSE);
}

pid_t Connection::remoteProcessID(GSocket* socket)
{
    GRefPtr<GUnixConnection> connection = adoptGRef(G_UNIX_CONNECTION(g_object_new(G_TYPE_UNIX_CONNECTION, "socket", socket, nullptr)));
    GUniqueOutPtr<GError> error;
    GRefPtr<GCredentials> credentials = adoptGRef(g_unix_connection_receive_credentials(connection.get(), nullptr, &error.outPtr()));
    if (!credentials)
        g_error("Connection: failed to receive credentials: %s", error->message);

    pid_t processID = g_credentials_get_unix_pid(credentials.get(), &error.outPtr());
    if (error)
        g_error("Connection: failed to get pid from credentials: %s", error->message);

    return processID;
}

#if OS(ANDROID)
bool Connection::sendOutgoingHardwareBuffers()
{
    while (!m_outgoingHardwareBuffers.isEmpty()) {
        auto& buffer = m_outgoingHardwareBuffers.first();
        RELEASE_ASSERT(buffer);

        // There is no need to check for EINTR, it is handled internally.
        int result = AHardwareBuffer_sendHandleToUnixSocket(buffer.get(), g_socket_get_fd(m_socket.get()));
        if (!result) {
            m_outgoingHardwareBuffers.removeAt(0);
            continue;
        }

        if (result == -EAGAIN || result == -EWOULDBLOCK) {
            m_writeSocketMonitor.start(m_socket.get(), G_IO_OUT, m_connectionQueue->runLoop(), m_cancellable.get(), [this, protectedThis = Ref { *this }] (GIOCondition condition) -> gboolean {
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

        if (result == -EPIPE || result == -ECONNRESET || g_cancellable_is_cancelled(m_cancellable.get())) {
            connectionDidClose();
            return false;
        }

        if (m_isConnected) {
            LOG_ERROR("Error sending AHardwareBuffer on socket %d in process %d: %s", g_socket_get_fd(m_socket.get()), getpid(), safeStrerror(-result).data());
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
        int result = AHardwareBuffer_recvHandleFromUnixSocket(g_socket_get_fd(m_socket.get()), &buffer);
        if (!result) {
            m_pendingIncomingHardwareBufferCount--;
            auto hardwareBuffer = adoptRef(buffer);
            m_incomingHardwareBuffers.append(WTFMove(hardwareBuffer));
            continue;
        }

        if (result == -EAGAIN || result == -EWOULDBLOCK)
            return false;

        if (result == -ECONNRESET || g_cancellable_is_cancelled(m_cancellable.get())) {
            connectionDidClose();
            return false;
        }

        if (m_isConnected) {
            LOG_ERROR("Error receiving AHardwareBuffer on socket %d in process %d: %s", g_socket_get_fd(m_socket.get()), getpid(), safeStrerror(-result).data());
            connectionDidClose();
        }
        return false;
    }

    return true;
}
#endif // OS(ANDROID)

} // namespace IPC

#endif // USE(GLIB)
