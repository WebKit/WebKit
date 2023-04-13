/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "SWScriptStorage.h"

#if ENABLE(SERVICE_WORKER)
#include "Logging.h"
#include "ScriptBuffer.h"
#include "ServiceWorkerRegistrationKey.h"
#include <pal/crypto/CryptoDigest.h>
#include <wtf/MainThread.h>
#include <wtf/PageBlock.h>
#include <wtf/text/Base64.h>

namespace WebCore {

static bool shouldUseFileMapping(uint64_t fileSize)
{
    return fileSize >= pageSize();
}

SWScriptStorage::SWScriptStorage(const String& directory)
    : m_directory(directory)
    , m_salt(valueOrDefault(FileSystem::readOrMakeSalt(saltPath())))
{
    ASSERT(!isMainThread());
}

String SWScriptStorage::sha2Hash(const String& input) const
{
    auto crypto = PAL::CryptoDigest::create(PAL::CryptoDigest::Algorithm::SHA_256);
    crypto->addBytes(m_salt.data(), m_salt.size());
    auto inputUTF8 = input.utf8();
    crypto->addBytes(inputUTF8.data(), inputUTF8.length());
    auto hash = crypto->computeHash();
    return base64URLEncodeToString(hash.data(), hash.size());
}

String SWScriptStorage::sha2Hash(const URL& input) const
{
    return sha2Hash(input.string());
}

String SWScriptStorage::saltPath() const
{
    return FileSystem::pathByAppendingComponent(m_directory, "salt"_s);
}

String SWScriptStorage::registrationDirectory(const ServiceWorkerRegistrationKey& registrationKey) const
{
    return FileSystem::pathByAppendingComponents(m_directory, { sha2Hash(registrationKey.topOrigin().toString()), sha2Hash(registrationKey.scope()) });
}

String SWScriptStorage::scriptPath(const ServiceWorkerRegistrationKey& registrationKey, const URL& scriptURL) const
{
    return FileSystem::pathByAppendingComponent(registrationDirectory(registrationKey), sha2Hash(scriptURL));
}

ScriptBuffer SWScriptStorage::store(const ServiceWorkerRegistrationKey& registrationKey, const URL& scriptURL, const ScriptBuffer& script)
{
    ASSERT(!isMainThread());

    auto scriptPath = this->scriptPath(registrationKey, scriptURL);
    FileSystem::makeAllDirectories(FileSystem::parentPath(scriptPath));

    auto iterateOverBufferAndWriteData = [&](const Function<bool(Span<const uint8_t>)>& writeData) {
        script.buffer()->forEachSegment([&](Span<const uint8_t> span) {
            writeData(span);
        });
    };

    // Make sure we delete the file before writing as there may be code using a mmap'd version of this file.
    FileSystem::deleteFile(scriptPath);

    if (!shouldUseFileMapping(script.buffer()->size())) {
        auto handle = FileSystem::openFile(scriptPath, FileSystem::FileOpenMode::Truncate);
        if (!FileSystem::isHandleValid(handle)) {
            RELEASE_LOG_ERROR(ServiceWorker, "SWScriptStorage::store: Failure to store %s, FileSystem::openFile() failed", scriptPath.utf8().data());
            return { };
        }
        iterateOverBufferAndWriteData([&](Span<const uint8_t> span) {
            FileSystem::writeToFile(handle, span.data(), span.size());
            return true;
        });
        FileSystem::closeFile(handle);
        return script;
    }

    auto mappedFile = FileSystem::mapToFile(scriptPath, script.buffer()->size(), WTFMove(iterateOverBufferAndWriteData));
    if (!mappedFile) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWScriptStorage::store: Failure to store %s, FileSystem::mapToFile() failed", scriptPath.utf8().data());
        return { };
    }
    return ScriptBuffer { SharedBuffer::create(WTFMove(mappedFile)) };
}

ScriptBuffer SWScriptStorage::retrieve(const ServiceWorkerRegistrationKey& registrationKey, const URL& scriptURL)
{
    ASSERT(!isMainThread());

    auto scriptPath = this->scriptPath(registrationKey, scriptURL);
    auto fileSize = FileSystem::fileSize(scriptPath);
    if (!fileSize) {
        RELEASE_LOG_ERROR(ServiceWorker, "SWScriptStorage::retrieve: Failure to retrieve %s, FileSystem::fileSize() failed", scriptPath.utf8().data());
        return { };
    }

    // FIXME: Do we need to disable file mapping in more cases to avoid having too many file descriptors open?
    RefPtr<FragmentedSharedBuffer> buffer = SharedBuffer::createWithContentsOfFile(scriptPath, FileSystem::MappedFileMode::Private, shouldUseFileMapping(*fileSize) ? SharedBuffer::MayUseFileMapping::Yes : SharedBuffer::MayUseFileMapping::No);
    return buffer;
}

void SWScriptStorage::clear(const ServiceWorkerRegistrationKey& registrationKey)
{
    auto registrationDirectory = this->registrationDirectory(registrationKey);
    bool result = FileSystem::deleteNonEmptyDirectory(registrationDirectory);
    RELEASE_LOG_ERROR_IF(!result, ServiceWorker, "SWScriptStorage::clear: Failure to clear scripts for registration %s", registrationKey.toDatabaseKey().utf8().data());
}

} // namespace WebCore

#endif // ENABLE(SERVICE_WORKER)
