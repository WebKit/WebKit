/*
 * Copyright (C) 2013-2021 Apple Inc. All rights reserved.
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

#include "JITCompilationMode.h"
#include <wtf/HashMap.h>

namespace JSC {

class CodeBlock;
class CodeBlockSet;

class JITCompilationKey {
public:
    JITCompilationKey()
        : m_profiledBlock(nullptr)
        , m_mode(JITCompilationMode::InvalidCompilation)
    {
    }
    
    JITCompilationKey(WTF::HashTableDeletedValueType)
        : m_profiledBlock(nullptr)
        , m_mode(JITCompilationMode::DFG)
    {
    }
    
    JITCompilationKey(CodeBlock* profiledBlock, JITCompilationMode mode)
        : m_profiledBlock(profiledBlock)
        , m_mode(mode)
    {
    }
    
    bool operator!() const
    {
        return !m_profiledBlock && m_mode == JITCompilationMode::InvalidCompilation;
    }
    
    bool isHashTableDeletedValue() const
    {
        return !m_profiledBlock && m_mode != JITCompilationMode::InvalidCompilation;
    }
    
    CodeBlock* profiledBlock() const { return m_profiledBlock; }
    JITCompilationMode mode() const { return m_mode; }
    
    bool operator==(const JITCompilationKey& other) const
    {
        return m_profiledBlock == other.m_profiledBlock
            && m_mode == other.m_mode;
    }
    
    unsigned hash() const
    {
        return WTF::pairIntHash(WTF::PtrHash<CodeBlock*>::hash(m_profiledBlock), static_cast<std::underlying_type<JITCompilationMode>::type>(m_mode));
    }
    
    void dump(PrintStream&) const;

private:
    CodeBlock* m_profiledBlock;
    JITCompilationMode m_mode;
};

struct JITCompilationKeyHash {
    static unsigned hash(const JITCompilationKey& key) { return key.hash(); }
    static bool equal(const JITCompilationKey& a, const JITCompilationKey& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

} // namespace JSC

namespace WTF {

template<typename T> struct DefaultHash;
template<> struct DefaultHash<JSC::JITCompilationKey> : JSC::JITCompilationKeyHash { };

template<typename T> struct HashTraits;
template<> struct HashTraits<JSC::JITCompilationKey> : SimpleClassHashTraits<JSC::JITCompilationKey> { };

} // namespace WTF
