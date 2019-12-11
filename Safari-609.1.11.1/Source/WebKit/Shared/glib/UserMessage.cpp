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
#include "Attachment.h"
#include "DataReference.h"
#include <gio/gunixfdlist.h>

namespace WebKit {

void UserMessage::encode(IPC::Encoder& encoder) const
{
    encoder.encodeEnum(type);
    if (type == Type::Null)
        return;

    encoder << name;
    if (type == Type::Error) {
        encoder << errorCode;
        return;
    }

    IPC::encode(encoder, parameters.get());

    Vector<IPC::Attachment> attachments;
    if (fileDescriptors) {
        int length = g_unix_fd_list_get_length(fileDescriptors.get());
        for (int i = 0; i < length; ++i)
            attachments.append(IPC::Attachment(g_unix_fd_list_get(fileDescriptors.get(), i, nullptr)));
    }
    encoder << attachments;
}

Optional<UserMessage> UserMessage::decode(IPC::Decoder& decoder)
{
    UserMessage result;
    if (!decoder.decodeEnum(result.type))
        return WTF::nullopt;

    if (result.type == Type::Null)
        return result;

    if (!decoder.decode(result.name))
        return WTF::nullopt;

    if (result.type == Type::Error) {
        Optional<uint32_t> errorCode;
        decoder >> errorCode;
        if (!errorCode)
            return WTF::nullopt;

        result.errorCode = errorCode.value();
        return result;
    }

    Optional<GRefPtr<GVariant>> parameters = IPC::decode(decoder);
    if (!parameters)
        return WTF::nullopt;
    result.parameters = WTFMove(*parameters);

    Optional<Vector<IPC::Attachment>> attachments;
    decoder >> attachments;
    if (!attachments)
        return WTF::nullopt;
    if (!attachments->isEmpty()) {
        result.fileDescriptors = adoptGRef(g_unix_fd_list_new());
        for (auto& attachment : *attachments) {
            if (g_unix_fd_list_append(result.fileDescriptors.get(), attachment.releaseFileDescriptor(), nullptr) == -1)
                return WTF::nullopt;
        }
    }

    return result;
}

}
