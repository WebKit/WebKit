/*
 * Copyright (C) 2012, 2013 Apple Inc. All Rights Reserved.
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

#pragma once

#include "BytecodeGenerator.h"
#include "CachedTypes.h"
#include "ExecutableInfo.h"
#include "JSCInlines.h"
#include "Parser.h"
#include "ParserModes.h"
#include "SourceCodeKey.h"
#include "Strong.h"
#include "StrongInlines.h"
#include "UnlinkedCodeBlock.h"
#include "UnlinkedEvalCodeBlock.h"
#include "UnlinkedFunctionCodeBlock.h"
#include "UnlinkedModuleProgramCodeBlock.h"
#include "UnlinkedProgramCodeBlock.h"
#include <sys/stat.h>
#include <wtf/Forward.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class EvalExecutable;
class IndirectEvalExecutable;
class Identifier;
class DirectEvalExecutable;
class ModuleProgramExecutable;
class ParserError;
class ProgramExecutable;
class SourceCode;
class UnlinkedCodeBlock;
class UnlinkedEvalCodeBlock;
class UnlinkedFunctionExecutable;
class UnlinkedModuleProgramCodeBlock;
class UnlinkedProgramCodeBlock;
class VM;
class VariableEnvironment;

namespace CodeCacheInternal {
static const bool verbose = false;
} // namespace CodeCacheInternal

#define VERBOSE_LOG(...) do { \
    if (CodeCacheInternal::verbose) \
        dataLogLn("(JSC::CodeCache) ", __VA_ARGS__); \
} while (false)

struct SourceCodeValue {
    SourceCodeValue()
    {
    }

    SourceCodeValue(VM& vm, JSCell* cell, int64_t age, bool written = false)
        : cell(vm, cell)
        , age(age)
        , written(written)
    {
    }

    Strong<JSCell> cell;
    int64_t age;
    bool written;
};

class CodeCacheMap {
public:
    typedef HashMap<SourceCodeKey, SourceCodeValue, SourceCodeKey::Hash, SourceCodeKey::HashTraits> MapType;
    typedef MapType::iterator iterator;
    typedef MapType::AddResult AddResult;

    CodeCacheMap()
        : m_size(0)
        , m_sizeAtLastPrune(0)
        , m_timeAtLastPrune(MonotonicTime::now())
        , m_minCapacity(0)
        , m_capacity(0)
        , m_age(0)
    {
    }

    iterator begin() { return m_map.begin(); }
    iterator end() { return m_map.end(); }

    template<typename UnlinkedCodeBlockType>
    UnlinkedCodeBlockType* fetchFromDiskImpl(VM& vm, const SourceCodeKey& key)
    {
        {
            const auto* cachedBytecode = key.source().provider().cachedBytecode();
            if (cachedBytecode && cachedBytecode->size()) {
                VERBOSE_LOG("Found cached CodeBlock in the SourceProvider");
                UnlinkedCodeBlockType* unlinkedCodeBlock = decodeCodeBlock<UnlinkedCodeBlockType>(vm, key, cachedBytecode->data(), cachedBytecode->size());
                if (unlinkedCodeBlock)
                    return unlinkedCodeBlock;
            }
        }

#if OS(DARWIN)
        const char* cachePath = Options::diskCachePath();
        if (!cachePath)
            return nullptr;

        unsigned hash = key.hash();
        char filename[512];
        int count = snprintf(filename, 512, "%s/%u.cache", cachePath, hash);
        if (count < 0 || count > 512)
            return nullptr;

        int fd = open(filename, O_RDONLY);
        if (fd == -1)
            return nullptr;

        int rc = flock(fd, LOCK_SH | LOCK_NB);
        if (rc) {
            close(fd);
            return nullptr;
        }

        struct stat sb;
        int res = fstat(fd, &sb);
        size_t size = static_cast<size_t>(sb.st_size);
        if (res || !size) {
            close(fd);
            return nullptr;
        }

        void* buffer = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
        UnlinkedCodeBlockType* unlinkedCodeBlock = decodeCodeBlock<UnlinkedCodeBlockType>(vm, key, buffer, size);
        munmap(buffer, size);

        if (!unlinkedCodeBlock)
            return nullptr;

        VERBOSE_LOG("Found cached CodeBlock on disk");
        addCache(key, SourceCodeValue(vm, unlinkedCodeBlock, m_age, true));
        return unlinkedCodeBlock;
#else
        UNUSED_PARAM(vm);
        UNUSED_PARAM(key);
        return nullptr;
#endif
    }

    template<typename UnlinkedCodeBlockType>
    std::enable_if_t<std::is_base_of<UnlinkedCodeBlock, UnlinkedCodeBlockType>::value && !std::is_same<UnlinkedCodeBlockType, UnlinkedEvalCodeBlock>::value, UnlinkedCodeBlockType*>
    fetchFromDisk(VM& vm, const SourceCodeKey& key)
    {
        UnlinkedCodeBlockType* codeBlock = fetchFromDiskImpl<UnlinkedCodeBlockType>(vm, key);
        if (UNLIKELY(Options::forceDiskCache()))
            RELEASE_ASSERT(codeBlock);
        return codeBlock;
    }

    template<typename T>
    std::enable_if_t<!std::is_base_of<UnlinkedCodeBlock, T>::value || std::is_same<T, UnlinkedEvalCodeBlock>::value, T*>
    fetchFromDisk(VM&, const SourceCodeKey&) { return nullptr; }

    template<typename UnlinkedCodeBlockType>
    UnlinkedCodeBlockType* findCacheAndUpdateAge(VM& vm, const SourceCodeKey& key)
    {
        prune();

        VERBOSE_LOG("Trying to find cached CodeBlock for ", key.source().provider().url().string());
        iterator findResult = m_map.find(key);
        if (findResult == m_map.end())
            return fetchFromDisk<UnlinkedCodeBlockType>(vm, key);

        int64_t age = m_age - findResult->value.age;
        if (age > m_capacity) {
            // A requested object is older than the cache's capacity. We can
            // infer that requested objects are subject to high eviction probability,
            // so we grow the cache to improve our hit rate.
            m_capacity += recencyBias * oldObjectSamplingMultiplier * key.length();
        } else if (age < m_capacity / 2) {
            // A requested object is much younger than the cache's capacity. We can
            // infer that requested objects are subject to low eviction probability,
            // so we shrink the cache to save memory.
            m_capacity -= recencyBias * key.length();
            if (m_capacity < m_minCapacity)
                m_capacity = m_minCapacity;
        }

        findResult->value.age = m_age;
        m_age += key.length();

        VERBOSE_LOG("Found cached CodeBlock in memory");
        return jsCast<UnlinkedCodeBlockType*>(findResult->value.cell.get());
    }

    AddResult addCache(const SourceCodeKey& key, const SourceCodeValue& value)
    {
        prune();

        AddResult addResult = m_map.add(key, value);
        ASSERT(addResult.isNewEntry);

        m_size += key.length();
        m_age += key.length();
        return addResult;
    }

    void remove(iterator it)
    {
        m_size -= it->key.length();
        m_map.remove(it);
    }

    void clear()
    {
        m_size = 0;
        m_age = 0;
        m_map.clear();
    }

    int64_t age() { return m_age; }

private:
    // This constant factor biases cache capacity toward allowing a minimum
    // working set to enter the cache before it starts evicting.
    static const Seconds workingSetTime;
    static const int64_t workingSetMaxBytes = 16000000;
    static const size_t workingSetMaxEntries = 2000;

    // This constant factor biases cache capacity toward recent activity. We
    // want to adapt to changing workloads.
    static const int64_t recencyBias = 4;

    // This constant factor treats a sampled event for one old object as if it
    // happened for many old objects. Most old objects are evicted before we can
    // sample them, so we need to extrapolate from the ones we do sample.
    static const int64_t oldObjectSamplingMultiplier = 32;

    size_t numberOfEntries() const { return static_cast<size_t>(m_map.size()); }
    bool canPruneQuickly() const { return numberOfEntries() < workingSetMaxEntries; }

    void pruneSlowCase();
    void prune()
    {
        if (m_size <= m_capacity && canPruneQuickly())
            return;

        if (MonotonicTime::now() - m_timeAtLastPrune < workingSetTime
            && m_size - m_sizeAtLastPrune < workingSetMaxBytes
            && canPruneQuickly())
                return;

        pruneSlowCase();
    }

    MapType m_map;
    int64_t m_size;
    int64_t m_sizeAtLastPrune;
    MonotonicTime m_timeAtLastPrune;
    int64_t m_minCapacity;
    int64_t m_capacity;
    int64_t m_age;
};

// Caches top-level code such as <script>, window.eval(), new Function, and JSEvaluateScript().
class CodeCache {
    WTF_MAKE_FAST_ALLOCATED;
public:
    UnlinkedProgramCodeBlock* getUnlinkedProgramCodeBlock(VM&, ProgramExecutable*, const SourceCode&, JSParserStrictMode, DebuggerMode, ParserError&);
    UnlinkedEvalCodeBlock* getUnlinkedEvalCodeBlock(VM&, IndirectEvalExecutable*, const SourceCode&, JSParserStrictMode, DebuggerMode, ParserError&, EvalContextType);
    UnlinkedModuleProgramCodeBlock* getUnlinkedModuleProgramCodeBlock(VM&, ModuleProgramExecutable*, const SourceCode&, DebuggerMode, ParserError&);
    UnlinkedFunctionExecutable* getUnlinkedGlobalFunctionExecutable(VM&, const Identifier&, const SourceCode&, DebuggerMode, Optional<int> functionConstructorParametersEndPosition, ParserError&);

    void clear() { m_sourceCode.clear(); }
    JS_EXPORT_PRIVATE void write(VM&);

private:
    template <class UnlinkedCodeBlockType, class ExecutableType> 
    UnlinkedCodeBlockType* getUnlinkedGlobalCodeBlock(VM&, ExecutableType*, const SourceCode&, JSParserStrictMode, JSParserScriptMode, DebuggerMode, ParserError&, EvalContextType);

    CodeCacheMap m_sourceCode;
};

template <typename T> struct CacheTypes { };

template <> struct CacheTypes<UnlinkedProgramCodeBlock> {
    typedef JSC::ProgramNode RootNode;
    static const SourceCodeType codeType = SourceCodeType::ProgramType;
    static const SourceParseMode parseMode = SourceParseMode::ProgramMode;
};

template <> struct CacheTypes<UnlinkedEvalCodeBlock> {
    typedef JSC::EvalNode RootNode;
    static const SourceCodeType codeType = SourceCodeType::EvalType;
    static const SourceParseMode parseMode = SourceParseMode::ProgramMode;
};

template <> struct CacheTypes<UnlinkedModuleProgramCodeBlock> {
    typedef JSC::ModuleProgramNode RootNode;
    static const SourceCodeType codeType = SourceCodeType::ModuleType;
    static const SourceParseMode parseMode = SourceParseMode::ModuleEvaluateMode;
};

template <class UnlinkedCodeBlockType, class ExecutableType = ScriptExecutable>
UnlinkedCodeBlockType* generateUnlinkedCodeBlockImpl(VM& vm, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, DebuggerMode debuggerMode, ParserError& error, EvalContextType evalContextType, DerivedContextType derivedContextType, bool isArrowFunctionContext, const VariableEnvironment* variablesUnderTDZ, ExecutableType* executable = nullptr)
{
    typedef typename CacheTypes<UnlinkedCodeBlockType>::RootNode RootNode;
    std::unique_ptr<RootNode> rootNode = parse<RootNode>(
        &vm, source, Identifier(), JSParserBuiltinMode::NotBuiltin, strictMode, scriptMode, CacheTypes<UnlinkedCodeBlockType>::parseMode, SuperBinding::NotNeeded, error, nullptr, ConstructorKind::None, derivedContextType, evalContextType);
    if (!rootNode)
        return nullptr;

    unsigned lineCount = rootNode->lastLine() - rootNode->firstLine();
    unsigned startColumn = rootNode->startColumn() + 1;
    bool endColumnIsOnStartLine = !lineCount;
    unsigned unlinkedEndColumn = rootNode->endColumn();
    unsigned endColumn = unlinkedEndColumn + (endColumnIsOnStartLine ? startColumn : 1);
    unsigned arrowContextFeature = isArrowFunctionContext ? ArrowFunctionContextFeature : 0;
    if (executable)
        executable->recordParse(rootNode->features() | arrowContextFeature, rootNode->hasCapturedVariables(), rootNode->lastLine(), endColumn);

    bool usesEval = rootNode->features() & EvalFeature;
    bool isStrictMode = rootNode->features() & StrictModeFeature;
    ExecutableInfo executableInfo(usesEval, isStrictMode, false, false, ConstructorKind::None, scriptMode, SuperBinding::NotNeeded, CacheTypes<UnlinkedCodeBlockType>::parseMode, derivedContextType, isArrowFunctionContext, false, evalContextType);

    UnlinkedCodeBlockType* unlinkedCodeBlock = UnlinkedCodeBlockType::create(&vm, executableInfo, debuggerMode);
    unlinkedCodeBlock->recordParse(rootNode->features(), rootNode->hasCapturedVariables(), lineCount, unlinkedEndColumn);
    if (!source.provider()->sourceURLDirective().isNull())
        unlinkedCodeBlock->setSourceURLDirective(source.provider()->sourceURLDirective());
    if (!source.provider()->sourceMappingURLDirective().isNull())
        unlinkedCodeBlock->setSourceMappingURLDirective(source.provider()->sourceMappingURLDirective());

    error = BytecodeGenerator::generate(vm, rootNode.get(), source, unlinkedCodeBlock, debuggerMode, variablesUnderTDZ);

    if (error.isValid())
        return nullptr;

    return unlinkedCodeBlock;
}

template <class UnlinkedCodeBlockType, class ExecutableType>
UnlinkedCodeBlockType* generateUnlinkedCodeBlock(VM& vm, ExecutableType* executable, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, DebuggerMode debuggerMode, ParserError& error, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    return generateUnlinkedCodeBlockImpl<UnlinkedCodeBlockType, ExecutableType>(vm, source, strictMode, scriptMode, debuggerMode, error, evalContextType, executable->derivedContextType(), executable->isArrowFunctionContext(), variablesUnderTDZ, executable);
}

void generateUnlinkedCodeBlockForFunctions(VM&, UnlinkedCodeBlock*, const SourceCode&, DebuggerMode, ParserError&);

template <class UnlinkedCodeBlockType>
std::enable_if_t<!std::is_same<UnlinkedCodeBlockType, UnlinkedEvalCodeBlock>::value, UnlinkedCodeBlockType*>
recursivelyGenerateUnlinkedCodeBlock(VM& vm, const SourceCode& source, JSParserStrictMode strictMode, JSParserScriptMode scriptMode, DebuggerMode debuggerMode, ParserError& error, EvalContextType evalContextType, const VariableEnvironment* variablesUnderTDZ)
{
    bool isArrowFunctionContext = false;
    UnlinkedCodeBlockType* unlinkedCodeBlock = generateUnlinkedCodeBlockImpl<UnlinkedCodeBlockType>(vm, source, strictMode, scriptMode, debuggerMode, error, evalContextType, DerivedContextType::None, isArrowFunctionContext, variablesUnderTDZ);
    if (!unlinkedCodeBlock)
        return nullptr;

    generateUnlinkedCodeBlockForFunctions(vm, unlinkedCodeBlock, source, debuggerMode, error);
    return unlinkedCodeBlock;
}

void writeCodeBlock(VM&, const SourceCodeKey&, const SourceCodeValue&);
CachedBytecode serializeBytecode(VM&, UnlinkedCodeBlock*, const SourceCode&, SourceCodeType, JSParserStrictMode, JSParserScriptMode, DebuggerMode);

} // namespace JSC
