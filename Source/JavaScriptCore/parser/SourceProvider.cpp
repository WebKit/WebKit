/*
 * Copyright (C) 2013-2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SourceProvider.h"

#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/ProcessID.h>
#include <wtf/text/MakeString.h>

namespace JSC {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(StringSourceProvider);

SourceProvider::SourceProvider(const SourceOrigin& sourceOrigin, String&& sourceURL, String&& preRedirectURL, SourceTaintedOrigin taintedness, const TextPosition& startPosition, SourceProviderSourceType sourceType)
    : m_sourceType(sourceType)
    , m_sourceOrigin(sourceOrigin)
    , m_sourceURL(WTF::move(sourceURL))
    , m_preRedirectURL(WTF::move(preRedirectURL))
    , m_startPosition(startPosition)
    , m_taintedness(taintedness)
{
}

SourceProvider::~SourceProvider() = default;

void SourceProvider::lockUnderlyingBuffer()
{
    if (!m_lockingCount++)
        lockUnderlyingBufferImpl();
}

void SourceProvider::unlockUnderlyingBuffer()
{
    if (!--m_lockingCount)
        unlockUnderlyingBufferImpl();
}

CodeBlockHash SourceProvider::codeBlockHashConcurrently(int startOffset, int endOffset, CodeSpecializationKind kind)
{
    auto entireSourceCode = source();
    return CodeBlockHash { entireSourceCode.substring(startOffset, endOffset - startOffset), entireSourceCode, kind };
}

void SourceProvider::lockUnderlyingBufferImpl() { }

void SourceProvider::unlockUnderlyingBufferImpl() { }

void SourceProvider::getID()
{
    if (!m_id) {
        static std::atomic<SourceID> nextProviderID = nullID;
        m_id = ++nextProviderID;
        RELEASE_ASSERT(m_id);
    }
}

const String& SourceProvider::sourceURLStripped()
{
    if (m_sourceURL.isNull()) [[unlikely]]
        return m_sourceURLStripped;
    if (!m_sourceURLStripped.isNull()) [[likely]]
        return m_sourceURLStripped;
    m_sourceURLStripped = URL(m_sourceURL).strippedForUseAsReport();
    return m_sourceURLStripped;
}

CString SourceProvider::sourceCodeDumpFilePath(const CString& dumpDirectory)
{
    if (m_sourceCodeDumped.load(std::memory_order_acquire)) {
        Locker locker { m_sourceCodeDumpLock };
        return m_sourceCodeDumpFilePath;
    }

    Locker locker { m_sourceCodeDumpLock };
    if (m_sourceCodeDumped.load(std::memory_order_relaxed))
        return m_sourceCodeDumpFilePath;

    auto tryExtractLocalPath = [](const String& urlString) -> String {
        if (urlString.isNull())
            return { };
        if (urlString.startsWith('/'))
            return urlString;
        if (urlString.startsWith("file://"_s))
            return URL(urlString).fileSystemPath();
        return { };
    };

    String localPath = tryExtractLocalPath(sourceURL());

    if (!localPath.isNull())
        m_sourceCodeDumpFilePath = FileSystem::fileSystemRepresentation(localPath);
    else {
        auto baseName = makeString("source-"_s, asID(), '-', WTF::getCurrentProcessID());
        String filePath;
        FileSystem::FileHandle handle;
        if (dumpDirectory.isNull()) {
            auto result = FileSystem::openTemporaryFile(baseName, ".js"_s);
            filePath = result.first;
            handle = WTF::move(result.second);
        } else {
            filePath = makeString(String::fromUTF8(dumpDirectory.span()), FileSystem::pathSeparator, baseName, ".js"_s);
            handle = FileSystem::openFile(filePath, FileSystem::FileOpenMode::Truncate);
        }
        if (handle) {
            auto sourceText = source().utf8();
            handle.write(WTF::asByteSpan(sourceText.span()));
            handle.flush();
            m_sourceCodeDumpFilePath = FileSystem::fileSystemRepresentation(filePath);
        }
    }

    m_sourceCodeDumped.store(true, std::memory_order_release);
    return m_sourceCodeDumpFilePath;
}

#if ENABLE(WEBASSEMBLY)
BaseWebAssemblySourceProvider::BaseWebAssemblySourceProvider(const SourceOrigin& sourceOrigin, String&& sourceURL)
    : SourceProvider(sourceOrigin, WTF::move(sourceURL), String(), SourceTaintedOrigin::Untainted, TextPosition(), SourceProviderSourceType::WebAssembly)
{
}
#endif

} // namespace JSC

