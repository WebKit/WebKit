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
#include "ArgumentCodersGLib.h"

#include <gio/gio.h>
#include <gio/gunixfdlist.h>
#include <wtf/Vector.h>

namespace IPC {

void ArgumentCoder<GTlsCertificateFlags>::encode(Encoder& encoder, GTlsCertificateFlags flags)
{
    encoder << static_cast<uint32_t>(flags);
}

std::optional<GTlsCertificateFlags> ArgumentCoder<GTlsCertificateFlags>::decode(Decoder& decoder)
{
    auto flags = decoder.decode<uint32_t>();
    if (!flags) [[unlikely]]
        return std::nullopt;
    return static_cast<GTlsCertificateFlags>(*flags);
}

void ArgumentCoder<GRefPtr<GUnixFDList>>::encode(Encoder& encoder, const GRefPtr<GUnixFDList>& fdList)
{
    if (!fdList) {
        encoder << false;
        return;
    }

    Vector<UnixFileDescriptor> attachments;
    unsigned length = std::max(0, g_unix_fd_list_get_length(fdList.get()));
    if (length) {
        attachments = Vector<UnixFileDescriptor>(length, [&](size_t i) {
            return UnixFileDescriptor { g_unix_fd_list_get(fdList.get(), i, nullptr), UnixFileDescriptor::Adopt };
        });
    }
    encoder << true << WTF::move(attachments);
}

std::optional<GRefPtr<GUnixFDList>> ArgumentCoder<GRefPtr<GUnixFDList>>::decode(Decoder& decoder)
{
    auto hasObject = decoder.decode<bool>();
    if (!hasObject) [[unlikely]]
        return std::nullopt;
    if (!*hasObject)
        return GRefPtr<GUnixFDList> { };

    auto attachments = decoder.decode<Vector<UnixFileDescriptor>>();
    if (!attachments) [[unlikely]]
        return std::nullopt;

    GRefPtr<GUnixFDList> fdList = adoptGRef(g_unix_fd_list_new());
    for (auto& attachment : *attachments) {
        int ret = g_unix_fd_list_append(fdList.get(), attachment.value(), nullptr);
        if (ret == -1) [[unlikely]]
            return std::nullopt;
    }
    return fdList;
}

} // namespace IPC
