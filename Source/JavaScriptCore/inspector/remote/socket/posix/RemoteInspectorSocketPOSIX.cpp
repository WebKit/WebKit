/*
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "RemoteInspectorSocket.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <wtf/UniStdExtras.h>

namespace Inspector {

namespace Socket {

void init()
{
}

Optional<PlatformSocketType> connect(const char* serverAddress, uint16_t serverPort)
{
    struct sockaddr_in address = { };

    address.sin_family = AF_INET;
    inet_aton(serverAddress, &address.sin_addr);
    address.sin_port = htons(serverPort);

    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        LOG_ERROR("Failed to create socket for  %s:%d, errno = %d", serverAddress, serverPort, errno);
        return WTF::nullopt;
    }

    int error = ::connect(fd, (struct sockaddr*)&address, sizeof(address));
    if (error < 0) {
        LOG_ERROR("Failed to connect to %s:%u, errno = %d", serverAddress, serverPort, errno);
        ::close(fd);
        return WTF::nullopt;
    }

    return fd;
}

Optional<PlatformSocketType> listen(const char* addressStr, uint16_t port)
{
    struct sockaddr_in address = { };

    int fdListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fdListen < 0) {
        LOG_ERROR("socket() failed, errno = %d", errno);
        return WTF::nullopt;
    }

    const int enabled = 1;
    int error = setsockopt(fdListen, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    if (error < 0) {
        LOG_ERROR("setsockopt() SO_REUSEADDR, errno = %d", errno);
        return WTF::nullopt;
    }
    error = setsockopt(fdListen, SOL_SOCKET, SO_REUSEPORT, &enabled, sizeof(enabled));
    if (error < 0) {
        LOG_ERROR("setsockopt() SO_REUSEPORT, errno = %d", errno);
        return WTF::nullopt;
    }

    // FIXME: Support AF_INET6 connections.
    address.sin_family = AF_INET;
    if (addressStr && *addressStr)
        inet_aton(addressStr, &address.sin_addr);
    else
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    error = ::bind(fdListen, (struct sockaddr*)&address, sizeof(address));
    if (error < 0) {
        LOG_ERROR("bind() failed, errno = %d", errno);
        ::close(fdListen);
        return WTF::nullopt;
    }

    error = ::listen(fdListen, 1);
    if (error < 0) {
        LOG_ERROR("listen() failed, errno = %d", errno);
        ::close(fdListen);
        return WTF::nullopt;
    }

    return fdListen;
}

Optional<PlatformSocketType> accept(PlatformSocketType socket)
{
    struct sockaddr_in address = { };

    socklen_t len = sizeof(struct sockaddr_in);
    int fd = ::accept(socket, (struct sockaddr*) &address, &len);
    if (fd >= 0)
        return fd;

    LOG_ERROR("accept(inet) error (errno = %d)", errno);
    return WTF::nullopt;
}

Optional<std::array<PlatformSocketType, 2>> createPair()
{
    std::array<PlatformSocketType, 2> sockets;

    if (socketpair(AF_UNIX, SOCK_STREAM, 0, &sockets[0]))
        return WTF::nullopt;

    return sockets;
}

void setup(PlatformSocketType socket)
{
    setCloseOnExec(socket);
    setNonBlock(socket);

    setsockopt(socket, SOL_SOCKET, SO_RCVBUF, &BufferSize, sizeof(BufferSize));
    setsockopt(socket, SOL_SOCKET, SO_SNDBUF, &BufferSize, sizeof(BufferSize));
}

bool isValid(PlatformSocketType socket)
{
    return socket != INVALID_SOCKET_VALUE;
}

bool isListening(PlatformSocketType socket)
{
    int out;
    socklen_t outSize = sizeof(out);
    if (getsockopt(socket, SOL_SOCKET, SO_ACCEPTCONN, &out, &outSize) != -1)
        return out;

    LOG_ERROR("getsockopt errno = %d", errno);
    return false;
}

uint16_t getPort(PlatformSocketType socket)
{
    ASSERT(isValid(socket));

    struct sockaddr_in address = { };
    socklen_t len = sizeof(address);
    getsockname(socket, reinterpret_cast<struct sockaddr*>(&address), &len);
    return address.sin_port;
}

Optional<size_t> read(PlatformSocketType socket, void* buffer, int bufferSize)
{
    ASSERT(isValid(socket));

    ssize_t readSize = ::read(socket, buffer, bufferSize);
    if (readSize >= 0)
        return static_cast<size_t>(readSize);

    LOG_ERROR("read error (errno = %d)", errno);
    return WTF::nullopt;
}

Optional<size_t> write(PlatformSocketType socket, const void* data, int size)
{
    ASSERT(isValid(socket));

    ssize_t writeSize = ::write(socket, data, size);
    if (writeSize >= 0)
        return static_cast<size_t>(writeSize);

    LOG_ERROR("write error (errno = %d)", errno);
    return WTF::nullopt;
}

void close(PlatformSocketType& socket)
{
    if (!isValid(socket))
        return;

    ::close(socket);
    socket = INVALID_SOCKET_VALUE;
}

PollingDescriptor preparePolling(PlatformSocketType socket)
{
    PollingDescriptor poll;
    poll.fd = socket;
    poll.events = POLLIN;
    return poll;
}

bool poll(Vector<PollingDescriptor>& pollDescriptors, int timeout)
{
    int ret = ::poll(pollDescriptors.data(), pollDescriptors.size(), timeout);
    return ret > 0;
}

bool isReadable(const PollingDescriptor& poll)
{
    return poll.revents & POLLIN;
}

bool isWritable(const PollingDescriptor& poll)
{
    return poll.revents & POLLOUT;
}

void markWaitingWritable(PollingDescriptor& poll)
{
    poll.events |= POLLOUT;
}

void clearWaitingWritable(PollingDescriptor& poll)
{
    poll.events &= ~POLLOUT;
}

} // namespace Socket

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
