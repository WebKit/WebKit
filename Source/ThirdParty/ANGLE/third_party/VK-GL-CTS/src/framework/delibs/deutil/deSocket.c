/*-------------------------------------------------------------------------
 * drawElements Utility Library
 * ----------------------------
 *
 * Copyright 2014 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 *//*!
 * \file
 * \brief Socket abstraction.
 *//*--------------------------------------------------------------------*/

#include "deSocket.h"
#include "deMemory.h"
#include "deMutex.h"
#include "deInt32.h"

#if (DE_OS == DE_OS_WIN32)
#define DE_USE_WINSOCK
#elif (DE_OS == DE_OS_UNIX) || (DE_OS == DE_OS_OSX) || (DE_OS == DE_OS_IOS) || (DE_OS == DE_OS_ANDROID) || \
    (DE_OS == DE_OS_SYMBIAN) || (DE_OS == DE_OS_QNX) || (DE_OS == DE_OS_FUCHSIA)
#define DE_USE_BERKELEY_SOCKETS
#else
#error Implement deSocket for your OS.
#endif

/* Common utilities. */

const char *deGetSocketResultName(deSocketResult result)
{
    switch (result)
    {
    case DE_SOCKETRESULT_SUCCESS:
        return "DE_SOCKETRESULT_SUCCESS";
    case DE_SOCKETRESULT_WOULD_BLOCK:
        return "DE_SOCKETRESULT_WOULD_BLOCK";
    case DE_SOCKETRESULT_CONNECTION_CLOSED:
        return "DE_SOCKETRESULT_CONNECTION_CLOSED";
    case DE_SOCKETRESULT_CONNECTION_TERMINATED:
        return "DE_SOCKETRESULT_CONNECTION_TERMINATED";
    case DE_SOCKETRESULT_ERROR:
        return "DE_SOCKETRESULT_ERROR";
    default:
        return NULL;
    }
}

const char *deGetSocketFamilyName(deSocketFamily family)
{
    switch (family)
    {
    case DE_SOCKETFAMILY_INET4:
        return "DE_SOCKETFAMILY_INET4";
    case DE_SOCKETFAMILY_INET6:
        return "DE_SOCKETFAMILY_INET6";
    default:
        return NULL;
    }
}

#if defined(DE_USE_WINSOCK) || defined(DE_USE_BERKELEY_SOCKETS)

/* Common deSocketAddress implementation. */

struct deSocketAddress_s
{
    char *host;
    int port;
    deSocketFamily family;
    deSocketType type;
    deSocketProtocol protocol;
};

deSocketAddress *deSocketAddress_create(void)
{
    deSocketAddress *addr = (deSocketAddress *)deCalloc(sizeof(deSocketAddress));
    if (!addr)
        return addr;

    /* Sane defaults. */
    addr->family   = DE_SOCKETFAMILY_INET4;
    addr->type     = DE_SOCKETTYPE_STREAM;
    addr->protocol = DE_SOCKETPROTOCOL_TCP;

    return addr;
}

bool deSocketAddress_setFamily(deSocketAddress *address, deSocketFamily family)
{
    address->family = family;
    return true;
}

deSocketFamily deSocketAddress_getFamily(const deSocketAddress *address)
{
    return address->family;
}

void deSocketAddress_destroy(deSocketAddress *address)
{
    deFree(address->host);
    deFree(address);
}

bool deSocketAddress_setPort(deSocketAddress *address, int port)
{
    address->port = port;
    return true;
}

int deSocketAddress_getPort(const deSocketAddress *address)
{
    return address->port;
}

bool deSocketAddress_setHost(deSocketAddress *address, const char *host)
{
    if (address->host)
    {
        deFree(address->host);
        address->host = NULL;
    }

    address->host = deStrdup(host);
    return address->host != NULL;
}

const char *deSocketAddress_getHost(const deSocketAddress *address)
{
    return address->host;
}

bool deSocketAddress_setType(deSocketAddress *address, deSocketType type)
{
    address->type = type;
    return true;
}

deSocketType deSocketAddress_getType(const deSocketAddress *address)
{
    return address->type;
}

bool deSocketAddress_setProtocol(deSocketAddress *address, deSocketProtocol protocol)
{
    address->protocol = protocol;
    return true;
}

deSocketProtocol deSocketAddress_getProtocol(const deSocketAddress *address)
{
    return address->protocol;
}

#endif

#if defined(DE_USE_WINSOCK)

/* WinSock spesific. */
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windef.h>

static bool initWinsock(void)
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;

    return true;
}

#elif defined(DE_USE_BERKELEY_SOCKETS)

/* Berkeley Socket includes. */
#include <sys/socket.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>

#endif

/* Socket type. */
#if defined(DE_USE_WINSOCK)
/* \note SOCKET is unsigned type! */
typedef SOCKET deSocketHandle;
typedef int NativeSocklen;
typedef int NativeSize;
#define DE_INVALID_SOCKET_HANDLE INVALID_SOCKET
#else
typedef int deSocketHandle;
typedef socklen_t NativeSocklen;
typedef size_t NativeSize;
#define DE_INVALID_SOCKET_HANDLE (-1)
#endif

bool deSocketHandleIsValid(deSocketHandle handle)
{
    return handle != DE_INVALID_SOCKET_HANDLE;
}

#if defined(DE_USE_WINSOCK) || defined(DE_USE_BERKELEY_SOCKETS)

/* Shared berkeley and winsock implementation. */

struct deSocket_s
{
    deSocketHandle handle;

    deMutex stateLock;
    volatile deSocketState state;
    volatile uint32_t openChannels;
};

/* Common socket functions. */

static uint16_t deHostToNetworkOrder16(uint16_t v)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    return deReverseBytes16(v);
#else
    return v;
#endif
}

static uint16_t deNetworkToHostOrder16(uint16_t v)
{
#if (DE_ENDIANNESS == DE_LITTLE_ENDIAN)
    return deReverseBytes16(v);
#else
    return v;
#endif
}

DE_STATIC_ASSERT(sizeof(((struct sockaddr_in *)NULL)->sin_port) == sizeof(uint16_t));
DE_STATIC_ASSERT(sizeof(((struct sockaddr_in6 *)NULL)->sin6_port) == sizeof(uint16_t));

static int deSocketFamilyToBsdFamily(deSocketFamily family)
{
    switch (family)
    {
    case DE_SOCKETFAMILY_INET4:
        return AF_INET;
    case DE_SOCKETFAMILY_INET6:
        return AF_INET6;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static int deSocketTypeToBsdType(deSocketType type)
{
    switch (type)
    {
    case DE_SOCKETTYPE_STREAM:
        return SOCK_STREAM;
    case DE_SOCKETTYPE_DATAGRAM:
        return SOCK_DGRAM;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static int deSocketProtocolToBsdProtocol(deSocketProtocol protocol)
{
    switch (protocol)
    {
    case DE_SOCKETPROTOCOL_TCP:
        return IPPROTO_TCP;
    case DE_SOCKETPROTOCOL_UDP:
        return IPPROTO_UDP;
    default:
        DE_ASSERT(false);
        return 0;
    }
}

static bool deSocketAddressToBsdAddress(const deSocketAddress *address, size_t bsdAddrBufSize, struct sockaddr *bsdAddr,
                                        NativeSocklen *bsdAddrLen)
{
    deMemset(bsdAddr, 0, bsdAddrBufSize);

    /* Resolve host. */
    if (address->host != NULL)
    {
        struct addrinfo *result = NULL;
        struct addrinfo hints;

        deMemset(&hints, 0, sizeof(hints));
        hints.ai_family   = deSocketFamilyToBsdFamily(address->family);
        hints.ai_socktype = deSocketTypeToBsdType(address->type);
        hints.ai_protocol = deSocketProtocolToBsdProtocol(address->protocol);

        if (getaddrinfo(address->host, NULL, &hints, &result) != 0 || !result)
        {
            if (result)
                freeaddrinfo(result);
            return false;
        }

        /* \note Always uses first address. */

        if (bsdAddrBufSize < (size_t)result->ai_addrlen)
        {
            DE_FATAL("Too small bsdAddr buffer");
            freeaddrinfo(result);
            return false;
        }

        *bsdAddrLen = (NativeSocklen)result->ai_addrlen;

        deMemcpy(bsdAddr, result->ai_addr, (size_t)result->ai_addrlen);
        freeaddrinfo(result);

        /* Add port. */
        if (bsdAddr->sa_family == AF_INET)
        {
            if (*bsdAddrLen < (NativeSocklen)sizeof(struct sockaddr_in))
                return false;
            ((struct sockaddr_in *)bsdAddr)->sin_port = deHostToNetworkOrder16((uint16_t)address->port);
        }
        else if (bsdAddr->sa_family == AF_INET6)
        {
            if (*bsdAddrLen < (NativeSocklen)sizeof(struct sockaddr_in6))
                return false;
            ((struct sockaddr_in6 *)bsdAddr)->sin6_port = deHostToNetworkOrder16((uint16_t)address->port);
        }
        else
            return false;

        return true;
    }
    else if (address->family == DE_SOCKETFAMILY_INET4)
    {
        struct sockaddr_in *addr4 = (struct sockaddr_in *)bsdAddr;

        if (bsdAddrBufSize < sizeof(struct sockaddr_in))
        {
            DE_FATAL("Too small bsdAddr buffer");
            return false;
        }

        addr4->sin_port        = deHostToNetworkOrder16((uint16_t)address->port);
        addr4->sin_family      = AF_INET;
        addr4->sin_addr.s_addr = INADDR_ANY;

        *bsdAddrLen = (NativeSocklen)sizeof(struct sockaddr_in);

        return true;
    }
    else if (address->family == DE_SOCKETFAMILY_INET6)
    {
        struct sockaddr_in6 *addr6 = (struct sockaddr_in6 *)bsdAddr;

        if (bsdAddrBufSize < sizeof(struct sockaddr_in6))
        {
            DE_FATAL("Too small bsdAddr buffer");
            return false;
        }

        addr6->sin6_port   = deHostToNetworkOrder16((uint16_t)address->port);
        addr6->sin6_family = AF_INET6;

        *bsdAddrLen = (NativeSocklen)sizeof(struct sockaddr_in6);

        return true;
    }
    else
        return false;
}

void deBsdAddressToSocketAddress(deSocketAddress *address, const struct sockaddr *bsdAddr, int addrLen)
{
    /* Decode client address info. */
    if (bsdAddr->sa_family == AF_INET)
    {
        const struct sockaddr_in *addr4 = (const struct sockaddr_in *)bsdAddr;
        DE_ASSERT(addrLen >= (int)sizeof(struct sockaddr_in));
        DE_UNREF(addrLen);

        deSocketAddress_setFamily(address, DE_SOCKETFAMILY_INET4);
        deSocketAddress_setPort(address, (int)deNetworkToHostOrder16((uint16_t)addr4->sin_port));

        {
            char buf[16]; /* Max valid address takes 3*4 + 3 = 15 chars */
            inet_ntop(AF_INET, (void *)&addr4->sin_addr, buf, sizeof(buf));
            deSocketAddress_setHost(address, buf);
        }
    }
    else if (bsdAddr->sa_family == AF_INET6)
    {
        const struct sockaddr_in6 *addr6 = (const struct sockaddr_in6 *)bsdAddr;
        DE_ASSERT(addrLen >= (int)sizeof(struct sockaddr_in6));
        DE_UNREF(addrLen);

        deSocketAddress_setFamily(address, DE_SOCKETFAMILY_INET6);
        deSocketAddress_setPort(address, (int)deNetworkToHostOrder16((uint16_t)addr6->sin6_port));

        {
            char buf[40]; /* Max valid address takes 8*4 + 7 = 39 chars */
            inet_ntop(AF_INET6, (void *)&addr6->sin6_addr, buf, sizeof(buf));
            deSocketAddress_setHost(address, buf);
        }
    }
    else
        DE_ASSERT(false);
}

deSocket *deSocket_create(void)
{
    deSocket *sock = (deSocket *)deCalloc(sizeof(deSocket));
    if (!sock)
        return sock;

#if defined(DE_USE_WINSOCK)
    /* Make sure WSA is up. */
    if (!initWinsock())
        return NULL;
#endif

    sock->stateLock = deMutex_create(0);
    sock->handle    = DE_INVALID_SOCKET_HANDLE;
    sock->state     = DE_SOCKETSTATE_CLOSED;

    return sock;
}

void deSocket_destroy(deSocket *sock)
{
    if (sock->state != DE_SOCKETSTATE_CLOSED)
        deSocket_close(sock);

    deMutex_destroy(sock->stateLock);
    deFree(sock);
}

deSocketState deSocket_getState(const deSocket *sock)
{
    return sock->state;
}

uint32_t deSocket_getOpenChannels(const deSocket *sock)
{
    return sock->openChannels;
}

bool deSocket_setFlags(deSocket *sock, uint32_t flags)
{
    deSocketHandle fd = sock->handle;

    if (sock->state == DE_SOCKETSTATE_CLOSED)
        return false;

    /* Keepalive. */
    {
        int mode = (flags & DE_SOCKET_KEEPALIVE) ? 1 : 0;
        if (setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (const char *)&mode, sizeof(mode)) != 0)
            return false;
    }

    /* Nodelay. */
    {
        int mode = (flags & DE_SOCKET_NODELAY) ? 1 : 0;
        if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, (const char *)&mode, sizeof(mode)) != 0)
            return false;
    }

    /* Non-blocking. */
    {
#if defined(DE_USE_WINSOCK)
        u_long mode = (flags & DE_SOCKET_NONBLOCKING) ? 1 : 0;
        if (ioctlsocket(fd, FIONBIO, &mode) != 0)
            return false;
#else
        int oldFlags = fcntl(fd, F_GETFL, 0);
        int newFlags = (flags & DE_SOCKET_NONBLOCKING) ? (oldFlags | O_NONBLOCK) : (oldFlags & ~O_NONBLOCK);
        if (fcntl(fd, F_SETFL, newFlags) != 0)
            return false;
#endif
    }

    /* Close on exec. */
    {
#if defined(DE_USE_BERKELEY_SOCKETS)
        int oldFlags = fcntl(fd, F_GETFD, 0);
        int newFlags = (flags & DE_SOCKET_CLOSE_ON_EXEC) ? (oldFlags | FD_CLOEXEC) : (oldFlags & ~FD_CLOEXEC);
        if (fcntl(fd, F_SETFD, newFlags) != 0)
            return false;
#endif
    }

    return true;
}

bool deSocket_listen(deSocket *sock, const deSocketAddress *address)
{
    const int backlogSize = 4;
    uint8_t bsdAddrBuf[sizeof(struct sockaddr_in6)];
    struct sockaddr *bsdAddr = (struct sockaddr *)&bsdAddrBuf[0];
    NativeSocklen bsdAddrLen;

    if (sock->state != DE_SOCKETSTATE_CLOSED)
        return false;

    /* Resolve address. */
    if (!deSocketAddressToBsdAddress(address, sizeof(bsdAddrBuf), bsdAddr, &bsdAddrLen))
        return false;

    /* Create socket. */
    sock->handle = socket(bsdAddr->sa_family, deSocketTypeToBsdType(address->type),
                          deSocketProtocolToBsdProtocol(address->protocol));
    if (!deSocketHandleIsValid(sock->handle))
        return false;

    sock->state = DE_SOCKETSTATE_DISCONNECTED;

    /* Allow re-using address. */
    {
        int reuseVal = 1;
        setsockopt(sock->handle, SOL_SOCKET, SO_REUSEADDR, (const char *)&reuseVal, (int)sizeof(reuseVal));
    }

    /* Bind to address. */
    if (bind(sock->handle, bsdAddr, (NativeSocklen)bsdAddrLen) != 0)
    {
        deSocket_close(sock);
        return false;
    }

    /* Start listening. */
    if (listen(sock->handle, backlogSize) != 0)
    {
        deSocket_close(sock);
        return false;
    }

    sock->state = DE_SOCKETSTATE_LISTENING;

    return true;
}

deSocket *deSocket_accept(deSocket *sock, deSocketAddress *clientAddress)
{
    deSocketHandle newFd = DE_INVALID_SOCKET_HANDLE;
    deSocket *newSock    = NULL;
    uint8_t bsdAddrBuf[sizeof(struct sockaddr_in6)];
    struct sockaddr *bsdAddr = (struct sockaddr *)&bsdAddrBuf[0];
    NativeSocklen bsdAddrLen = (NativeSocklen)sizeof(bsdAddrBuf);

    deMemset(bsdAddr, 0, (size_t)bsdAddrLen);

    newFd = accept(sock->handle, bsdAddr, &bsdAddrLen);
    if (!deSocketHandleIsValid(newFd))
        return NULL;

    newSock = (deSocket *)deCalloc(sizeof(deSocket));
    if (!newSock)
    {
#if defined(DE_USE_WINSOCK)
        closesocket(newFd);
#else
        close(newFd);
#endif
        return NULL;
    }

    newSock->stateLock    = deMutex_create(0);
    newSock->handle       = newFd;
    newSock->state        = DE_SOCKETSTATE_CONNECTED;
    newSock->openChannels = DE_SOCKETCHANNEL_BOTH;

    if (clientAddress)
        deBsdAddressToSocketAddress(clientAddress, bsdAddr, (int)bsdAddrLen);

    return newSock;
}

bool deSocket_connect(deSocket *sock, const deSocketAddress *address)
{
    uint8_t bsdAddrBuf[sizeof(struct sockaddr_in6)];
    struct sockaddr *bsdAddr = (struct sockaddr *)&bsdAddrBuf[0];
    NativeSocklen bsdAddrLen;

    /* Resolve address. */
    if (!deSocketAddressToBsdAddress(address, sizeof(bsdAddrBuf), bsdAddr, &bsdAddrLen))
        return false;

    /* Create socket. */
    sock->handle = socket(bsdAddr->sa_family, deSocketTypeToBsdType(address->type),
                          deSocketProtocolToBsdProtocol(address->protocol));
    if (!deSocketHandleIsValid(sock->handle))
        return false;

    /* Connect. */
    if (connect(sock->handle, bsdAddr, bsdAddrLen) != 0)
    {
#if defined(DE_USE_WINSOCK)
        closesocket(sock->handle);
#else
        close(sock->handle);
#endif
        sock->handle = DE_INVALID_SOCKET_HANDLE;
        return false;
    }

    sock->state        = DE_SOCKETSTATE_CONNECTED;
    sock->openChannels = DE_SOCKETCHANNEL_BOTH;

    return true;
}

bool deSocket_shutdown(deSocket *sock, uint32_t channels)
{
    uint32_t closedChannels = 0;

    deMutex_lock(sock->stateLock);

    if (sock->state == DE_SOCKETSTATE_DISCONNECTED || sock->state == DE_SOCKETSTATE_CLOSED)
    {
        deMutex_unlock(sock->stateLock);
        return false;
    }

    DE_ASSERT(channels != 0 && (channels & ~(uint32_t)DE_SOCKETCHANNEL_BOTH) == 0);

    /* Don't attempt to close already closed channels on partially open socket. */
    channels &= sock->openChannels;

    if (channels == 0)
    {
        deMutex_unlock(sock->stateLock);
        return false;
    }

#if defined(DE_USE_WINSOCK)
    {
        int how = 0;

        if ((channels & DE_SOCKETCHANNEL_BOTH) == DE_SOCKETCHANNEL_BOTH)
            how = SD_BOTH;
        else if (channels & DE_SOCKETCHANNEL_SEND)
            how = SD_SEND;
        else if (channels & DE_SOCKETCHANNEL_RECEIVE)
            how = SD_RECEIVE;

        if (shutdown(sock->handle, how) == 0)
            closedChannels = channels;
        else
        {
            int err = WSAGetLastError();

            /* \note Due to asynchronous behavior certain errors are perfectly ok. */
            if (err == WSAECONNABORTED || err == WSAECONNRESET || err == WSAENOTCONN)
                closedChannels = DE_SOCKETCHANNEL_BOTH;
            else
            {
                deMutex_unlock(sock->stateLock);
                return false;
            }
        }
    }
#else
    {
        int how = 0;

        if ((channels & DE_SOCKETCHANNEL_BOTH) == DE_SOCKETCHANNEL_BOTH)
            how = SHUT_RDWR;
        else if (channels & DE_SOCKETCHANNEL_SEND)
            how = SHUT_WR;
        else if (channels & DE_SOCKETCHANNEL_RECEIVE)
            how = SHUT_RD;

        if (shutdown(sock->handle, how) == 0)
            closedChannels = channels;
        else
        {
            if (errno == ENOTCONN)
                closedChannels = DE_SOCKETCHANNEL_BOTH;
            else
            {
                deMutex_unlock(sock->stateLock);
                return false;
            }
        }
    }
#endif

    sock->openChannels &= ~closedChannels;
    if (sock->openChannels == 0)
        sock->state = DE_SOCKETSTATE_DISCONNECTED;

    deMutex_unlock(sock->stateLock);
    return true;
}

bool deSocket_close(deSocket *sock)
{
    deMutex_lock(sock->stateLock);

    if (sock->state == DE_SOCKETSTATE_CLOSED)
    {
        deMutex_unlock(sock->stateLock);
        return false;
    }

#if !defined(DE_USE_WINSOCK)
    if (sock->state == DE_SOCKETSTATE_LISTENING)
    {
        /* There can be a thread blockin in accept(). Release it by calling shutdown. */
        shutdown(sock->handle, SHUT_RDWR);
    }
#endif

#if defined(DE_USE_WINSOCK)
    if (closesocket(sock->handle) != 0)
        return false;
#else
    if (close(sock->handle) != 0)
        return false;
#endif
    sock->state        = DE_SOCKETSTATE_CLOSED;
    sock->handle       = DE_INVALID_SOCKET_HANDLE;
    sock->openChannels = 0;

    deMutex_unlock(sock->stateLock);
    return true;
}

static deSocketResult mapSendRecvResult(int numBytes)
{
    if (numBytes > 0)
        return DE_SOCKETRESULT_SUCCESS;
    else if (numBytes == 0)
        return DE_SOCKETRESULT_CONNECTION_CLOSED;
    else
    {
        /* Other errors. */
#if defined(DE_USE_WINSOCK)
        int error = WSAGetLastError();
        switch (error)
        {
        case WSAEWOULDBLOCK:
            return DE_SOCKETRESULT_WOULD_BLOCK;
        case WSAENETDOWN:
        case WSAENETRESET:
        case WSAECONNABORTED:
        case WSAECONNRESET:
            return DE_SOCKETRESULT_CONNECTION_TERMINATED;
        default:
            return DE_SOCKETRESULT_ERROR;
        }
#else
        switch (errno)
        {
        case EAGAIN:
            return DE_SOCKETRESULT_WOULD_BLOCK;
        case ECONNABORTED:
        case ECONNRESET:
            return DE_SOCKETRESULT_CONNECTION_TERMINATED;
        default:
            return DE_SOCKETRESULT_ERROR;
        }
#endif
    }
}

void deSocket_setChannelsClosed(deSocket *sock, uint32_t channels)
{
    deMutex_lock(sock->stateLock);

    sock->openChannels &= ~channels;
    if (sock->openChannels == 0)
        sock->state = DE_SOCKETSTATE_DISCONNECTED;

    deMutex_unlock(sock->stateLock);
}

deSocketResult deSocket_send(deSocket *sock, const void *buf, size_t bufSize, size_t *numSentPtr)
{
    int numSent           = (int)send(sock->handle, (const char *)buf, (NativeSize)bufSize, 0);
    deSocketResult result = mapSendRecvResult(numSent);

    if (numSentPtr)
        *numSentPtr = (numSent > 0) ? ((size_t)numSent) : (0);

    /* Update state. */
    if (result == DE_SOCKETRESULT_CONNECTION_CLOSED)
        deSocket_setChannelsClosed(sock, DE_SOCKETCHANNEL_SEND);
    else if (result == DE_SOCKETRESULT_CONNECTION_TERMINATED)
        deSocket_setChannelsClosed(sock, DE_SOCKETCHANNEL_BOTH);

    return result;
}

deSocketResult deSocket_receive(deSocket *sock, void *buf, size_t bufSize, size_t *numReceivedPtr)
{
    int numRecv           = (int)recv(sock->handle, (char *)buf, (NativeSize)bufSize, 0);
    deSocketResult result = mapSendRecvResult(numRecv);

    if (numReceivedPtr)
        *numReceivedPtr = (numRecv > 0) ? ((size_t)numRecv) : (0);

    /* Update state. */
    if (result == DE_SOCKETRESULT_CONNECTION_CLOSED)
        deSocket_setChannelsClosed(sock, DE_SOCKETCHANNEL_RECEIVE);
    else if (result == DE_SOCKETRESULT_CONNECTION_TERMINATED)
        deSocket_setChannelsClosed(sock, DE_SOCKETCHANNEL_BOTH);

    return result;
}

#endif
