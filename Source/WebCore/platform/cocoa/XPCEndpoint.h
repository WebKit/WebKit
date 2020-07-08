/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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

#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>

namespace WebCore {

class XPCEndpoint {
public:
    WEBCORE_EXPORT XPCEndpoint();
    WEBCORE_EXPORT virtual ~XPCEndpoint() = default;

    WEBCORE_EXPORT void sendEndpointToConnection(xpc_connection_t);

    WEBCORE_EXPORT OSObjectPtr<xpc_endpoint_t> endpoint() const;

    static constexpr auto xpcMessageNameKey = "message-name";

private:
    virtual const char* xpcEndpointMessageNameKey() const = 0;
    virtual const char* xpcEndpointMessageName() const = 0;
    virtual const char* xpcEndpointNameKey() const = 0;
    virtual void handleEvent(xpc_connection_t, xpc_object_t) = 0;

    OSObjectPtr<xpc_connection_t> m_connection;
    OSObjectPtr<xpc_endpoint_t> m_endpoint;
};

}
