/*
* Copyright (C) 2019 Apple Inc. All rights reserved.
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

#if ENABLE(GPU_PROCESS)

namespace WebKit {

struct GPUProcessConnectionInfo {
    IPC::Attachment connection;
#if HAVE(AUDIT_TOKEN)
    std::optional<audit_token_t> auditToken;
#endif

    IPC::Connection::Identifier identifier() const
    {
#if USE(UNIX_DOMAIN_SOCKETS)
        return IPC::Connection::Identifier(connection.fileDescriptor());
#elif OS(DARWIN)
        return IPC::Connection::Identifier(connection.port());
#elif OS(WINDOWS)
        return IPC::Connection::Identifier(connection.handle());
#else
        ASSERT_NOT_REACHED();
        return IPC::Connection::Identifier();
#endif
    }

    IPC::Connection::Identifier releaseIdentifier()
    {
#if USE(UNIX_DOMAIN_SOCKETS)
        auto returnValue = IPC::Connection::Identifier(connection.releaseFileDescriptor());
#else
        auto returnValue = identifier();
#endif
        connection = { };
        return returnValue;
    }

    void encode(IPC::Encoder& encoder) const
    {
        encoder << connection;
#if HAVE(AUDIT_TOKEN)
        encoder << auditToken;
#endif
    }
    
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder& decoder, GPUProcessConnectionInfo& info)
    {
        if (!decoder.decode(info.connection))
            return false;
#if HAVE(AUDIT_TOKEN)
        if (!decoder.decode(info.auditToken))
            return false;
#endif
        return true;
    }
};

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
