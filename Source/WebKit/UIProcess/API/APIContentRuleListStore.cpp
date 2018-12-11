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
#include "APIContentRuleListStore.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include "APIContentRuleList.h"
#include "NetworkCacheData.h"
#include "NetworkCacheFileSystem.h"
#include "SharedMemory.h"
#include "WebCompiledContentRuleList.h"
#include <WebCore/ContentExtensionCompiler.h>
#include <WebCore/ContentExtensionError.h>
#include <WebCore/ContentExtensionParser.h>
#include <WebCore/QualifiedName.h>
#include <string>
#include <wtf/CompletionHandler.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>


namespace API {
using namespace WebKit::NetworkCache;
using namespace WebCore::FileSystem;

ContentRuleListStore& ContentRuleListStore::legacyDefaultStore()
{
    const bool legacyFilename = true;
    static ContentRuleListStore* defaultStore = adoptRef(new ContentRuleListStore(legacyFilename)).leakRef();
    return *defaultStore;
}
    
ContentRuleListStore& ContentRuleListStore::nonLegacyDefaultStore()
{
    const bool legacyFilename = false;
    static ContentRuleListStore* defaultStore = adoptRef(new ContentRuleListStore(legacyFilename)).leakRef();
    return *defaultStore;
}
    
ContentRuleListStore& ContentRuleListStore::defaultStore(bool legacyFilename)
{
    if (legacyFilename)
        return legacyDefaultStore();
    return nonLegacyDefaultStore();
}

Ref<ContentRuleListStore> ContentRuleListStore::storeWithPath(const WTF::String& storePath, bool legacyFilename)
{
    return adoptRef(*new ContentRuleListStore(storePath, legacyFilename));
}

ContentRuleListStore::ContentRuleListStore(bool legacyFilename)
    : ContentRuleListStore(defaultStorePath(legacyFilename), legacyFilename)
{
}

ContentRuleListStore::ContentRuleListStore(const WTF::String& storePath, bool legacyFilename)
    : m_storePath(storePath)
    , m_compileQueue(WorkQueue::create("ContentRuleListStore Compile Queue", WorkQueue::Type::Concurrent))
    , m_readQueue(WorkQueue::create("ContentRuleListStore Read Queue"))
    , m_removeQueue(WorkQueue::create("ContentRuleListStore Remove Queue"))
    , m_legacyFilename(legacyFilename)
{
    makeAllDirectories(storePath);
}

ContentRuleListStore::~ContentRuleListStore()
{
}

static const WTF::String& constructedPathPrefix(bool legacyFilename)
{
    static NeverDestroyed<WTF::String> prefix("ContentRuleList-");
    static NeverDestroyed<WTF::String> legacyPrefix("ContentExtension-");
    if (legacyFilename)
        return legacyPrefix;
    return prefix;
}

static const WTF::String constructedPathFilter(bool legacyFilename)
{
    return makeString(constructedPathPrefix(legacyFilename), '*');
}

static WTF::String constructedPath(const WTF::String& base, const WTF::String& identifier, bool legacyFilename)
{
    return pathByAppendingComponent(base, makeString(constructedPathPrefix(legacyFilename), encodeForFileName(identifier)));
}

// The size and offset of the densely packed bytes in the file, not sizeof and offsetof, which would
// represent the size and offset of the structure in memory, possibly with compiler-added padding.
const size_t ContentRuleListFileHeaderSize = 2 * sizeof(uint32_t) + 5 * sizeof(uint64_t);
const size_t ConditionsApplyOnlyToDomainOffset = sizeof(uint32_t) + 5 * sizeof(uint64_t);

struct ContentRuleListMetaData {
    uint32_t version { ContentRuleListStore::CurrentContentRuleListFileVersion };
    uint64_t sourceSize { 0 };
    uint64_t actionsSize { 0 };
    uint64_t filtersWithoutConditionsBytecodeSize { 0 };
    uint64_t filtersWithConditionsBytecodeSize { 0 };
    uint64_t conditionedFiltersBytecodeSize { 0 };
    uint32_t conditionsApplyOnlyToDomain { false };
    
    size_t fileSize() const
    {
        return ContentRuleListFileHeaderSize
            + sourceSize
            + actionsSize
            + filtersWithoutConditionsBytecodeSize
            + filtersWithConditionsBytecodeSize
            + conditionedFiltersBytecodeSize;
    }
};

static WebKit::NetworkCache::Data encodeContentRuleListMetaData(const ContentRuleListMetaData& metaData)
{
    WTF::Persistence::Encoder encoder;

    encoder << metaData.version;
    encoder << metaData.sourceSize;
    encoder << metaData.actionsSize;
    encoder << metaData.filtersWithoutConditionsBytecodeSize;
    encoder << metaData.filtersWithConditionsBytecodeSize;
    encoder << metaData.conditionedFiltersBytecodeSize;
    encoder << metaData.conditionsApplyOnlyToDomain;

    ASSERT(encoder.bufferSize() == ContentRuleListFileHeaderSize);
    return WebKit::NetworkCache::Data(encoder.buffer(), encoder.bufferSize());
}

static bool decodeContentRuleListMetaData(ContentRuleListMetaData& metaData, const WebKit::NetworkCache::Data& fileData)
{
    bool success = false;
    fileData.apply([&metaData, &success, &fileData](const uint8_t* data, size_t size) {
        // The file data should be mapped into one continuous memory segment so the size
        // passed to the applier should always equal the data size.
        if (size != fileData.size())
            return false;

        WTF::Persistence::Decoder decoder(data, size);
        if (!decoder.decode(metaData.version))
            return false;
        if (!decoder.decode(metaData.sourceSize))
            return false;
        if (!decoder.decode(metaData.actionsSize))
            return false;
        if (!decoder.decode(metaData.filtersWithoutConditionsBytecodeSize))
            return false;
        if (!decoder.decode(metaData.filtersWithConditionsBytecodeSize))
            return false;
        if (!decoder.decode(metaData.conditionedFiltersBytecodeSize))
            return false;
        if (!decoder.decode(metaData.conditionsApplyOnlyToDomain))
            return false;
        success = true;
        return false;
    });
    return success;
}

static bool openAndMapContentRuleList(const WTF::String& path, ContentRuleListMetaData& metaData, WebKit::NetworkCache::Data& fileData)
{
    fileData = mapFile(fileSystemRepresentation(path).data());
    if (fileData.isNull())
        return false;

    if (!decodeContentRuleListMetaData(metaData, fileData))
        return false;

    return true;
}

static bool writeDataToFile(const WebKit::NetworkCache::Data& fileData, PlatformFileHandle fd)
{
    bool success = true;
    fileData.apply([fd, &success](const uint8_t* data, size_t size) {
        if (writeToFile(fd, (const char*)data, size) == -1) {
            success = false;
            return false;
        }
        return true;
    });
    
    return success;
}

static std::error_code compiledToFile(WTF::String&& json, Vector<WebCore::ContentExtensions::ContentExtensionRule>&& parsedRules, const WTF::String& finalFilePath, ContentRuleListMetaData& metaData, WebKit::NetworkCache::Data& mappedData)
{
    using namespace WebCore::ContentExtensions;

    class CompilationClient final : public ContentExtensionCompilationClient {
    public:
        CompilationClient(PlatformFileHandle fileHandle, ContentRuleListMetaData& metaData)
            : m_fileHandle(fileHandle)
            , m_metaData(metaData)
        {
            ASSERT(!metaData.sourceSize);
            ASSERT(!metaData.actionsSize);
            ASSERT(!metaData.filtersWithoutConditionsBytecodeSize);
            ASSERT(!metaData.filtersWithConditionsBytecodeSize);
            ASSERT(!metaData.conditionedFiltersBytecodeSize);
            ASSERT(!metaData.conditionsApplyOnlyToDomain);
        }
        
        void writeSource(WTF::String&& sourceJSON) final
        {
            ASSERT(!m_filtersWithoutConditionsBytecodeWritten);
            ASSERT(!m_filtersWithConditionBytecodeWritten);
            ASSERT(!m_conditionFiltersBytecodeWritten);
            ASSERT(!m_actionsWritten);
            ASSERT(!m_sourceWritten);
            writeToFile(sourceJSON.is8Bit());
            m_sourceWritten += sizeof(bool);
            if (sourceJSON.is8Bit()) {
                size_t serializedLength = sourceJSON.length() * sizeof(LChar);
                writeToFile(WebKit::NetworkCache::Data(sourceJSON.characters8(), serializedLength));
                m_sourceWritten += serializedLength;
            } else {
                size_t serializedLength = sourceJSON.length() * sizeof(UChar);
                writeToFile(WebKit::NetworkCache::Data(reinterpret_cast<const uint8_t*>(sourceJSON.characters16()), serializedLength));
                m_sourceWritten += serializedLength;
            }
        }
        
        void writeFiltersWithoutConditionsBytecode(Vector<DFABytecode>&& bytecode) final
        {
            ASSERT(!m_filtersWithConditionBytecodeWritten);
            ASSERT(!m_conditionFiltersBytecodeWritten);
            m_filtersWithoutConditionsBytecodeWritten += bytecode.size();
            writeToFile(WebKit::NetworkCache::Data(bytecode.data(), bytecode.size()));
        }
        
        void writeFiltersWithConditionsBytecode(Vector<DFABytecode>&& bytecode) final
        {
            ASSERT(!m_conditionFiltersBytecodeWritten);
            m_filtersWithConditionBytecodeWritten += bytecode.size();
            writeToFile(WebKit::NetworkCache::Data(bytecode.data(), bytecode.size()));
        }
        
        void writeTopURLFiltersBytecode(Vector<DFABytecode>&& bytecode) final
        {
            m_conditionFiltersBytecodeWritten += bytecode.size();
            writeToFile(WebKit::NetworkCache::Data(bytecode.data(), bytecode.size()));
        }

        void writeActions(Vector<SerializedActionByte>&& actions, bool conditionsApplyOnlyToDomain) final
        {
            ASSERT(!m_filtersWithoutConditionsBytecodeWritten);
            ASSERT(!m_filtersWithConditionBytecodeWritten);
            ASSERT(!m_conditionFiltersBytecodeWritten);
            ASSERT(!m_actionsWritten);
            m_actionsWritten += actions.size();
            m_conditionsApplyOnlyToDomain = conditionsApplyOnlyToDomain;
            writeToFile(WebKit::NetworkCache::Data(actions.data(), actions.size()));
        }
        
        void finalize() final
        {
            m_metaData.sourceSize = m_sourceWritten;
            m_metaData.actionsSize = m_actionsWritten;
            m_metaData.filtersWithoutConditionsBytecodeSize = m_filtersWithoutConditionsBytecodeWritten;
            m_metaData.filtersWithConditionsBytecodeSize = m_filtersWithConditionBytecodeWritten;
            m_metaData.conditionedFiltersBytecodeSize = m_conditionFiltersBytecodeWritten;
            m_metaData.conditionsApplyOnlyToDomain = m_conditionsApplyOnlyToDomain;
            
            WebKit::NetworkCache::Data header = encodeContentRuleListMetaData(m_metaData);
            if (!m_fileError && seekFile(m_fileHandle, 0ll, FileSeekOrigin::Beginning) == -1) {
                closeFile(m_fileHandle);
                m_fileError = true;
            }
            writeToFile(header);
        }
        
        bool hadErrorWhileWritingToFile() { return m_fileError; }

    private:
        void writeToFile(bool value)
        {
            writeToFile(WebKit::NetworkCache::Data(reinterpret_cast<const uint8_t*>(&value), sizeof(value)));
        }
        void writeToFile(const WebKit::NetworkCache::Data& data)
        {
            if (!m_fileError && !writeDataToFile(data, m_fileHandle)) {
                closeFile(m_fileHandle);
                m_fileError = true;
            }
        }
        
        PlatformFileHandle m_fileHandle;
        ContentRuleListMetaData& m_metaData;
        size_t m_filtersWithoutConditionsBytecodeWritten { 0 };
        size_t m_filtersWithConditionBytecodeWritten { 0 };
        size_t m_conditionFiltersBytecodeWritten { 0 };
        size_t m_actionsWritten { 0 };
        size_t m_sourceWritten { 0 };
        bool m_conditionsApplyOnlyToDomain { false };
        bool m_fileError { false };
    };

    auto temporaryFileHandle = invalidPlatformFileHandle;
    WTF::String temporaryFilePath = openTemporaryFile("ContentRuleList", temporaryFileHandle);
    if (temporaryFileHandle == invalidPlatformFileHandle) {
        WTFLogAlways("Content Rule List compiling failed: Opening temporary file failed.");
        return ContentRuleListStore::Error::CompileFailed;
    }
    
    char invalidHeader[ContentRuleListFileHeaderSize];
    memset(invalidHeader, 0xFF, sizeof(invalidHeader));
    // This header will be rewritten in CompilationClient::finalize.
    if (writeToFile(temporaryFileHandle, invalidHeader, sizeof(invalidHeader)) == -1) {
        WTFLogAlways("Content Rule List compiling failed: Writing header to file failed.");
        closeFile(temporaryFileHandle);
        return ContentRuleListStore::Error::CompileFailed;
    }

    CompilationClient compilationClient(temporaryFileHandle, metaData);
    
    if (auto compilerError = compileRuleList(compilationClient, WTFMove(json), WTFMove(parsedRules))) {
        WTFLogAlways("Content Rule List compiling failed: Compiling failed.");
        closeFile(temporaryFileHandle);
        return compilerError;
    }
    if (compilationClient.hadErrorWhileWritingToFile()) {
        WTFLogAlways("Content Rule List compiling failed: Writing to file failed.");
        closeFile(temporaryFileHandle);
        return ContentRuleListStore::Error::CompileFailed;
    }

    mappedData = adoptAndMapFile(temporaryFileHandle, 0, metaData.fileSize());
    if (mappedData.isNull()) {
        WTFLogAlways("Content Rule List compiling failed: Mapping file failed.");
        return ContentRuleListStore::Error::CompileFailed;
    }

    if (!moveFile(temporaryFilePath, finalFilePath)) {
        WTFLogAlways("Content Rule List compiling failed: Moving file failed.");
        return ContentRuleListStore::Error::CompileFailed;
    }

    return { };
}

static Ref<API::ContentRuleList> createExtension(const WTF::String& identifier, const ContentRuleListMetaData& metaData, const WebKit::NetworkCache::Data& fileData)
{
    auto sharedMemory = WebKit::SharedMemory::create(const_cast<uint8_t*>(fileData.data()), fileData.size(), WebKit::SharedMemory::Protection::ReadOnly);
    const size_t headerAndSourceSize = ContentRuleListFileHeaderSize + metaData.sourceSize;
    auto compiledContentRuleListData = WebKit::WebCompiledContentRuleListData(
        WTFMove(sharedMemory),
        fileData,
        ConditionsApplyOnlyToDomainOffset,
        headerAndSourceSize,
        metaData.actionsSize,
        headerAndSourceSize
            + metaData.actionsSize,
        metaData.filtersWithoutConditionsBytecodeSize,
        headerAndSourceSize
            + metaData.actionsSize
            + metaData.filtersWithoutConditionsBytecodeSize,
        metaData.filtersWithConditionsBytecodeSize,
        headerAndSourceSize
            + metaData.actionsSize
            + metaData.filtersWithoutConditionsBytecodeSize
            + metaData.filtersWithConditionsBytecodeSize,
        metaData.conditionedFiltersBytecodeSize
    );
    auto compiledContentRuleList = WebKit::WebCompiledContentRuleList::create(WTFMove(compiledContentRuleListData));
    return API::ContentRuleList::create(identifier, WTFMove(compiledContentRuleList));
}

void ContentRuleListStore::lookupContentRuleList(const WTF::String& identifier, CompletionHandler<void(RefPtr<API::ContentRuleList>, std::error_code)> completionHandler)
{
    m_readQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), storePath = m_storePath.isolatedCopy(), legacyFilename = m_legacyFilename, completionHandler = WTFMove(completionHandler)]() mutable {
        auto path = constructedPath(storePath, identifier, legacyFilename);
        
        ContentRuleListMetaData metaData;
        WebKit::NetworkCache::Data fileData;
        if (!openAndMapContentRuleList(path, metaData, fileData)) {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(nullptr, Error::LookupFailed);
            });
            return;
        }
        
        if (metaData.version != ContentRuleListStore::CurrentContentRuleListFileVersion) {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(nullptr, Error::VersionMismatch);
            });
            return;
        }
        
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), identifier = identifier.isolatedCopy(), fileData = WTFMove(fileData), metaData = WTFMove(metaData), completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(createExtension(identifier, metaData, fileData), { });
        });
    });
}

void ContentRuleListStore::getAvailableContentRuleListIdentifiers(CompletionHandler<void(WTF::Vector<WTF::String>)> completionHandler)
{
    m_readQueue->dispatch([protectedThis = makeRef(*this), storePath = m_storePath.isolatedCopy(), legacyFilename = m_legacyFilename, completionHandler = WTFMove(completionHandler)]() mutable {

        Vector<WTF::String> fullPaths = listDirectory(storePath, constructedPathFilter(legacyFilename));
        Vector<WTF::String> identifiers;
        identifiers.reserveInitialCapacity(fullPaths.size());
        const auto prefixLength = constructedPathPrefix(legacyFilename).length();
        for (const auto& path : fullPaths)
            identifiers.uncheckedAppend(decodeFromFilename(path.substring(path.reverseFind('/') + 1 + prefixLength)));

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), identifiers = WTFMove(identifiers)]() mutable {
            completionHandler(WTFMove(identifiers));
        });
    });
}

void ContentRuleListStore::compileContentRuleList(const WTF::String& identifier, WTF::String&& json, CompletionHandler<void(RefPtr<API::ContentRuleList>, std::error_code)> completionHandler)
{
    AtomicString::init();
    WebCore::QualifiedName::init();
    
    auto parsedRules = WebCore::ContentExtensions::parseRuleList(json);
    if (!parsedRules.has_value())
        return completionHandler(nullptr, parsedRules.error());
    
    m_compileQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), legacyFilename = m_legacyFilename, json = json.isolatedCopy(), parsedRules = parsedRules.value().isolatedCopy(), storePath = m_storePath.isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
        auto path = constructedPath(storePath, identifier, legacyFilename);

        ContentRuleListMetaData metaData;
        WebKit::NetworkCache::Data fileData;
        auto error = compiledToFile(WTFMove(json), WTFMove(parsedRules), path, metaData, fileData);
        if (error) {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), error = WTFMove(error), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(nullptr, error);
            });
            return;
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), identifier = WTFMove(identifier), fileData = WTFMove(fileData), metaData = WTFMove(metaData), completionHandler = WTFMove(completionHandler)] () mutable {
            RefPtr<API::ContentRuleList> contentRuleList = createExtension(identifier, metaData, fileData);
            completionHandler(contentRuleList, { });
        });
    });
}

void ContentRuleListStore::removeContentRuleList(const WTF::String& identifier, CompletionHandler<void(std::error_code)> completionHandler)
{
    m_removeQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), storePath = m_storePath.isolatedCopy(), legacyFilename = m_legacyFilename, completionHandler = WTFMove(completionHandler)]() mutable {
        auto path = constructedPath(storePath, identifier, legacyFilename);

        if (!deleteFile(path)) {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(Error::RemoveFailed);
            });
            return;
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler({ });
        });
    });
}

void ContentRuleListStore::synchronousRemoveAllContentRuleLists()
{
    for (const auto& path : listDirectory(m_storePath, "*"))
        deleteFile(path);
}

void ContentRuleListStore::invalidateContentRuleListVersion(const WTF::String& identifier)
{
    auto file = openFile(constructedPath(m_storePath, identifier, m_legacyFilename), FileOpenMode::Write);
    if (file == invalidPlatformFileHandle)
        return;
    ContentRuleListMetaData invalidHeader = {0, 0, 0, 0, 0, 0};
    auto bytesWritten = writeToFile(file, reinterpret_cast<const char*>(&invalidHeader), sizeof(invalidHeader));
    ASSERT_UNUSED(bytesWritten, bytesWritten == sizeof(invalidHeader));
    closeFile(file);
}

void ContentRuleListStore::getContentRuleListSource(const WTF::String& identifier, CompletionHandler<void(WTF::String)> completionHandler)
{
    m_readQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), storePath = m_storePath.isolatedCopy(), legacyFilename = m_legacyFilename, completionHandler = WTFMove(completionHandler)]() mutable {
        auto path = constructedPath(storePath, identifier, legacyFilename);
        
        auto complete = [protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)](WTF::String source) mutable {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), source = source.isolatedCopy()] () mutable {
                completionHandler(source);
            });
        };
        
        ContentRuleListMetaData metaData;
        WebKit::NetworkCache::Data fileData;
        if (!openAndMapContentRuleList(path, metaData, fileData)) {
            complete({ });
            return;
        }
        
        switch (metaData.version) {
        case 9:
        case 10:
            if (!metaData.sourceSize) {
                complete({ });
                return;
            }
            bool is8Bit = fileData.data()[ContentRuleListFileHeaderSize];
            size_t start = ContentRuleListFileHeaderSize + sizeof(bool);
            size_t length = metaData.sourceSize - sizeof(bool);
            if (is8Bit)
                complete(WTF::String(fileData.data() + start, length));
            else {
                ASSERT(!(length % sizeof(UChar)));
                complete(WTF::String(reinterpret_cast<const UChar*>(fileData.data() + start), length / sizeof(UChar)));
            }
            return;
        }

        // Older versions cannot recover the original JSON source from disk.
        complete({ });
    });
}

const std::error_category& contentRuleListStoreErrorCategory()
{
    class ContentRuleListStoreErrorCategory : public std::error_category {
        const char* name() const noexcept final
        {
            return "content extension store";
        }

        std::string message(int errorCode) const final
        {
            switch (static_cast<ContentRuleListStore::Error>(errorCode)) {
            case ContentRuleListStore::Error::LookupFailed:
                return "Unspecified error during lookup.";
            case ContentRuleListStore::Error::VersionMismatch:
                return "Version of file does not match version of interpreter.";
            case ContentRuleListStore::Error::CompileFailed:
                return "Unspecified error during compile.";
            case ContentRuleListStore::Error::RemoveFailed:
                return "Unspecified error during remove.";
            }

            return std::string();
        }
    };

    static NeverDestroyed<ContentRuleListStoreErrorCategory> contentRuleListErrorCategory;
    return contentRuleListErrorCategory;
}

} // namespace API

#endif // ENABLE(CONTENT_EXTENSIONS)
