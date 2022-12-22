/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include <WebCore/ClientOrigin.h>
#include <WebCore/WebCorePersistentCoders.h>
#include <wtf/FileSystem.h>
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>

namespace WebKit {

static inline std::optional<WebCore::ClientOrigin> readOriginFromFile(const String& filePath)
{
    ASSERT(!RunLoop::isMain());

    if (filePath.isEmpty() || !FileSystem::fileExists(filePath))
        return std::nullopt;

    auto originFileHandle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Read);
    auto closeFile = makeScopeExit([&] {
        FileSystem::closeFile(originFileHandle);
    });

    if (!FileSystem::isHandleValid(originFileHandle))
        return std::nullopt;

    auto originContent = FileSystem::readEntireFile(originFileHandle);
    if (!originContent)
        return std::nullopt;

    WTF::Persistence::Decoder decoder({ originContent->data(), originContent->size() });
    std::optional<WebCore::ClientOrigin> origin;
    decoder >> origin;
    return origin;
}

static inline bool writeOriginToFile(const String& filePath, const WebCore::ClientOrigin& origin)
{
    if (filePath.isEmpty() || FileSystem::fileExists(filePath))
        return false;

    FileSystem::makeAllDirectories(FileSystem::parentPath(filePath));
    auto originFileHandle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::ReadWrite);
    auto closeFile = makeScopeExit([&] {
        FileSystem::closeFile(originFileHandle);
    });

    if (!FileSystem::isHandleValid(originFileHandle)) {
        LOG_ERROR("writeOriginToFile: Failed to open origin file '%s'", filePath.utf8().data());
        return false;
    }

    WTF::Persistence::Encoder encoder;
    encoder << origin;
    FileSystem::writeToFile(originFileHandle, encoder.buffer(), encoder.bufferSize());
    return true;
}

}
