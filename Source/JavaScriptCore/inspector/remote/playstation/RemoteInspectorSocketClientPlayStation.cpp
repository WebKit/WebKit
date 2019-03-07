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
#include "RemoteInspectorSocketClient.h"

#if ENABLE(REMOTE_INSPECTOR)

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace Inspector {

RemoteInspectorSocketClient::RemoteInspectorSocketClient(RemoteInspectorConnectionClient* inspectorClient)
    : RemoteInspectorSocket(inspectorClient)
{
}

Optional<ClientID> RemoteInspectorSocketClient::connectInet(const char* serverAddress, uint16_t serverPort)
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

    int error = connect(fd, (struct sockaddr*)&address, sizeof(address));
    if (error < 0) {
        LOG_ERROR("Failed to connect to %s:%u, errno = %d", serverAddress, serverPort, errno);
        ::close(fd);
        return WTF::nullopt;
    }

    return createClient(fd);
}

} // namespace Inspector

#endif // ENABLE(REMOTE_INSPECTOR)
