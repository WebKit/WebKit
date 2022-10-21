/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "UserMessage.h"

#include "ArgumentCodersGLib.h"
#include "DataReference.h"
#include "WebCoreArgumentCoders.h"
#include <gio/gunixfdlist.h>

namespace WebKit {

void UserMessage::encode(IPC::Encoder& encoder) const
{
    encoder << type;
    if (type == Type::Null)
        return;

    encoder << name;
    if (type == Type::Error) {
        encoder << errorCode;
        return;
    }

    encoder << parameters;

    Vector<UnixFileDescriptor> attachments;
    if (fileDescriptors) {
        int length = g_unix_fd_list_get_length(fileDescriptors.get());
        for (int i = 0; i < length; ++i)
            attachments.append(UnixFileDescriptor { g_unix_fd_list_get(fileDescriptors.get(), i, nullptr), UnixFileDescriptor::Adopt });
    }
    encoder << attachments;
}

std::optional<UserMessage> UserMessage::decode(IPC::Decoder& decoder)
{
    UserMessage result;
    if (!decoder.decode(result.type))
        return std::nullopt;

    if (result.type == Type::Null)
        return result;

    if (!decoder.decode(result.name))
        return std::nullopt;

    if (result.type == Type::Error) {
        std::optional<uint32_t> errorCode;
        decoder >> errorCode;
        if (!errorCode)
            return std::nullopt;

        result.errorCode = errorCode.value();
        return result;
    }

    std::optional<GRefPtr<GVariant>> parameters;
    decoder >> parameters;
    if (!parameters)
        return std::nullopt;
    result.parameters = WTFMove(*parameters);

    std::optional<Vector<UnixFileDescriptor>> attachments;
    decoder >> attachments;
    if (!attachments)
        return std::nullopt;
    if (!attachments->isEmpty()) {
        result.fileDescriptors = adoptGRef(g_unix_fd_list_new());
        for (auto& attachment : *attachments) {
            if (g_unix_fd_list_append(result.fileDescriptors.get(), attachment.release(), nullptr) == -1)
                return std::nullopt;
        }
    }

    return result;
}

}
