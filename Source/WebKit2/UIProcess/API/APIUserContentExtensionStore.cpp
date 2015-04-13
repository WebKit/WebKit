/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "APIUserContentExtensionStore.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "APIUserContentExtension.h"
#include "NetworkCacheData.h"
#include "NetworkCacheDecoder.h"
#include "NetworkCacheEncoder.h"
#include "NetworkCacheFileSystemPosix.h"
#include "SharedMemory.h"
#include "WebCompiledContentExtension.h"
#include <WebCore/ContentExtensionCompiler.h>
#include <WebCore/ContentExtensionError.h>
#include <string>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>

using namespace WebKit::NetworkCache;

namespace API {

UserContentExtensionStore& UserContentExtensionStore::defaultStore()
{
    static UserContentExtensionStore* defaultStore = adoptRef(new UserContentExtensionStore).leakRef();
    return *defaultStore;
}

UserContentExtensionStore::UserContentExtensionStore()
    : UserContentExtensionStore(defaultStorePath())
{
}

UserContentExtensionStore::UserContentExtensionStore(const WTF::String& storePath)
    : m_storePath(storePath)
    , m_compileQueue(WorkQueue::create("UserContentExtensionStore Compile Queue", WorkQueue::Type::Concurrent))
    , m_readQueue(WorkQueue::create("UserContentExtensionStore Read Queue"))
    , m_removeQueue(WorkQueue::create("UserContentExtensionStore Remove Queue"))
{
}

UserContentExtensionStore::~UserContentExtensionStore()
{
}

static String constructedPath(const String& base, const String& identifier)
{
    return WebCore::pathByAppendingComponent(base, "ContentExtension-" + WebCore::encodeForFileName(identifier));
}

struct ContentExtensionMetaData {
    uint32_t version;
    uint64_t actionsOffset;
    uint64_t actionsSize;
    uint64_t bytecodeOffset;
    uint64_t bytecodeSize;
};

static Data encodeContentExtensionMetaData(const ContentExtensionMetaData& metaData)
{
    WebKit::NetworkCache::Encoder encoder;

    encoder << metaData.version;
    encoder << metaData.actionsSize;
    encoder << metaData.bytecodeSize;

    return Data(encoder.buffer(), encoder.bufferSize());
}

static bool decodeContentExtensionMetaData(ContentExtensionMetaData& metaData, const Data& fileData)
{
    bool success = false;
    fileData.apply([&metaData, &success, &fileData](const uint8_t* data, size_t size) {
        // The file data should be mapped into one continuous memory segment so the size
        // passed to the applier should always equal the data size.
        if (size != fileData.size())
            return false;

        WebKit::NetworkCache::Decoder decoder(data, size);
        if (!decoder.decode(metaData.version))
            return false;
        if (!decoder.decode(metaData.actionsSize))
            return false;
        if (!decoder.decode(metaData.bytecodeSize))
            return false;
        metaData.actionsOffset = decoder.currentOffset();
        metaData.bytecodeOffset = metaData.actionsOffset + metaData.actionsSize;
        success = true;
        return false;
    });
    return success;
}

static bool openAndMapContentExtension(const String& path, ContentExtensionMetaData& metaData, Data& fileData)
{
    auto fd = WebCore::openFile(path, WebCore::OpenForRead);
    if (fd == WebCore::invalidPlatformFileHandle)
        return false;

    long long fileSize = 0;
    if (!WebCore::getFileSize(fd, fileSize)) {
        WebCore::closeFile(fd);
        return false;
    }

    fileData = mapFile(fd, 0, static_cast<size_t>(fileSize));
    WebCore::closeFile(fd);

    if (fileData.isNull())
        return false;

    if (!decodeContentExtensionMetaData(metaData, fileData))
        return false;

    return true;
}

static bool writeDataToFile(Data& fileData, WebCore::PlatformFileHandle fd)
{
    bool success = true;
    fileData.apply([fd, &success](const uint8_t* data, size_t size) {
        if (WebCore::writeToFile(fd, (const char*)data, size) == -1) {
            success = false;
            return false;
        }
        return true;
    });
    
    return success;
}

static std::error_code compiledToFile(const String& json, const String& finalFilePath, ContentExtensionMetaData& metaData, Data& mappedData)
{
    using namespace WebCore::ContentExtensions;

    class CompilationClient final : public ContentExtensionCompilationClient {
    public:
        CompilationClient(Data& bytecodeData, Data& actionsData)
            : m_bytecodeData(bytecodeData)
            , m_actionsData(actionsData)
        {
        }

        virtual void writeBytecode(Vector<DFABytecode>&& bytecode) override
        {
            m_bytecodeData = Data(bytecode.data(), bytecode.size());
        }

        virtual void writeActions(Vector<SerializedActionByte>&& actions) override
        {
            m_actionsData = Data(actions.data(), actions.size());
        }

    private:
        Data& m_bytecodeData;
        Data& m_actionsData;
    };

    Data bytecode;
    Data actions;
    CompilationClient compilationClient(bytecode, actions);

    // FIXME: This copies the data. Instead, we should be passing an interface
    // to the compiler that can write directly to a file.

    auto compilerError = compileRuleList(compilationClient, json);
    if (compilerError)
        return compilerError;

    auto actionsAndBytecode = concatenate(actions, bytecode);

    metaData.version = 1;
    metaData.actionsSize = actions.size();
    metaData.bytecodeSize = bytecode.size();

    auto encodedMetaData = encodeContentExtensionMetaData(metaData);

    metaData.actionsOffset = encodedMetaData.size();
    metaData.bytecodeOffset = encodedMetaData.size() + metaData.actionsSize;

    auto data = concatenate(encodedMetaData, actionsAndBytecode);
    auto dataSize = data.size();

    ASSERT(metaData.actionsOffset + metaData.actionsSize + metaData.bytecodeSize == dataSize);

    auto temporaryFileHandle = WebCore::invalidPlatformFileHandle;
    String temporaryFilePath = WebCore::openTemporaryFile("ContentExtension", temporaryFileHandle);
    if (temporaryFileHandle == WebCore::invalidPlatformFileHandle)
        return UserContentExtensionStore::Error::CompileFailed;

    if (!writeDataToFile(data, temporaryFileHandle)) {
        WebCore::closeFile(temporaryFileHandle);
        return UserContentExtensionStore::Error::CompileFailed;
    }

    mappedData = mapFile(temporaryFileHandle, 0, dataSize);
    WebCore::closeFile(temporaryFileHandle);

    if (mappedData.isNull())
        return UserContentExtensionStore::Error::CompileFailed;

    if (!WebCore::renameFile(temporaryFilePath, finalFilePath))
        return UserContentExtensionStore::Error::CompileFailed;

    return { };
}

static RefPtr<API::UserContentExtension> createExtension(const String& identifier, const ContentExtensionMetaData& metaData, const Data& fileData)
{
    auto sharedMemory = WebKit::SharedMemory::createFromVMBuffer(const_cast<uint8_t*>(fileData.data()), fileData.size());
    auto compiledContentExtensionData = WebKit::WebCompiledContentExtensionData(
        WTF::move(sharedMemory),
        fileData,
        metaData.actionsOffset,
        metaData.actionsSize,
        metaData.bytecodeOffset,
        metaData.bytecodeSize
    );
    auto compiledContentExtension = WebKit::WebCompiledContentExtension::create(WTF::move(compiledContentExtensionData));
    return API::UserContentExtension::create(identifier, WTF::move(compiledContentExtension));
}

void UserContentExtensionStore::lookupContentExtension(const WTF::String& identifier, std::function<void(RefPtr<API::UserContentExtension>, std::error_code)> completionHandler)
{
    RefPtr<UserContentExtensionStore> self(this);
    StringCapture identifierCapture(identifier);
    StringCapture pathCapture(m_storePath);

    m_readQueue->dispatch([self, identifierCapture, pathCapture, completionHandler] {
        auto path = constructedPath(pathCapture.string(), identifierCapture.string());
        
        ContentExtensionMetaData metaData;
        Data fileData;
        if (!openAndMapContentExtension(path, metaData, fileData)) {
            RunLoop::main().dispatch([self, completionHandler] {
                completionHandler(nullptr, Error::LookupFailed);
            });
            return;
        }
        
        RunLoop::main().dispatch([self, identifierCapture, fileData, metaData, completionHandler] {
            RefPtr<API::UserContentExtension> userContentExtension = createExtension(identifierCapture.string(), metaData, fileData);
            completionHandler(userContentExtension, { });
        });
    });
}

void UserContentExtensionStore::compileContentExtension(const WTF::String& identifier, const WTF::String& json, std::function<void(RefPtr<API::UserContentExtension>, std::error_code)> completionHandler)
{
    RefPtr<UserContentExtensionStore> self(this);
    StringCapture identifierCapture(identifier);
    StringCapture jsonCapture(json);
    StringCapture pathCapture(m_storePath);

    m_compileQueue->dispatch([self, identifierCapture, jsonCapture, pathCapture, completionHandler] {
        auto path = constructedPath(pathCapture.string(), identifierCapture.string());

        ContentExtensionMetaData metaData;
        Data fileData;
        auto error = compiledToFile(jsonCapture.string(), path, metaData, fileData);
        if (error) {
            RunLoop::main().dispatch([self, error, completionHandler] {
                completionHandler(nullptr, error);
            });
            return;
        }

        RunLoop::main().dispatch([self, identifierCapture, fileData, metaData, completionHandler] {
            RefPtr<API::UserContentExtension> userContentExtension = createExtension(identifierCapture.string(), metaData, fileData);
            completionHandler(userContentExtension, { });
        });
    });
}

void UserContentExtensionStore::removeContentExtension(const WTF::String& identifier, std::function<void(std::error_code)> completionHandler)
{
    RefPtr<UserContentExtensionStore> self(this);
    StringCapture identifierCapture(identifier);
    StringCapture pathCapture(m_storePath);

    m_removeQueue->dispatch([self, identifierCapture, pathCapture, completionHandler] {
        auto path = constructedPath(pathCapture.string(), identifierCapture.string());

        if (!WebCore::deleteFile(path)) {
            RunLoop::main().dispatch([self, completionHandler] {
                completionHandler(Error::RemoveFailed);
            });
            return;
        }

        RunLoop::main().dispatch([self, completionHandler] {
            completionHandler({ });
        });
    });
}

void UserContentExtensionStore::synchronousRemoveAllContentExtensions()
{
    for (const auto& path : WebCore::listDirectory(m_storePath, "*"))
        WebCore::deleteFile(path);
}

const std::error_category& userContentExtensionStoreErrorCategory()
{
    class UserContentExtensionStoreErrorCategory : public std::error_category {
        const char* name() const noexcept override
        {
            return "user content extension store";
        }

        virtual std::string message(int errorCode) const override
        {
            switch (static_cast<UserContentExtensionStore::Error>(errorCode)) {
            case UserContentExtensionStore::Error::LookupFailed:
                return "Unspecified error during lookup.";
            case UserContentExtensionStore::Error::CompileFailed:
                return "Unspecified error during compile.";
            case UserContentExtensionStore::Error::RemoveFailed:
                return "Unspecified error during remove.";
            }

            return std::string();
        }
    };

    static NeverDestroyed<UserContentExtensionStoreErrorCategory> contentExtensionErrorCategory;
    return contentExtensionErrorCategory;
}

} // namespace API

#endif // ENABLE(CONTENT_EXTENSIONS)
