/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include <optional>
#include <wtf/Forward.h>
#include <wtf/OSObjectPtr.h>
#include <wtf/spi/darwin/XPCSPI.h>
#include <wtf/text/WTFString.h>

namespace WebPushD {

class ClientConnection {
    WTF_MAKE_FAST_ALLOCATED;
public:
    ClientConnection(xpc_connection_t);

    bool hasHostAppAuditToken() const { return !!m_hostAppAuditToken; }
    void setHostAppAuditTokenData(const Vector<uint8_t>&);

    const String& hostAppCodeSigningIdentifier();
    bool hostAppHasPushEntitlement();

    bool debugModeIsEnabled() const { return m_debugModeEnabled; }
    void setDebugModeIsEnabled(bool);

private:
    OSObjectPtr<xpc_connection_t> m_xpcConnection;

    std::optional<audit_token_t> m_hostAppAuditToken;
    std::optional<String> m_hostAppCodeSigningIdentifier;
    std::optional<bool> m_hostAppHasPushEntitlement;

    bool m_debugModeEnabled { false };
};

} // namespace WebPushD
