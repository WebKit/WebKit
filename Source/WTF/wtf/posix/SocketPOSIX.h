/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#pragma once

#include <sys/socket.h>
#include <wtf/Compiler.h>
#include <wtf/Function.h>
#include <wtf/TypeCasts.h>

namespace WTF {

inline bool isIPV4SocketAddress(const struct sockaddr& socket)
{
    return socket.sa_family == AF_INET;
}

inline const struct sockaddr_in* dynamicCastToIPV4SocketAddress(const struct sockaddr& socket)
{
    if (isIPV4SocketAddress(socket))
        SUPPRESS_MEMORY_UNSAFE_CAST return reinterpret_cast<const struct sockaddr_in*>(&socket);
    return nullptr;
}

inline const struct sockaddr_in& asIPV4SocketAddress(const struct sockaddr& socket)
{
    RELEASE_ASSERT(isIPV4SocketAddress(socket));
    SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const struct sockaddr_in*>(&socket);
}

inline bool initializeIPV4SocketAddress(sockaddr_storage& storage, NOESCAPE const Function<bool(sockaddr_in&)>& initialize)
{
    zeroBytes(storage);
    SUPPRESS_MEMORY_UNSAFE_CAST auto& address = *reinterpret_cast<struct sockaddr_in*>(&storage);
    if (!initialize(address))
        return false;
    storage.ss_family = AF_INET;
    return true;
}

inline bool isIPV6SocketAddress(const struct sockaddr& socket)
{
    return socket.sa_family == AF_INET6;
}

inline const struct sockaddr_in6* dynamicCastToIPV6SocketAddress(const struct sockaddr& socket)
{
    if (isIPV6SocketAddress(socket))
        SUPPRESS_MEMORY_UNSAFE_CAST return reinterpret_cast<const struct sockaddr_in6*>(&socket);
    return nullptr;
}

inline const struct sockaddr_in6& asIPV6SocketAddress(const struct sockaddr& socket)
{
    RELEASE_ASSERT(isIPV6SocketAddress(socket));
    SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const struct sockaddr_in6*>(&socket);
}

inline bool initializeIPV6SocketAddress(sockaddr_storage& storage, NOESCAPE const Function<bool(sockaddr_in6&)>& initialize)
{
    zeroBytes(storage);
    SUPPRESS_MEMORY_UNSAFE_CAST auto& address = *reinterpret_cast<struct sockaddr_in6*>(&storage);
    if (!initialize(address))
        return false;
    storage.ss_family = AF_INET6;
    return true;
}

inline struct sockaddr& asSocketAddress(sockaddr_storage& storage)
{
    SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<struct sockaddr*>(&storage);
}

} // namespace WTF

using WTF::asIPV4SocketAddress;
using WTF::asIPV6SocketAddress;
using WTF::asSocketAddress;
using WTF::dynamicCastToIPV4SocketAddress;
using WTF::dynamicCastToIPV6SocketAddress;
using WTF::initializeIPV4SocketAddress;
using WTF::initializeIPV6SocketAddress;
using WTF::isIPV4SocketAddress;
using WTF::isIPV6SocketAddress;
