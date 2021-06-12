/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
#include "ProxyServer.h"

#include <wtf/text/StringBuilder.h>

namespace WebCore {

static void appendProxyServerString(StringBuilder& builder, const ProxyServer& proxyServer)
{
    ASSERT(!proxyServer.hostName().isNull());
    ASSERT(proxyServer.port() != -1);

    switch (proxyServer.type()) {
    case ProxyServer::Direct:
        builder.append("DIRECT");
        break;
    case ProxyServer::HTTP:
    case ProxyServer::HTTPS:
        builder.append("PROXY ", proxyServer.hostName(), ':', proxyServer.port());
        break;
    case ProxyServer::SOCKS:
        builder.append("SOCKS ", proxyServer.hostName(), ':', proxyServer.port());
        break;
    }
}

String toString(const Vector<ProxyServer>& proxyServers)
{
    if (proxyServers.isEmpty())
        return "DIRECT";

    StringBuilder stringBuilder;
    for (size_t i = 0; i < proxyServers.size(); ++i) {
        if (i)
            stringBuilder.append("; ");
        appendProxyServerString(stringBuilder, proxyServers[i]);
    }
    return stringBuilder.toString();
}

} // namespace WebCore
