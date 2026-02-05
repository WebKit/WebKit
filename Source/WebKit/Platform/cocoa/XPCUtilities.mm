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

#if USE(EXIT_XPC_MESSAGE_WORKAROUND)
#include "Logging.h"
#include <wtf/WTFProcess.h>
#include <wtf/darwin/XPCObjectPtr.h>
#include <wtf/text/ASCIILiteral.h>
#endif

namespace WebKit {

#if USE(EXIT_XPC_MESSAGE_WORKAROUND)
static constexpr auto messageNameKey = "message-name"_s;
static constexpr auto exitProcessMessage = "exit"_s;
#endif

#if !USE(EXTENSIONKIT_PROCESS_TERMINATION)
void terminateWithReason(xpc_connection_t connection, ReasonCode, const char*)
{
    // This could use ReasonSPI.h, but currently does not as the SPI is blocked by the sandbox.
    // See https://bugs.webkit.org/show_bug.cgi?id=224499 rdar://76396241
    if (!connection)
        return;

#if USE(EXIT_XPC_MESSAGE_WORKAROUND)
    // Give the process a chance to exit cleanly by sending a XPC message to request termination, then try xpc_connection_kill.
    // FIXME: This is a false positive. <rdar://164843889>
    SUPPRESS_RETAINPTR_CTOR_ADOPT auto exitMessage = adoptOSObject(xpc_dictionary_create(nullptr, nullptr, 0));
    xpc_dictionary_set_string(exitMessage.get(), messageNameKey, exitProcessMessage.characters());
    xpc_connection_send_message(connection, exitMessage.get());
#endif

ALLOW_DEPRECATED_DECLARATIONS_BEGIN
    xpc_connection_kill(connection, SIGKILL);
ALLOW_DEPRECATED_DECLARATIONS_END
}
#endif // !USE(EXTENSIONKIT_PROCESS_TERMINATION)

#if USE(EXIT_XPC_MESSAGE_WORKAROUND)
void handleXPCExitMessage(xpc_object_t event)
{
    if (xpc_get_type(event) == XPC_TYPE_DICTIONARY) {
        String messageName = xpcDictionaryGetString(event, messageNameKey);
        if (messageName == exitProcessMessage) {
            RELEASE_LOG_ERROR(IPC, "Received exit message, exiting now.");
            terminateProcess(EXIT_FAILURE);
        }
    }
}
#endif // USE(EXIT_XPC_MESSAGE_WORKAROUND)

}
