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
#include <WebCore/SharedBuffer.h>
#include <string>
#include <wtf/CompletionHandler.h>
#include <wtf/FileSystem.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/RunLoop.h>
#include <wtf/WorkQueue.h>
#include <wtf/persistence/PersistentDecoder.h>
#include <wtf/persistence/PersistentEncoder.h>


namespace API {
using namespace WebKit::NetworkCache;
using namespace FileSystem;

ContentRuleListStore& ContentRuleListStore::defaultStore()
{
    static ContentRuleListStore* defaultStore = adoptRef(new ContentRuleListStore()).leakRef();
    return *defaultStore;
}

Ref<ContentRuleListStore> ContentRuleListStore::storeWithPath(const WTF::String& storePath)
{
    return adoptRef(*new ContentRuleListStore(storePath));
}

ContentRuleListStore::ContentRuleListStore()
    : ContentRuleListStore(defaultStorePath())
{
}

ContentRuleListStore::ContentRuleListStore(const WTF::String& storePath)
    : m_storePath(storePath)
    , m_compileQueue(WorkQueue::create("ContentRuleListStore Compile Queue", WorkQueue::Type::Concurrent))
    , m_readQueue(WorkQueue::create("ContentRuleListStore Read Queue"))
    , m_removeQueue(WorkQueue::create("ContentRuleListStore Remove Queue"))
{
    makeAllDirectories(storePath);
}

ContentRuleListStore::~ContentRuleListStore() = default;

// FIXME: Remove legacyFilename in 2022 or 2023 after users have had a chance to run the updating logic.
static const WTF::String& constructedPathPrefix(bool legacyFilename)
{
    static NeverDestroyed<WTF::String> prefix("ContentRuleList-");
    static NeverDestroyed<WTF::String> legacyPrefix("ContentExtension-");
    if (legacyFilename)
        return legacyPrefix;
    return prefix;
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

template<typename T> void getData(const T&, const Function<bool(Span<const uint8_t>)>&);
template<> void getData(const WebKit::NetworkCache::Data& data, const Function<bool(Span<const uint8_t>)>& function)
{
    data.apply(function);
}
template<> void getData(const WebCore::SharedBuffer& data, const Function<bool(Span<const uint8_t>)>& function)
{
    function({ data.data(), data.size() });
}

template<typename T>
static std::optional<ContentRuleListMetaData> decodeContentRuleListMetaData(const T& fileData)
{
    bool success = false;
    ContentRuleListMetaData metaData;
    getData(fileData, [&metaData, &success, &fileData](Span<const uint8_t> span) {
        // The file data should be mapped into one continuous memory segment so the size
        // passed to the applier should always equal the data size.
        if (span.size() != fileData.size())
            return false;

        WTF::Persistence::Decoder decoder(span);
        
        std::optional<uint32_t> version;
        decoder >> version;
        if (!version)
            return false;
        metaData.version = WTFMove(*version);

        std::optional<uint64_t> sourceSize;
        decoder >> sourceSize;
        if (!sourceSize)
            return false;
        metaData.sourceSize = WTFMove(*sourceSize);

        std::optional<uint64_t> actionsSize;
        decoder >> actionsSize;
        if (!actionsSize)
            return false;
        metaData.actionsSize = WTFMove(*actionsSize);

        std::optional<uint64_t> filtersWithoutConditionsBytecodeSize;
        decoder >> filtersWithoutConditionsBytecodeSize;
        if (!filtersWithoutConditionsBytecodeSize)
            return false;
        metaData.filtersWithoutConditionsBytecodeSize = WTFMove(*filtersWithoutConditionsBytecodeSize);

        std::optional<uint64_t> filtersWithConditionsBytecodeSize;
        decoder >> filtersWithConditionsBytecodeSize;
        if (!filtersWithConditionsBytecodeSize)
            return false;
        metaData.filtersWithConditionsBytecodeSize = WTFMove(*filtersWithConditionsBytecodeSize);

        std::optional<uint64_t> conditionedFiltersBytecodeSize;
        decoder >> conditionedFiltersBytecodeSize;
        if (!conditionedFiltersBytecodeSize)
            return false;
        metaData.conditionedFiltersBytecodeSize = WTFMove(*conditionedFiltersBytecodeSize);

        std::optional<uint32_t> conditionsApplyOnlyToDomain;
        decoder >> conditionsApplyOnlyToDomain;
        if (!conditionsApplyOnlyToDomain)
            return false;
        metaData.conditionsApplyOnlyToDomain = WTFMove(*conditionsApplyOnlyToDomain);

        success = true;
        return false;
    });
    if (!success)
        return std::nullopt;
    return metaData;
}

struct MappedData {
    ContentRuleListMetaData metaData;
    WebKit::NetworkCache::Data data;
};

static std::optional<MappedData> openAndMapContentRuleList(const WTF::String& path)
{
    FileSystem::makeSafeToUseMemoryMapForPath(path);
    WebKit::NetworkCache::Data fileData = mapFile(fileSystemRepresentation(path).data());
    if (fileData.isNull())
        return std::nullopt;
    auto metaData = decodeContentRuleListMetaData(fileData);
    if (!metaData)
        return std::nullopt;
    return {{ WTFMove(*metaData), { WTFMove(fileData) }}};
}

static bool writeDataToFile(const WebKit::NetworkCache::Data& fileData, PlatformFileHandle fd)
{
    bool success = true;
    fileData.apply([fd, &success](Span<const uint8_t> span) {
        if (writeToFile(fd, span.data(), span.size()) == -1) {
            success = false;
            return false;
        }
        return true;
    });
    
    return success;
}

static Expected<MappedData, std::error_code> compiledToFile(WTF::String&& json, Vector<WebCore::ContentExtensions::ContentExtensionRule>&& parsedRules, const WTF::String& finalFilePath)
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
        return makeUnexpected(ContentRuleListStore::Error::CompileFailed);
    }
    
    char invalidHeader[ContentRuleListFileHeaderSize];
    memset(invalidHeader, 0xFF, sizeof(invalidHeader));
    // This header will be rewritten in CompilationClient::finalize.
    if (writeToFile(temporaryFileHandle, invalidHeader, sizeof(invalidHeader)) == -1) {
        WTFLogAlways("Content Rule List compiling failed: Writing header to file failed.");
        closeFile(temporaryFileHandle);
        return makeUnexpected(ContentRuleListStore::Error::CompileFailed);
    }

    ContentRuleListMetaData metaData;
    CompilationClient compilationClient(temporaryFileHandle, metaData);
    
    if (auto compilerError = compileRuleList(compilationClient, WTFMove(json), WTFMove(parsedRules))) {
        WTFLogAlways("Content Rule List compiling failed: Compiling failed.");
        closeFile(temporaryFileHandle);
        return makeUnexpected(compilerError);
    }
    if (compilationClient.hadErrorWhileWritingToFile()) {
        WTFLogAlways("Content Rule List compiling failed: Writing to file failed.");
        closeFile(temporaryFileHandle);
        return makeUnexpected(ContentRuleListStore::Error::CompileFailed);
    }

    auto mappedData = adoptAndMapFile(temporaryFileHandle, 0, metaData.fileSize());
    if (mappedData.isNull()) {
        WTFLogAlways("Content Rule List compiling failed: Mapping file failed.");
        return makeUnexpected(ContentRuleListStore::Error::CompileFailed);
    }

    // Try and delete any files at the destination instead of overwriting them
    // in case there is already a file there and it is mmapped.
    deleteFile(finalFilePath);

    if (!moveFile(temporaryFilePath, finalFilePath)) {
        WTFLogAlways("Content Rule List compiling failed: Moving file failed.");
        return makeUnexpected(ContentRuleListStore::Error::CompileFailed);
    }
    
    FileSystem::makeSafeToUseMemoryMapForPath(finalFilePath);
    
    return {{ WTFMove(metaData), WTFMove(mappedData) }};
}

static Ref<API::ContentRuleList> createExtension(const WTF::String& identifier, MappedData&& data)
{
    auto sharedMemory = data.data.tryCreateSharedMemory();

    // Content extensions are always compiled to files, and at this point the file
    // has been already mapped, therefore tryCreateSharedMemory() cannot fail.
    ASSERT(sharedMemory);

    const size_t headerAndSourceSize = ContentRuleListFileHeaderSize + data.metaData.sourceSize;
    auto compiledContentRuleListData = WebKit::WebCompiledContentRuleListData(
        WTFMove(sharedMemory),
        ConditionsApplyOnlyToDomainOffset,
        headerAndSourceSize,
        data.metaData.actionsSize,
        headerAndSourceSize
            + data.metaData.actionsSize,
        data.metaData.filtersWithoutConditionsBytecodeSize,
        headerAndSourceSize
            + data.metaData.actionsSize
            + data.metaData.filtersWithoutConditionsBytecodeSize,
        data.metaData.filtersWithConditionsBytecodeSize,
        headerAndSourceSize
            + data.metaData.actionsSize
            + data.metaData.filtersWithoutConditionsBytecodeSize
            + data.metaData.filtersWithConditionsBytecodeSize,
        data.metaData.conditionedFiltersBytecodeSize
    );
    auto compiledContentRuleList = WebKit::WebCompiledContentRuleList::create(WTFMove(compiledContentRuleListData));
    return API::ContentRuleList::create(identifier, WTFMove(compiledContentRuleList), WTFMove(data.data));
}

static WTF::String getContentRuleListSourceFromMappedFile(const MappedData& mappedData)
{
    ASSERT(!RunLoop::isMain());

    switch (mappedData.metaData.version) {
    case 9:
    case 10:
    case 11:
        if (!mappedData.metaData.sourceSize)
            return { };
        bool is8Bit = mappedData.data.data()[ContentRuleListFileHeaderSize];
        size_t start = ContentRuleListFileHeaderSize + sizeof(bool);
        size_t length = mappedData.metaData.sourceSize - sizeof(bool);
        if (is8Bit)
            return WTF::String(mappedData.data.data() + start, length);
        if (length % sizeof(UChar)) {
            ASSERT_NOT_REACHED();
            return { };
        }
        return WTF::String(reinterpret_cast<const UChar*>(mappedData.data.data() + start), length / sizeof(UChar));
    }

    // Older versions cannot recover the original JSON source from disk.
    return { };
}

void ContentRuleListStore::lookupContentRuleList(const WTF::String& identifier, CompletionHandler<void(RefPtr<API::ContentRuleList>, std::error_code)> completionHandler)
{
    ASSERT(RunLoop::isMain());
    m_readQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), storePath = m_storePath.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        auto path = constructedPath(storePath, identifier, false);
        auto legacyPath = constructedPath(storePath, identifier, true);
        if (fileExists(legacyPath)) {
            // Try and delete any files at the destination instead of overwriting them
            // in case there is already a file there and it is mmapped.
            deleteFile(path);
            if (!moveFile(legacyPath, path)) {
                WTFLogAlways("Content Rule List lookup failed: Moving a legacy file failed.");
                if (!deleteFile(legacyPath))
                    WTFLogAlways("Content Rule List lookup failed: Deleting a legacy file failed.");
                RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
                    completionHandler(nullptr, Error::LookupFailed);
                });
                return;
            }
        }

        auto contentRuleList = openAndMapContentRuleList(path);
        if (!contentRuleList) {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(nullptr, Error::LookupFailed);
            });
            return;
        }
        
        if (contentRuleList->metaData.version != ContentRuleListStore::CurrentContentRuleListFileVersion) {
            if (auto sourceFromOldVersion = getContentRuleListSourceFromMappedFile(*contentRuleList); !sourceFromOldVersion.isEmpty()) {
                RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), sourceFromOldVersion = sourceFromOldVersion.isolatedCopy(), identifier = identifier.isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
                    protectedThis->compileContentRuleList(identifier, WTFMove(sourceFromOldVersion), WTFMove(completionHandler));
                });
                return;
            }
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(nullptr, Error::VersionMismatch);
            });
            return;
        }
        
        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), identifier = identifier.isolatedCopy(), contentRuleList = WTFMove(*contentRuleList), completionHandler = WTFMove(completionHandler)] () mutable {
            completionHandler(createExtension(identifier, WTFMove(contentRuleList)), { });
        });
    });
}

void ContentRuleListStore::getAvailableContentRuleListIdentifiers(CompletionHandler<void(WTF::Vector<WTF::String>)> completionHandler)
{
    ASSERT(RunLoop::isMain());
    m_readQueue->dispatch([protectedThis = makeRef(*this), storePath = m_storePath.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        auto prefix = constructedPathPrefix(false /*legacy*/);
        auto prefixLength = prefix.length();
        auto legacyPrefix = constructedPathPrefix(true /*legacy*/);
        auto legacyPrefixLength = legacyPrefix.length();

        Vector<WTF::String> identifiers;
        for (auto& fileName : listDirectory(storePath)) {
            if (fileName.startsWith(prefix))
                identifiers.append(decodeFromFilename(fileName.substring(prefixLength)));
            else if (fileName.startsWith(legacyPrefix))
                identifiers.append(decodeFromFilename(fileName.substring(legacyPrefixLength)));
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), identifiers = WTFMove(identifiers)]() mutable {
            completionHandler(WTFMove(identifiers));
        });
    });
}

void ContentRuleListStore::compileContentRuleList(const WTF::String& identifier, WTF::String&& json, CompletionHandler<void(RefPtr<API::ContentRuleList>, std::error_code)> completionHandler)
{
    ASSERT(RunLoop::isMain());
    AtomString::init();
    WebCore::QualifiedName::init();
    
    auto parsedRules = WebCore::ContentExtensions::parseRuleList(json);
    if (!parsedRules.has_value())
        return completionHandler(nullptr, parsedRules.error());
    
    m_compileQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), json = json.isolatedCopy(), parsedRules = parsedRules.value().isolatedCopy(), storePath = m_storePath.isolatedCopy(), completionHandler = WTFMove(completionHandler)] () mutable {
        auto path = constructedPath(storePath, identifier, false);

        auto result = compiledToFile(WTFMove(json), WTFMove(parsedRules), path);
        if (!result.has_value()) {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), error = WTFMove(result.error()), completionHandler = WTFMove(completionHandler)] () mutable {
                completionHandler(nullptr, error);
            });
            return;
        }

        RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), identifier = WTFMove(identifier), data = WTFMove(result.value()), completionHandler = WTFMove(completionHandler)] () mutable {
            auto contentRuleList = createExtension(identifier, WTFMove(data));
            completionHandler(contentRuleList.ptr(), { });
        });
    });
}

void ContentRuleListStore::removeContentRuleList(const WTF::String& identifier, CompletionHandler<void(std::error_code)> completionHandler)
{
    ASSERT(RunLoop::isMain());
    m_removeQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), storePath = m_storePath.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        auto complete = [protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)](std::error_code error) mutable {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), error = WTFMove(error)] () mutable {
                completionHandler(error);
            });
        };
        if (auto legacyPath = constructedPath(storePath, identifier, true); fileExists(legacyPath)) {
            if (!deleteFile(legacyPath))
                return complete(Error::RemoveFailed);
            return complete({ });
        }
        if (auto path = constructedPath(storePath, identifier, false); !deleteFile(path))
            return complete(Error::RemoveFailed);
        complete({ });
    });
}

void ContentRuleListStore::synchronousRemoveAllContentRuleLists()
{
    for (const auto& fileName : listDirectory(m_storePath))
        deleteFile(FileSystem::pathByAppendingComponent(m_storePath, fileName));
}

void ContentRuleListStore::invalidateContentRuleListVersion(const WTF::String& identifier)
{
    auto file = openFile(constructedPath(m_storePath, identifier, false), FileOpenMode::Write);
    if (file == invalidPlatformFileHandle)
        return;
    ContentRuleListMetaData invalidHeader = {0, 0, 0, 0, 0, 0};
    auto bytesWritten = writeToFile(file, &invalidHeader, sizeof(invalidHeader));
    ASSERT_UNUSED(bytesWritten, bytesWritten == sizeof(invalidHeader));
    closeFile(file);
}

void ContentRuleListStore::getContentRuleListSource(const WTF::String& identifier, CompletionHandler<void(WTF::String)> completionHandler)
{
    ASSERT(RunLoop::isMain());
    m_readQueue->dispatch([protectedThis = makeRef(*this), identifier = identifier.isolatedCopy(), storePath = m_storePath.isolatedCopy(), completionHandler = WTFMove(completionHandler)]() mutable {
        auto complete = [protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler)](WTF::String source) mutable {
            RunLoop::main().dispatch([protectedThis = WTFMove(protectedThis), completionHandler = WTFMove(completionHandler), source = source.isolatedCopy()] () mutable {
                completionHandler(source);
            });
        };

        auto contentRuleList = openAndMapContentRuleList(constructedPath(storePath, identifier, false));
        if (!contentRuleList) {
            auto legacyContentRuleList = openAndMapContentRuleList(constructedPath(storePath, identifier, true));
            if (!legacyContentRuleList)
                return complete({ });
            return complete(getContentRuleListSourceFromMappedFile(*legacyContentRuleList));
        }
        complete(getContentRuleListSourceFromMappedFile(*contentRuleList));
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
