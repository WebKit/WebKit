/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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

#include "SharedFileHandle.h"
#include <WebCore/FileSystemSyncAccessHandleIdentifier.h>

namespace WebKit {

struct FileSystemSyncAccessHandleInfo {
    WebCore::FileSystemSyncAccessHandleIdentifier identifier;
    IPC::SharedFileHandle handle;
    uint64_t capacity { 0 };

    template<class Encoder> void encode(Encoder&) const;
    template<class Decoder> static std::optional<FileSystemSyncAccessHandleInfo> decode(Decoder&);
};

template<class Encoder> void FileSystemSyncAccessHandleInfo::encode(Encoder& encoder) const
{
    encoder << identifier;
    encoder << handle;
    encoder << capacity;
}

template<class Decoder> std::optional<FileSystemSyncAccessHandleInfo> FileSystemSyncAccessHandleInfo::decode(Decoder& decoder)
{
    WebCore::FileSystemSyncAccessHandleIdentifier identifier;
    if (!decoder.decode(identifier))
        return std::nullopt;

    IPC::SharedFileHandle handle;
    if (!decoder.decode(handle))
        return std::nullopt;

    uint64_t capacity;
    if (!decoder.decode(capacity))
        return std::nullopt;

    return FileSystemSyncAccessHandleInfo { identifier, WTFMove(handle), capacity };
}

} // namespace WebKit
