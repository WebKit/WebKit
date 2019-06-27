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

#include <mutex>
#include <ws2tcpip.h>

namespace Inspector {

namespace Socket {

void init()
{
    static std::once_flag flag;
    std::call_once(flag, [] {
        WSADATA data;
        WORD versionRequested = MAKEWORD(2, 0);

        WSAStartup(versionRequested, &data);
    });
}

class Socket {
public:
    Socket()
        : m_socket(create())
    {
    }

    explicit Socket(PlatformSocketType socket)
        : m_socket(socket)
    {
    }

    explicit Socket(const Socket&) = delete;
    ~Socket()
    {
        close();
    }

    void close()
    {
        if (isValid(m_socket)) {
            ::closesocket(m_socket);
            m_socket = INVALID_SOCKET_VALUE;
        }
    }

    operator PlatformSocketType() const { return m_socket; }
    operator bool() const { return isValid(m_socket); }

    PlatformSocketType leak()
    {
        auto socket = m_socket;
        m_socket = INVALID_SOCKET_VALUE;
        return socket;
    }

private:
    static PlatformSocketType create()
    {
        PlatformSocketType socket = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (socket == INVALID_SOCKET) {
            LOG_ERROR("socket() failed, errno = %d", WSAGetLastError());
            return INVALID_SOCKET_VALUE;
        }
        return socket;
    }

    PlatformSocketType m_socket { INVALID_SOCKET_VALUE };
};

static bool setOpt(PlatformSocketType socket, int optname, const void* optval, int optlen, const char* debug)
{
    int error = ::setsockopt(socket, SOL_SOCKET, optname, static_cast<const char*>(optval), optlen);
    if (error < 0) {
        LOG_ERROR("setsockopt() %s, errno = %d", debug, WSAGetLastError());
        return false;
    }
    return true;
}

static bool setOptEnabled(PlatformSocketType socket, int optname, bool flag, const char* debug)
{
    const int val = flag;
    return setOpt(socket, optname, &val, sizeof(val), debug);
}

static bool enableOpt(PlatformSocketType socket, int optname, const char* debug)
{
    return setOptEnabled(socket, optname, true, debug);
}

static PlatformSocketType connectTo(struct sockaddr_in& address)
{
    Socket socket;
    if (!socket)
        return INVALID_SOCKET_VALUE;

    int error = ::connect(socket, (struct sockaddr*)&address, sizeof(address));
    if (error < 0)
        return INVALID_SOCKET_VALUE;

    return socket.leak();
}

static PlatformSocketType bindAndListen(struct sockaddr_in& address)
{
    Socket socket;
    if (!socket)
        return INVALID_SOCKET_VALUE;

    if (!enableOpt(socket, SO_REUSEADDR, "SO_REUSEADDR"))
        return INVALID_SOCKET_VALUE;

    // WinSock doesn't have `SO_REUSEPORT`.

    int error = ::bind(socket, (struct sockaddr*)&address, sizeof(address));
    if (error < 0) {
        LOG_ERROR("bind() failed, errno = %d", WSAGetLastError());
        return INVALID_SOCKET_VALUE;
    }

    error = ::listen(socket, 1);
    if (error < 0) {
        LOG_ERROR("listen() failed, errno = %d", WSAGetLastError());
        return INVALID_SOCKET_VALUE;
    }

    return socket.leak();
}

Optional<PlatformSocketType> connect(const char* serverAddress, uint16_t serverPort)
{
    struct sockaddr_in address = { };

    address.sin_family = AF_INET;
    ::inet_pton(AF_INET, serverAddress, &address.sin_addr);
    address.sin_port = htons(serverPort);

    auto socket = connectTo(address);
    if (!isValid(socket)) {
        LOG_ERROR("Failed to connect to %s:%u, errno = %d", serverAddress, serverPort, WSAGetLastError());
        return WTF::nullopt;
    }

    return socket;
}

Optional<PlatformSocketType> listen(const char* addressStr, uint16_t port)
{
    // FIXME: Support AF_INET6 connections.
    struct sockaddr_in address = { };
    address.sin_family = AF_INET;
    if (addressStr && *addressStr)
        ::inet_pton(AF_INET, addressStr, &address.sin_addr);
    else
        address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);

    auto socket = bindAndListen(address);
    if (!isValid(socket))
        return WTF::nullopt;

    return socket;
}

Optional<PlatformSocketType> accept(PlatformSocketType socket)
{
    struct sockaddr_in address = { };

    socklen_t len = sizeof(struct sockaddr_in);
    int fd = ::accept(socket, (struct sockaddr*) &address, &len);
    if (fd >= 0)
        return fd;

    LOG_ERROR("accept(inet) error (errno = %d)", WSAGetLastError());
    return WTF::nullopt;
}

Optional<std::array<PlatformSocketType, 2>> createPair()
{
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    address.sin_port = 0;

    Socket server(bindAndListen(address));
    if (!server)
        return WTF::nullopt;

    address.sin_port = getPort(server);

    std::array<PlatformSocketType, 2> sockets;
    sockets[0] = connectTo(address);
    if (!isValid(sockets[0]))
        return WTF::nullopt;

    if (auto socket = accept(server)) {
        sockets[1] = *socket;
        return sockets;
    }

    return WTF::nullopt;
}

void setup(PlatformSocketType socket)
{
    setOpt(socket, SO_RCVBUF, &BufferSize, sizeof(BufferSize), "SO_RCVBUF");
    setOpt(socket, SO_SNDBUF, &BufferSize, sizeof(BufferSize), "SO_SNDBUF");
}

bool isValid(PlatformSocketType socket)
{
    return socket != INVALID_SOCKET_VALUE;
}

bool isListening(PlatformSocketType socket)
{
    int out;
    socklen_t outSize = sizeof(out);
    if (getsockopt(socket, SOL_SOCKET, SO_ACCEPTCONN, reinterpret_cast<char*>(&out), &outSize) != -1)
        return out;

    LOG_ERROR("getsockopt errno = %d", WSAGetLastError());
    return false;
}

uint16_t getPort(PlatformSocketType socket)
{
    ASSERT(isValid(socket));

    struct sockaddr_in address = { };
    int len = sizeof(address);
    getsockname(socket, reinterpret_cast<struct sockaddr*>(&address), &len);
    return address.sin_port;
}

Optional<size_t> read(PlatformSocketType socket, void* buffer, int bufferSize)
{
    ASSERT(isValid(socket));

    auto readSize = ::recv(socket, reinterpret_cast<char*>(buffer), bufferSize, 0);
    if (readSize != SOCKET_ERROR)
        return static_cast<size_t>(readSize);

    LOG_ERROR("read error (errno = %d)", WSAGetLastError());
    return WTF::nullopt;
}

Optional<size_t> write(PlatformSocketType socket, const void* data, int size)
{
    ASSERT(isValid(socket));

    auto writeSize = ::send(socket, reinterpret_cast<const char*>(data), size, 0);
    if (writeSize != SOCKET_ERROR)
        return static_cast<size_t>(writeSize);

    LOG_ERROR("write error (errno = %d)", WSAGetLastError());
    return WTF::nullopt;
}

void close(PlatformSocketType& socket)
{
    Socket(socket).close();
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
    int ret = ::WSAPoll(pollDescriptors.data(), pollDescriptors.size(), timeout);
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
