/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
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

#ifndef CodeCache_h
#define CodeCache_h

#include "CodeSpecializationKind.h"
#include "ParserModes.h"
#include "SourceCode.h"
#include "Strong.h"
#include "WeakRandom.h"
#include <wtf/FixedArray.h>
#include <wtf/Forward.h>
#include <wtf/PassOwnPtr.h>
#include <wtf/RandomNumber.h>
#include <wtf/text/WTFString.h>

namespace JSC {

class EvalExecutable;
class FunctionBodyNode;
class Identifier;
class ProgramExecutable;
class UnlinkedCodeBlock;
class UnlinkedEvalCodeBlock;
class UnlinkedFunctionCodeBlock;
class UnlinkedFunctionExecutable;
class UnlinkedProgramCodeBlock;
class JSGlobalData;
struct ParserError;
class SourceCode;
class SourceProvider;

template <int CacheSize, typename KeyType, typename EntryType, typename HashArg = typename DefaultHash<KeyType>::Hash, typename KeyTraitsArg = HashTraits<KeyType> >
class CacheMap {
    typedef HashMap<KeyType, EntryType, HashArg, KeyTraitsArg> MapType;
    typedef typename MapType::iterator iterator;

public:
    const EntryType* find(const KeyType& key)
    {
        iterator result = m_map.find(key);
        if (result == m_map.end())
            return 0;
        return &result->value;
    }

    void set(const KeyType& key, const EntryType& value)
    {
        if (m_map.size() >= CacheSize)
            m_map.remove(m_map.begin());

        m_map.set(key, value);
    }

    void clear() { m_map.clear(); }

private:
    MapType m_map;
};

class SourceCodeKey {
public:
    enum CodeType { EvalType, ProgramType, FunctionCallType, FunctionConstructType };

    SourceCodeKey()
        : m_flags(0)
    {
    }

    SourceCodeKey(const SourceCode& sourceCode, const String& name, CodeType codeType, JSParserStrictness jsParserStrictness)
        : m_sourceString(sourceCode.toString())
        , m_name(name)
        , m_flags((codeType << 1) | jsParserStrictness)
    {
    }

    SourceCodeKey(WTF::HashTableDeletedValueType)
        : m_sourceString(WTF::HashTableDeletedValue)
    {
    }

    bool isHashTableDeletedValue() const { return m_sourceString.isHashTableDeletedValue(); }

    unsigned hash() const { return m_sourceString.impl()->hash(); }

    bool isNull() const { return m_sourceString.isNull(); }

    bool operator==(const SourceCodeKey& other) const
    {
        return m_flags == other.m_flags
            && m_name == other.m_name
            && m_sourceString == other.m_sourceString;
    }

private:
    String m_sourceString;
    String m_name;
    unsigned m_flags;
};

struct SourceCodeKeyHash {
    static unsigned hash(const SourceCodeKey& key) { return key.hash(); }
    static bool equal(const SourceCodeKey& a, const SourceCodeKey& b) { return a == b; }
    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct SourceCodeKeyHashTraits : SimpleClassHashTraits<SourceCodeKey> {
    static const bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const SourceCodeKey& sourceCodeKey) { return sourceCodeKey.isNull(); }
};

class CodeCache {
public:
    static PassOwnPtr<CodeCache> create() { return adoptPtr(new CodeCache); }

    UnlinkedProgramCodeBlock* getProgramCodeBlock(JSGlobalData&, ProgramExecutable*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedEvalCodeBlock* getEvalCodeBlock(JSGlobalData&, EvalExecutable*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedFunctionCodeBlock* getFunctionCodeBlock(JSGlobalData&, UnlinkedFunctionExecutable*, const SourceCode&, CodeSpecializationKind, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedFunctionExecutable* getFunctionExecutableFromGlobalCode(JSGlobalData&, const Identifier&, const SourceCode&, ParserError&);
    void usedFunctionCode(JSGlobalData&, UnlinkedFunctionCodeBlock*);
    ~CodeCache();

    void clear()
    {
        m_sourceCode.clear();
        m_recentlyUsedFunctions.clear();
    }

private:
    CodeCache();

    UnlinkedFunctionCodeBlock* generateFunctionCodeBlock(JSGlobalData&, UnlinkedFunctionExecutable*, const SourceCode&, CodeSpecializationKind, DebuggerMode, ProfilerMode, ParserError&);

    template <class UnlinkedCodeBlockType, class ExecutableType> inline UnlinkedCodeBlockType* getCodeBlock(JSGlobalData&, ExecutableType*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);

    enum {
        MaxRootEntries = 1280, // Top-level code such as <script>, eval(), JSEvaluateScript(), etc.
        MaxChildFunctionEntries = MaxRootEntries * 8 // Sampling shows that each root holds about 6 functions. 8 is enough to usually cache all the child functions for each top-level entry.
    };

    CacheMap<MaxRootEntries, SourceCodeKey, Strong<JSCell>, SourceCodeKeyHash, SourceCodeKeyHashTraits> m_sourceCode;
    CacheMap<MaxChildFunctionEntries, UnlinkedFunctionCodeBlock*, Strong<UnlinkedFunctionCodeBlock> > m_recentlyUsedFunctions;
};

}

#endif // CodeCache_h
