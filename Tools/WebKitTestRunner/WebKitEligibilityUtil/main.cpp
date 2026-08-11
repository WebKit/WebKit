/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
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

#include <xpc/xpc.h>

int main(int argc, const char * argv[])
{
    xpc_object_t message = xpc_dictionary_create(nullptr, nullptr, 0);
    xpc_connection_t connection = xpc_connection_create_mach_service("com.apple.eligibilityd", nullptr, 0);
    xpc_connection_set_event_handler(connection, ^(xpc_object_t) { });
    xpc_connection_activate(connection);
    xpc_object_t replyMessage = xpc_connection_send_message_with_reply_sync(connection, message);

    xpc_type_t replyType = xpc_get_type(replyMessage);
    if (!replyType || replyType == XPC_TYPE_ERROR) {
        const char* error = xpc_dictionary_get_string(replyMessage, _xpc_error_key_description);
        if (error)
            fprintf(stderr, "%s", error);
        return 1;
    }
    return 0;
}
