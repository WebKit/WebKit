/*
 * Copyright (C) 2018 Igalia S.L. All rights reserved.
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
#include "WebsiteSessionHashSaltStorage.h"

#include "Logging.h"
#include "PersistencyUtils.h"

#include <WebCore/SharedBuffer.h>
#include <wtf/CryptographicallyRandomNumber.h>
#include <wtf/FileSystem.h>
#include <wtf/HexNumber.h>
#include <wtf/RunLoop.h>
#include <wtf/text/StringBuilder.h>
#include <wtf/text/StringHash.h>

namespace WebKit {
using namespace WebCore;

static constexpr unsigned websiteSessionHashSaltStorageVersion { 1 };
static constexpr unsigned hashSaltSize { 48 };
static constexpr unsigned randomDataSize { hashSaltSize / 16 };

Ref<WebsiteSessionHashSaltStorage> WebsiteSessionHashSaltStorage::create(const String& websiteSessionHashSaltStorageDirectory)
{
    auto websiteSessionHashSaltStorage = adoptRef(*new WebsiteSessionHashSaltStorage(websiteSessionHashSaltStorageDirectory));
    return websiteSessionHashSaltStorage;
}

void WebsiteSessionHashSaltStorage::completePendingHandler(CompletionHandler<void(HashSet<SecurityOriginData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    HashSet<SecurityOriginData> origins;

    for (auto& hashSaltForOrigin : m_websiteSessionHashSaltForOrigins) {
        origins.add(hashSaltForOrigin.value->documentOrigin);
        origins.add(hashSaltForOrigin.value->parentOrigin);
    }

    RunLoop::main().dispatch([origins = WTFMove(origins), completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(WTFMove(origins));
    });
}

WebsiteSessionHashSaltStorage::WebsiteSessionHashSaltStorage(const String& websiteSessionHashSaltStorageDirectory)
    : m_queue(WorkQueue::create("com.apple.WebKit.WebsiteSessionHashSaltStorage"))
    , m_websiteSessionHashSaltStorageDirectory(!websiteSessionHashSaltStorageDirectory.isEmpty() ? FileSystem::pathByAppendingComponent(websiteSessionHashSaltStorageDirectory, String::number(websiteSessionHashSaltStorageVersion)) : String())
{
    if (m_websiteSessionHashSaltStorageDirectory.isEmpty()) {
        m_isLoaded = true;
        return;
    }

    loadStorageFromDisk([this, protectedThis = Ref { *this }] (auto&& websiteSessionHashSaltForOrigins) {
        ASSERT(RunLoop::isMain());
        m_websiteSessionHashSaltForOrigins = WTFMove(websiteSessionHashSaltForOrigins);
        m_isLoaded = true;

        auto pendingCompletionHandlers = WTFMove(m_pendingCompletionHandlers);
        for (auto& completionHandler : pendingCompletionHandlers)
            completionHandler();
    });
}

WebsiteSessionHashSaltStorage::~WebsiteSessionHashSaltStorage()
{
    auto pendingCompletionHandlers = WTFMove(m_pendingCompletionHandlers);
    for (auto& completionHandler : pendingCompletionHandlers)
        completionHandler();
}

static std::optional<SecurityOriginData> getSecurityOriginData(ASCIILiteral name, KeyedDecoder* decoder)
{
    String origin;

    if (!decoder->decodeString(name, origin))
        return std::nullopt;

    auto securityOriginData = SecurityOriginData::fromDatabaseIdentifier(origin);
    if (!securityOriginData)
        return std::nullopt;

    return securityOriginData;
}

void WebsiteSessionHashSaltStorage::loadStorageFromDisk(CompletionHandler<void(HashMap<String, std::unique_ptr<HashSaltForOrigin>>&&)>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        ASSERT(!RunLoop::isMain());

        FileSystem::makeAllDirectories(m_websiteSessionHashSaltStorageDirectory);

        HashMap<String, std::unique_ptr<HashSaltForOrigin>> websiteSessionHashSaltForOrigins;
        for (auto& origin : FileSystem::listDirectory(m_websiteSessionHashSaltStorageDirectory)) {
            auto originPath = FileSystem::pathByAppendingComponent(m_websiteSessionHashSaltStorageDirectory, origin);
            auto websiteSessionHashSalt = URL::fileURLWithFileSystemPath(originPath).lastPathComponent().toString();

            if (hashSaltSize != websiteSessionHashSalt.length()) {
                RELEASE_LOG_ERROR(DiskPersistency, "WebsiteSessionHashSaltStorage: The length of the hash salt (%d) is different to the length of the hash salts defined in WebKit (%d)", websiteSessionHashSalt.length(), hashSaltSize);
                continue;
            }

            if (!FileSystem::fileSize(originPath)) {
                RELEASE_LOG_ERROR(DiskPersistency, "WebsiteSessionHashSaltStorage: Impossible to get the file size of: '%s'", originPath.utf8().data());
                continue;
            }

            auto decoder = createForFile(originPath);

            if (!decoder) {
                RELEASE_LOG_ERROR(DiskPersistency, "WebsiteSessionHashSaltStorage: Impossible to access the file to restore the hash salt: '%s'", originPath.utf8().data());
                continue;
            }

            auto hashSaltForOrigin = getDataFromDecoder(decoder.get(), WTFMove(websiteSessionHashSalt));
            if (!hashSaltForOrigin)
                continue;

            auto origins = makeString(hashSaltForOrigin->documentOrigin.toString(), hashSaltForOrigin->parentOrigin.toString());
            auto websiteSessionHashSaltForOrigin = websiteSessionHashSaltForOrigins.ensure(origins, [hashSaltForOrigin = WTFMove(hashSaltForOrigin)] () mutable {
                return WTFMove(hashSaltForOrigin);
            });

            if (!websiteSessionHashSaltForOrigin.isNewEntry)
                RELEASE_LOG_ERROR(DiskPersistency, "WebsiteSessionHashSaltStorage: There are two files with different hash salts for the same origin: '%s'", originPath.utf8().data());
        }

        RunLoop::main().dispatch([websiteSessionHashSaltForOrigins = WTFMove(websiteSessionHashSaltForOrigins), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(websiteSessionHashSaltForOrigins));
        });
    });
}

std::unique_ptr<WebsiteSessionHashSaltStorage::HashSaltForOrigin> WebsiteSessionHashSaltStorage::getDataFromDecoder(KeyedDecoder* decoder, String&& websiteSessionHashSalt) const
{
    auto securityOriginData = getSecurityOriginData("origin"_s, decoder);
    if (!securityOriginData) {
        RELEASE_LOG_ERROR(DiskPersistency, "WebsiteSessionHashSaltStorage: The security origin data in the file is not correct: '%s'", websiteSessionHashSalt.utf8().data());
        return nullptr;
    }

    auto parentSecurityOriginData = getSecurityOriginData("parentOrigin"_s, decoder);
    if (!parentSecurityOriginData) {
        RELEASE_LOG_ERROR(DiskPersistency, "WebsiteSessionHashSaltStorage: The parent security origin data in the file is not correct: '%s'", websiteSessionHashSalt.utf8().data());
        return nullptr;
    }

    double lastTimeUsed;
    if (!decoder->decodeDouble("lastTimeUsed"_s, lastTimeUsed)) {
        RELEASE_LOG_ERROR(DiskPersistency, "WebsiteSessionHashSaltStorage: The last time used was not correctly restored for: '%s'", websiteSessionHashSalt.utf8().data());
        return nullptr;
    }

    auto hashSaltForOrigin = makeUnique<HashSaltForOrigin>(WTFMove(securityOriginData.value()), WTFMove(parentSecurityOriginData.value()), WTFMove(websiteSessionHashSalt));

    hashSaltForOrigin->lastTimeUsed = WallTime::fromRawSeconds(lastTimeUsed);

    return hashSaltForOrigin;
}

std::unique_ptr<KeyedEncoder> WebsiteSessionHashSaltStorage::createEncoderFromData(const HashSaltForOrigin& hashSaltForOrigin) const
{
    auto encoder = KeyedEncoder::encoder();
    encoder->encodeString("origin"_s, hashSaltForOrigin.documentOrigin.databaseIdentifier());
    encoder->encodeString("parentOrigin"_s, hashSaltForOrigin.parentOrigin.databaseIdentifier());
    encoder->encodeDouble("lastTimeUsed"_s, hashSaltForOrigin.lastTimeUsed.secondsSinceEpoch().value());
    return encoder;
}

void WebsiteSessionHashSaltStorage::storeHashSaltToDisk(const HashSaltForOrigin& hashSaltForOrigin)
{
    if (m_websiteSessionHashSaltStorageDirectory.isEmpty())
        return;

    m_queue->dispatch([this, protectedThis = Ref { *this }, hashSaltForOrigin = hashSaltForOrigin.isolatedCopy()]() mutable {
        auto encoder = createEncoderFromData(hashSaltForOrigin);
        writeToDisk(WTFMove(encoder), FileSystem::pathByAppendingComponent(m_websiteSessionHashSaltStorageDirectory, hashSaltForOrigin.websiteSessionHashSalt));
    });
}

void WebsiteSessionHashSaltStorage::completeWebsiteSessionHashSaltForOriginCall(SecurityOriginData&& documentOrigin, SecurityOriginData&& parentOrigin, CompletionHandler<void(String&&)>&& completionHandler)
{
    auto origins = makeString(documentOrigin.toString(), parentOrigin.toString());
    auto& websiteSessionHashSalt = m_websiteSessionHashSaltForOrigins.ensure(origins, [documentOrigin = WTFMove(documentOrigin), parentOrigin = WTFMove(parentOrigin)] () mutable {
        uint64_t randomData[randomDataSize];
        cryptographicallyRandomValues(randomData, sizeof(randomData));

        StringBuilder builder;
        builder.reserveCapacity(hashSaltSize);
        for (unsigned i = 0; i < randomDataSize; i++)
            builder.append(hex(randomData[i]));

        String websiteSessionHashSalt = builder.toString();

        auto newHashSaltForOrigin = makeUnique<HashSaltForOrigin>(WTFMove(documentOrigin), WTFMove(parentOrigin), WTFMove(websiteSessionHashSalt));

        return newHashSaltForOrigin;
    }).iterator->value;

    websiteSessionHashSalt->lastTimeUsed = WallTime::now();

    storeHashSaltToDisk(*websiteSessionHashSalt.get());

    completionHandler(String(websiteSessionHashSalt->websiteSessionHashSalt));
}

void WebsiteSessionHashSaltStorage::websiteSessionHashSaltForOrigin(const SecurityOrigin& documentOrigin, const SecurityOrigin& parentOrigin, CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (!m_isLoaded) {
        m_pendingCompletionHandlers.append([this, documentOrigin = documentOrigin.data().isolatedCopy(), parentOrigin = parentOrigin.data().isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completeWebsiteSessionHashSaltForOriginCall(WTFMove(documentOrigin), WTFMove(parentOrigin), WTFMove(completionHandler));
        });
        return;
    }

    completeWebsiteSessionHashSaltForOriginCall(SecurityOriginData(documentOrigin.data()), SecurityOriginData(parentOrigin.data()), WTFMove(completionHandler));
}

void WebsiteSessionHashSaltStorage::getWebsiteSessionHashSaltOrigins(CompletionHandler<void(HashSet<SecurityOriginData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (!m_isLoaded) {
        m_pendingCompletionHandlers.append([this, completionHandler = WTFMove(completionHandler)]() mutable {
            completePendingHandler(WTFMove(completionHandler));
        });
        return;
    }

    completePendingHandler(WTFMove(completionHandler));
}

void WebsiteSessionHashSaltStorage::deleteHashSaltFromDisk(const HashSaltForOrigin& hashSaltForOrigin)
{
    m_queue->dispatch([this, protectedThis = Ref { *this }, websiteSessionHashSalt = hashSaltForOrigin.websiteSessionHashSalt.isolatedCopy()]() mutable {
        ASSERT(!RunLoop::isMain());

        String fileFullPath = FileSystem::pathByAppendingComponent(m_websiteSessionHashSaltStorageDirectory, websiteSessionHashSalt);
        FileSystem::deleteFile(fileFullPath);
    });
}

void WebsiteSessionHashSaltStorage::deleteWebsiteSessionHashSaltForOrigins(const Vector<SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_websiteSessionHashSaltForOrigins.removeIf([this, &origins](auto& keyAndValue) {
        bool needsRemoval = origins.contains(keyAndValue.value->documentOrigin) || origins.contains(keyAndValue.value->parentOrigin);
        if (m_websiteSessionHashSaltStorageDirectory.isEmpty())
            return needsRemoval;
        if (needsRemoval)
            this->deleteHashSaltFromDisk(*keyAndValue.value.get());
        return needsRemoval;
    });

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

void WebsiteSessionHashSaltStorage::deleteWebsiteSessionHashSaltOriginsModifiedSince(WallTime time, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_websiteSessionHashSaltForOrigins.removeIf([this, time](auto& keyAndValue) {
        bool needsRemoval = keyAndValue.value->lastTimeUsed > time;
        if (m_websiteSessionHashSaltStorageDirectory.isEmpty())
            return needsRemoval;
        if (needsRemoval)
            this->deleteHashSaltFromDisk(*keyAndValue.value.get());
        return needsRemoval;
    });

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

} // namespace WebKit
