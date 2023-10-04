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

#include "config.h"
#include "StorageUtilities.h"

#include "ClientOrigin.h"
#include "WebCorePersistentCoders.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/FileSystem.h>
#include <wtf/Scope.h>
#include <wtf/persistence/PersistentCoders.h>
#include <wtf/text/Base64.h>

#if ASSERT_ENABLED
#include <wtf/RunLoop.h>
#endif

namespace WebCore {
namespace StorageUtilities {

std::optional<ClientOrigin> readOriginFromFile(const String& filePath)
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
    std::optional<ClientOrigin> origin;
    decoder >> origin;
    return origin;
}

bool writeOriginToFile(const String& filePath, const ClientOrigin& origin)
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

String encodeSecurityOriginForFileName(FileSystem::Salt salt, const SecurityOriginData& origin)
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    auto originString = origin.toString().utf8();
    crypto->addBytes(originString.data(), originString.length());
    crypto->addBytes(salt.data(), salt.size());
    auto hash = crypto->computeHash();
    return base64URLEncodeToString(hash.data(), hash.size());
}

} // namespace StorageUtilities
} // namespace WebCore
