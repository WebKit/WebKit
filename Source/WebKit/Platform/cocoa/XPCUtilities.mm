/*
 * Copyright (C) 2022-2023 Apple Inc. All rights reserved.
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
#include "XPCUtilities.h"

#include <wtf/spi/darwin/ReasonSPI.h>
#include <wtf/spi/darwin/XPCSPI.h>

#if HAVE(LIBPROC)
#include <libproc.h>
#endif

namespace WebKit {

#if !USE(EXTENSIONKIT_PROCESS_TERMINATION)
void terminateWithReason(xpc_connection_t connection, WebKitReasonCode reasonCode, const char* reason, std::optional<IPC::MessageName> invalidMessageName)
{
    if (!connection)
        return;

#if ASAN_ENABLED
    // Unlike xpc_connection_kill(), this leaves a crash report that run-webkit-tests can match.
    // Not done unconditionally because the SPI is blocked for embedders with a sandbox (Bug 224499).
    if (reasonCode == WebKitReasonCode::MessageCheckKilled || reasonCode == WebKitReasonCode::WatchdogTimerFired) {
        // Encode the WebKitReasonCode in the low byte; for a message check, pack the failing
        // IPC message name above it so the crash report identifies which check failed.
        uint64_t code = std::to_underlying(reasonCode);
        if (invalidMessageName)
            code |= static_cast<uint64_t>(std::to_underlying(*invalidMessageName)) << 8;
        bool isSameProcess = true;
#if HAVE(LIBPROC)
        // terminate_with_reason() signals by pid, and the watchdog fires 30s after the connection
        // was cancelled, so first confirm the pid still names the connection's process.
        audit_token_t auditToken { };
        xpc_connection_get_audit_token(connection, &auditToken);
        std::array<char, PROC_PIDPATHINFO_MAXSIZE> path;
        isSameProcess = proc_pidpath_audittoken(&auditToken, path.data(), path.size()) > 0;
#endif
        if (isSameProcess && !terminate_with_reason(xpc_connection_get_pid(connection), OS_REASON_WEBKIT, code, reason, 0))
            return;
    }
#else
    UNUSED_PARAM(reasonCode);
    UNUSED_PARAM(reason);
    UNUSED_PARAM(invalidMessageName);
#endif

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    xpc_connection_kill(connection, SIGKILL);
ALLOW_DEPRECATED_DECLARATIONS_END
}
#endif // !USE(EXTENSIONKIT_PROCESS_TERMINATION)

}
