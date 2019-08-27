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

#pragma once

#if ENABLE(REMOTE_INSPECTOR)

#include <array>
#include <wtf/Optional.h>
#include <wtf/Vector.h>

#if OS(WINDOWS)
#include <winsock2.h>
#else
#include <poll.h>
#endif

namespace Inspector {

using ConnectionID = uint32_t;

#if OS(WINDOWS)

using PlatformSocketType = SOCKET;
using PollingDescriptor = WSAPOLLFD;
constexpr PlatformSocketType INVALID_SOCKET_VALUE = INVALID_SOCKET;

#else

using PlatformSocketType = int;
using PollingDescriptor = struct pollfd;
constexpr PlatformSocketType INVALID_SOCKET_VALUE = -1;

#endif

namespace Socket {

enum class Domain {
    Local,
    Network,
};

void init();

Optional<PlatformSocketType> connect(const char* serverAddress, uint16_t serverPort);
Optional<PlatformSocketType> listen(const char* address, uint16_t port);
Optional<PlatformSocketType> accept(PlatformSocketType);
Optional<std::array<PlatformSocketType, 2>> createPair();

void setup(PlatformSocketType);
bool isValid(PlatformSocketType);
bool isListening(PlatformSocketType);
uint16_t getPort(PlatformSocketType);

Optional<size_t> read(PlatformSocketType, void* buffer, int bufferSize);
Optional<size_t> write(PlatformSocketType, const void* data, int size);

void close(PlatformSocketType&);

PollingDescriptor preparePolling(PlatformSocketType);
bool poll(Vector<PollingDescriptor>&, int timeout);
bool isReadable(const PollingDescriptor&);
bool isWritable(const PollingDescriptor&);
void markWaitingWritable(PollingDescriptor&);
void clearWaitingWritable(PollingDescriptor&);

constexpr size_t BufferSize = 65536;

} // namespace Socket

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
