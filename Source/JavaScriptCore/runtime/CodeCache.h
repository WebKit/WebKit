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

class SourceCodeKey {
public:
    enum CodeType { EvalType, ProgramType, FunctionType };

    SourceCodeKey()
    {
    }

    SourceCodeKey(const SourceCode& sourceCode, const String& name, CodeType codeType, JSParserStrictness jsParserStrictness)
        : m_sourceCode(sourceCode)
        , m_name(name)
        , m_flags((codeType << 1) | jsParserStrictness)
        , m_hash(string().impl()->hash())
    {
    }

    SourceCodeKey(WTF::HashTableDeletedValueType)
        : m_name(WTF::HashTableDeletedValue)
    {
    }

    bool isHashTableDeletedValue() const { return m_name.isHashTableDeletedValue(); }

    unsigned hash() const { return m_hash; }

    size_t length() const { return m_sourceCode.length(); }

    bool isNull() const { return m_sourceCode.isNull(); }

    // To save memory, we compute our string on demand. It's expected that source
    // providers cache their strings to make this efficient.
    String string() const { return m_sourceCode.toString(); }

    bool operator==(const SourceCodeKey& other) const
    {
        return m_hash == other.m_hash
            && length() == other.length()
            && m_flags == other.m_flags
            && m_name == other.m_name
            && string() == other.string();
    }

private:
    SourceCode m_sourceCode;
    String m_name;
    unsigned m_flags;
    unsigned m_hash;
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

class CodeCacheMap {
    typedef HashMap<SourceCodeKey, Strong<JSCell>, SourceCodeKeyHash, SourceCodeKeyHashTraits> MapType;
    typedef MapType::iterator iterator;

public:
    CodeCacheMap(size_t capacity)
        : m_size(0)
        , m_capacity(capacity)
    {
    }

    const Strong<JSCell>* find(const SourceCodeKey& key)
    {
        iterator result = m_map.find(key);
        if (result == m_map.end())
            return 0;
        return &result->value;
    }

    void set(const SourceCodeKey& key, const Strong<JSCell>& value)
    {
        while (m_size >= m_capacity) {
            MapType::iterator it = m_map.begin();
            m_size -= it->key.length();
            m_map.remove(it);
        }

        m_size += key.length();
        m_map.set(key, value);
    }

    void clear()
    {
        m_size = 0;
        m_map.clear();
    }

private:
    MapType m_map;
    size_t m_size;
    size_t m_capacity;
};

// Caches top-level code such as <script>, eval(), new Function, and JSEvaluateScript().
class CodeCache {
public:
    static PassOwnPtr<CodeCache> create() { return adoptPtr(new CodeCache); }

    UnlinkedProgramCodeBlock* getProgramCodeBlock(JSGlobalData&, ProgramExecutable*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedEvalCodeBlock* getEvalCodeBlock(JSGlobalData&, EvalExecutable*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);
    UnlinkedFunctionExecutable* getFunctionExecutableFromGlobalCode(JSGlobalData&, const Identifier&, const SourceCode&, ParserError&);
    ~CodeCache();

    void clear()
    {
        m_sourceCode.clear();
    }

private:
    CodeCache();

    template <class UnlinkedCodeBlockType, class ExecutableType> 
    UnlinkedCodeBlockType* getCodeBlock(JSGlobalData&, ExecutableType*, const SourceCode&, JSParserStrictness, DebuggerMode, ProfilerMode, ParserError&);

    enum { CacheSize = 16000000 }; // Size in characters

    CodeCacheMap m_sourceCode;
};

}

#endif // CodeCache_h
