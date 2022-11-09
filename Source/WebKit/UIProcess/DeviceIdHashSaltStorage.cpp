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
#include "DeviceIdHashSaltStorage.h"

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

static constexpr unsigned deviceIdHashSaltStorageVersion { 1 };
static constexpr unsigned hashSaltSize { 48 };
static constexpr unsigned randomDataSize { hashSaltSize / 16 };

Ref<DeviceIdHashSaltStorage> DeviceIdHashSaltStorage::create(const String& deviceIdHashSaltStorageDirectory)
{
    auto deviceIdHashSaltStorage = adoptRef(*new DeviceIdHashSaltStorage(deviceIdHashSaltStorageDirectory));
    return deviceIdHashSaltStorage;
}

void DeviceIdHashSaltStorage::completePendingHandler(CompletionHandler<void(HashSet<SecurityOriginData>&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    HashSet<SecurityOriginData> origins;

    for (auto& hashSaltForOrigin : m_deviceIdHashSaltForOrigins) {
        origins.add(hashSaltForOrigin.value->documentOrigin);
        origins.add(hashSaltForOrigin.value->parentOrigin);
    }

    RunLoop::main().dispatch([origins = WTFMove(origins), completionHandler = WTFMove(completionHandler)]() mutable {
        completionHandler(WTFMove(origins));
    });
}

DeviceIdHashSaltStorage::DeviceIdHashSaltStorage(const String& deviceIdHashSaltStorageDirectory)
    : m_queue(WorkQueue::create("com.apple.WebKit.DeviceIdHashSaltStorage"))
    , m_deviceIdHashSaltStorageDirectory(!deviceIdHashSaltStorageDirectory.isEmpty() ? FileSystem::pathByAppendingComponent(deviceIdHashSaltStorageDirectory, String::number(deviceIdHashSaltStorageVersion)) : String())
{
    if (m_deviceIdHashSaltStorageDirectory.isEmpty()) {
        m_isLoaded = true;
        return;
    }

    loadStorageFromDisk([this, protectedThis = Ref { *this }] (auto&& deviceIdHashSaltForOrigins) {
        ASSERT(RunLoop::isMain());
        m_deviceIdHashSaltForOrigins = WTFMove(deviceIdHashSaltForOrigins);
        m_isLoaded = true;

        auto pendingCompletionHandlers = WTFMove(m_pendingCompletionHandlers);
        for (auto& completionHandler : pendingCompletionHandlers)
            completionHandler();
    });
}

DeviceIdHashSaltStorage::~DeviceIdHashSaltStorage()
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

void DeviceIdHashSaltStorage::loadStorageFromDisk(CompletionHandler<void(HashMap<String, std::unique_ptr<HashSaltForOrigin>>&&)>&& completionHandler)
{
    m_queue->dispatch([this, protectedThis = Ref { *this }, completionHandler = WTFMove(completionHandler)]() mutable {
        ASSERT(!RunLoop::isMain());

        FileSystem::makeAllDirectories(m_deviceIdHashSaltStorageDirectory);

        HashMap<String, std::unique_ptr<HashSaltForOrigin>> deviceIdHashSaltForOrigins;
        for (auto& origin : FileSystem::listDirectory(m_deviceIdHashSaltStorageDirectory)) {
            auto originPath = FileSystem::pathByAppendingComponent(m_deviceIdHashSaltStorageDirectory, origin);
            auto deviceIdHashSalt = URL::fileURLWithFileSystemPath(originPath).lastPathComponent().toString();

            if (hashSaltSize != deviceIdHashSalt.length()) {
                RELEASE_LOG_ERROR(DiskPersistency, "DeviceIdHashSaltStorage: The length of the hash salt (%d) is different to the length of the hash salts defined in WebKit (%d)", deviceIdHashSalt.length(), hashSaltSize);
                continue;
            }

            if (!FileSystem::fileSize(originPath)) {
                RELEASE_LOG_ERROR(DiskPersistency, "DeviceIdHashSaltStorage: Impossible to get the file size of: '%s'", originPath.utf8().data());
                continue;
            }

            auto decoder = createForFile(originPath);

            if (!decoder) {
                RELEASE_LOG_ERROR(DiskPersistency, "DeviceIdHashSaltStorage: Impossible to access the file to restore the hash salt: '%s'", originPath.utf8().data());
                continue;
            }

            auto hashSaltForOrigin = getDataFromDecoder(decoder.get(), WTFMove(deviceIdHashSalt));
            if (!hashSaltForOrigin)
                continue;

            auto origins = makeString(hashSaltForOrigin->documentOrigin.toString(), hashSaltForOrigin->parentOrigin.toString());
            auto deviceIdHashSaltForOrigin = deviceIdHashSaltForOrigins.ensure(origins, [hashSaltForOrigin = WTFMove(hashSaltForOrigin)] () mutable {
                return WTFMove(hashSaltForOrigin);
            });

            if (!deviceIdHashSaltForOrigin.isNewEntry)
                RELEASE_LOG_ERROR(DiskPersistency, "DeviceIdHashSaltStorage: There are two files with different hash salts for the same origin: '%s'", originPath.utf8().data());
        }

        RunLoop::main().dispatch([deviceIdHashSaltForOrigins = WTFMove(deviceIdHashSaltForOrigins), completionHandler = WTFMove(completionHandler)]() mutable {
            completionHandler(WTFMove(deviceIdHashSaltForOrigins));
        });
    });
}

std::unique_ptr<DeviceIdHashSaltStorage::HashSaltForOrigin> DeviceIdHashSaltStorage::getDataFromDecoder(KeyedDecoder* decoder, String&& deviceIdHashSalt) const
{
    auto securityOriginData = getSecurityOriginData("origin"_s, decoder);
    if (!securityOriginData) {
        RELEASE_LOG_ERROR(DiskPersistency, "DeviceIdHashSaltStorage: The security origin data in the file is not correct: '%s'", deviceIdHashSalt.utf8().data());
        return nullptr;
    }

    auto parentSecurityOriginData = getSecurityOriginData("parentOrigin"_s, decoder);
    if (!parentSecurityOriginData) {
        RELEASE_LOG_ERROR(DiskPersistency, "DeviceIdHashSaltStorage: The parent security origin data in the file is not correct: '%s'", deviceIdHashSalt.utf8().data());
        return nullptr;
    }

    double lastTimeUsed;
    if (!decoder->decodeDouble("lastTimeUsed"_s, lastTimeUsed)) {
        RELEASE_LOG_ERROR(DiskPersistency, "DeviceIdHashSaltStorage: The last time used was not correctly restored for: '%s'", deviceIdHashSalt.utf8().data());
        return nullptr;
    }

    auto hashSaltForOrigin = makeUnique<HashSaltForOrigin>(WTFMove(securityOriginData.value()), WTFMove(parentSecurityOriginData.value()), WTFMove(deviceIdHashSalt));

    hashSaltForOrigin->lastTimeUsed = WallTime::fromRawSeconds(lastTimeUsed);

    return hashSaltForOrigin;
}

std::unique_ptr<KeyedEncoder> DeviceIdHashSaltStorage::createEncoderFromData(const HashSaltForOrigin& hashSaltForOrigin) const
{
    auto encoder = KeyedEncoder::encoder();
    encoder->encodeString("origin"_s, hashSaltForOrigin.documentOrigin.databaseIdentifier());
    encoder->encodeString("parentOrigin"_s, hashSaltForOrigin.parentOrigin.databaseIdentifier());
    encoder->encodeDouble("lastTimeUsed"_s, hashSaltForOrigin.lastTimeUsed.secondsSinceEpoch().value());
    return encoder;
}

void DeviceIdHashSaltStorage::storeHashSaltToDisk(const HashSaltForOrigin& hashSaltForOrigin)
{
    if (m_deviceIdHashSaltStorageDirectory.isEmpty())
        return;

    m_queue->dispatch([this, protectedThis = Ref { *this }, hashSaltForOrigin = hashSaltForOrigin.isolatedCopy()]() mutable {
        auto encoder = createEncoderFromData(hashSaltForOrigin);
        writeToDisk(WTFMove(encoder), FileSystem::pathByAppendingComponent(m_deviceIdHashSaltStorageDirectory, hashSaltForOrigin.deviceIdHashSalt));
    });
}

void DeviceIdHashSaltStorage::completeDeviceIdHashSaltForOriginCall(SecurityOriginData&& documentOrigin, SecurityOriginData&& parentOrigin, CompletionHandler<void(String&&)>&& completionHandler)
{
    auto origins = makeString(documentOrigin.toString(), parentOrigin.toString());
    auto& deviceIdHashSalt = m_deviceIdHashSaltForOrigins.ensure(origins, [documentOrigin = WTFMove(documentOrigin), parentOrigin = WTFMove(parentOrigin)] () mutable {
        uint64_t randomData[randomDataSize];
        cryptographicallyRandomValues(randomData, sizeof(randomData));

        StringBuilder builder;
        builder.reserveCapacity(hashSaltSize);
        for (unsigned i = 0; i < randomDataSize; i++)
            builder.append(hex(randomData[i]));

        String deviceIdHashSalt = builder.toString();

        auto newHashSaltForOrigin = makeUnique<HashSaltForOrigin>(WTFMove(documentOrigin), WTFMove(parentOrigin), WTFMove(deviceIdHashSalt));

        return newHashSaltForOrigin;
    }).iterator->value;

    deviceIdHashSalt->lastTimeUsed = WallTime::now();

    storeHashSaltToDisk(*deviceIdHashSalt.get());

    completionHandler(String(deviceIdHashSalt->deviceIdHashSalt));
}

void DeviceIdHashSaltStorage::deviceIdHashSaltForOrigin(const SecurityOrigin& documentOrigin, const SecurityOrigin& parentOrigin, CompletionHandler<void(String&&)>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    if (!m_isLoaded) {
        m_pendingCompletionHandlers.append([this, documentOrigin = documentOrigin.data().isolatedCopy(), parentOrigin = parentOrigin.data().isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
            completeDeviceIdHashSaltForOriginCall(WTFMove(documentOrigin), WTFMove(parentOrigin), WTFMove(completionHandler));
        });
        return;
    }

    completeDeviceIdHashSaltForOriginCall(SecurityOriginData(documentOrigin.data()), SecurityOriginData(parentOrigin.data()), WTFMove(completionHandler));
}

void DeviceIdHashSaltStorage::getDeviceIdHashSaltOrigins(CompletionHandler<void(HashSet<SecurityOriginData>&&)>&& completionHandler)
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

void DeviceIdHashSaltStorage::deleteHashSaltFromDisk(const HashSaltForOrigin& hashSaltForOrigin)
{
    m_queue->dispatch([this, protectedThis = Ref { *this }, deviceIdHashSalt = hashSaltForOrigin.deviceIdHashSalt.isolatedCopy()]() mutable {
        ASSERT(!RunLoop::isMain());

        String fileFullPath = FileSystem::pathByAppendingComponent(m_deviceIdHashSaltStorageDirectory, deviceIdHashSalt);
        FileSystem::deleteFile(fileFullPath);
    });
}

void DeviceIdHashSaltStorage::deleteDeviceIdHashSaltForOrigins(const Vector<SecurityOriginData>& origins, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_deviceIdHashSaltForOrigins.removeIf([this, &origins](auto& keyAndValue) {
        bool needsRemoval = origins.contains(keyAndValue.value->documentOrigin) || origins.contains(keyAndValue.value->parentOrigin);
        if (m_deviceIdHashSaltStorageDirectory.isEmpty())
            return needsRemoval;
        if (needsRemoval)
            this->deleteHashSaltFromDisk(*keyAndValue.value.get());
        return needsRemoval;
    });

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

void DeviceIdHashSaltStorage::deleteDeviceIdHashSaltOriginsModifiedSince(WallTime time, CompletionHandler<void()>&& completionHandler)
{
    ASSERT(RunLoop::isMain());

    m_deviceIdHashSaltForOrigins.removeIf([this, time](auto& keyAndValue) {
        bool needsRemoval = keyAndValue.value->lastTimeUsed > time;
        if (m_deviceIdHashSaltStorageDirectory.isEmpty())
            return needsRemoval;
        if (needsRemoval)
            this->deleteHashSaltFromDisk(*keyAndValue.value.get());
        return needsRemoval;
    });

    RunLoop::main().dispatch(WTFMove(completionHandler));
}

} // namespace WebKit
