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
#include "RemoteInspectorSocketServer.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace Inspector {

RemoteInspectorSocketServer::RemoteInspectorSocketServer(RemoteInspectorConnectionClient* inspectorClient)
    : RemoteInspectorSocket(inspectorClient)
{
}

bool RemoteInspectorSocketServer::listenInet(uint16_t port)
{
    struct sockaddr_in address;
    memset(&address, 0, sizeof(address));

    int fdListen = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fdListen < 0) {
        LOG_ERROR("socket() failed, errno = %d", errno);
        return false;
    }

    const int enabled = 1;
    int error = setsockopt(fdListen, SOL_SOCKET, SO_REUSEADDR, &enabled, sizeof(enabled));
    if (error < 0) {
        LOG_ERROR("setsockopt() SO_REUSEADDR, errno = %d", errno);
        return false;
    }
    error = setsockopt(fdListen, SOL_SOCKET, SO_REUSEPORT, &enabled, sizeof(enabled));
    if (error < 0) {
        LOG_ERROR("setsockopt() SO_REUSEPORT, errno = %d", errno);
        return false;
    }

    // FIXME: Support AF_INET6 connections.
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(port);
    error = bind(fdListen, (struct sockaddr*)&address, sizeof(address));
    if (error < 0) {
        LOG_ERROR("bind() failed, errno = %d", errno);
        ::close(fdListen);
        return false;
    }

    error = listen(fdListen, 1);
    if (error < 0) {
        LOG_ERROR("listen() failed, errno = %d", errno);
        ::close(fdListen);
        return false;
    }

    if (createClient(fdListen))
        return true;

    return false;
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
