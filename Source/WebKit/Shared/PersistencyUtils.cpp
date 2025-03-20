/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "PersistencyUtils.h"

#include "Logging.h"
#include <WebCore/SharedBuffer.h>
#include <wtf/FileSystem.h>
#include <wtf/RunLoop.h>

namespace WebKit {

using namespace WebCore;

std::unique_ptr<KeyedDecoder> createForFile(const String& path)
{
    ASSERT(!RunLoop::isMain());

    auto buffer = FileSystem::readEntireFile(path);
    if (!buffer)
        return nullptr;

    return KeyedDecoder::decoder(buffer->span());
}

void writeToDisk(std::unique_ptr<KeyedEncoder>&& encoder, String&& path)
{
    ASSERT(!RunLoop::isMain());

    auto rawData = encoder->finishEncoding();
    if (!rawData)
        return;

    auto handle = FileSystem::openFile(path, FileSystem::FileOpenMode::Truncate, FileSystem::FileAccessPermission::All, { FileSystem::FileLockMode::Exclusive });
    if (!handle)
        return;

    auto writtenBytes = handle.write(rawData->span());
    if (writtenBytes != rawData->size())
        RELEASE_LOG_ERROR(DiskPersistency, "Disk persistency: We only wrote %" PRIu64 " out of %zu bytes to disk", *writtenBytes, rawData->size());
}

} // namespace WebKit
