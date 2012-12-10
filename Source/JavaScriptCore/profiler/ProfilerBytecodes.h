/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef ProfilerBytecodes_h
#define ProfilerBytecodes_h

#include "CodeBlockHash.h"
#include "JSValue.h"
#include "ProfilerBytecode.h"
#include <wtf/PrintStream.h>
#include <wtf/text/WTFString.h>

namespace JSC { namespace Profiler {

class Bytecodes {
public:
    Bytecodes(size_t id, const String& inferredName, const String& sourceCode, CodeBlockHash);
    ~Bytecodes();
    
    void append(const Bytecode& bytecode) { m_bytecode.append(bytecode); }
    
    size_t id() const { return m_id; }
    const String& inferredName() const { return m_inferredName; }
    const String& sourceCode() const { return m_sourceCode; }
    CodeBlockHash hash() const { return m_hash; }
    
    // Note that this data structure is not indexed by bytecode index.
    unsigned size() const { return m_bytecode.size(); }
    const Bytecode& at(unsigned i) const { return m_bytecode[i]; }
    
    unsigned indexForBytecodeIndex(unsigned bytecodeIndex) const;
    const Bytecode& forBytecodeIndex(unsigned bytecodeIndex) const;
    
    void dump(PrintStream&) const;
    
    JSValue toJS(ExecState*) const;
    
private:
    size_t m_id;
    String m_inferredName;
    String m_sourceCode;
    CodeBlockHash m_hash;
    Vector<Bytecode> m_bytecode;
};

} } // namespace JSC::Profiler

#endif // ProfilerBytecodes_h

