/*
 * Copyright (C) 2012 Apple Inc. All Rights Reserved.
 * Copyright (C) 2015 Yusuke Suzuki <utatane.tea@gmail.com>
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

#ifndef SourceCodeKey_h
#define SourceCodeKey_h

#include "ParserModes.h"
#include "SourceCode.h"
#include <wtf/HashTraits.h>

namespace JSC {

enum class SourceCodeType { EvalType, ProgramType, FunctionType, ModuleType };

class SourceCodeFlags {
public:
    SourceCodeFlags() = default;

    SourceCodeFlags(SourceCodeType codeType, JSParserBuiltinMode builtinMode, JSParserStrictMode strictMode, JSParserCommentMode commentMode, DerivedContextType derivedContextType, EvalContextType evalContextType, bool isArrowFunctionContext)
        : m_flags(
            (static_cast<unsigned>(commentMode) << 8) |
            (static_cast<unsigned>(isArrowFunctionContext) << 7) |
            (static_cast<unsigned>(evalContextType) << 6) |
            (static_cast<unsigned>(derivedContextType) << 4) |
            (static_cast<unsigned>(codeType) << 2) |
            (static_cast<unsigned>(builtinMode) << 1) |
            (static_cast<unsigned>(strictMode))
        )
    {
    }

    inline bool operator==(const SourceCodeFlags& rhs) const
    {
        return m_flags == rhs.m_flags;
    }

private:
    unsigned m_flags { 0 };
};

class SourceCodeKey {
public:
    SourceCodeKey()
    {
    }

    SourceCodeKey(const SourceCode& sourceCode, const String& name, SourceCodeType codeType, JSParserBuiltinMode builtinMode, JSParserStrictMode strictMode, JSParserCommentMode commentMode, DerivedContextType derivedContextType, EvalContextType evalContextType, bool isArrowFunctionContext)
        : m_sourceCode(sourceCode)
        , m_name(name)
        , m_flags(codeType, builtinMode, strictMode, commentMode, derivedContextType, evalContextType, isArrowFunctionContext)
        , m_hash(sourceCode.hash())
    {
    }

    SourceCodeKey(WTF::HashTableDeletedValueType)
        : m_sourceCode(WTF::HashTableDeletedValue)
    {
    }

    bool isHashTableDeletedValue() const { return m_sourceCode.isHashTableDeletedValue(); }

    unsigned hash() const { return m_hash; }

    size_t length() const { return m_sourceCode.length(); }

    bool isNull() const { return m_sourceCode.isNull(); }

    // To save memory, we compute our string on demand. It's expected that source
    // providers cache their strings to make this efficient.
    StringView string() const { return m_sourceCode.view(); }

    bool operator==(const SourceCodeKey& other) const
    {
        return m_hash == other.m_hash
            && length() == other.length()
            && m_flags == other.m_flags
            && m_name == other.m_name
            && string() == other.string();
    }

    struct Hash {
        static unsigned hash(const SourceCodeKey& key) { return key.hash(); }
        static bool equal(const SourceCodeKey& a, const SourceCodeKey& b) { return a == b; }
        static const bool safeToCompareToEmptyOrDeleted = false;
    };

    struct HashTraits : SimpleClassHashTraits<SourceCodeKey> {
        static const bool hasIsEmptyValueFunction = true;
        static bool isEmptyValue(const SourceCodeKey& key) { return key.isNull(); }
    };

private:
    SourceCode m_sourceCode;
    String m_name;
    SourceCodeFlags m_flags;
    unsigned m_hash;
};

}

#endif // SourceCodeKey_h
