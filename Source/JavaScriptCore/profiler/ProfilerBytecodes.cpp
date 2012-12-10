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

#include "config.h"
#include "ProfilerBytecodes.h"

#include "JSGlobalObject.h"
#include <wtf/StringPrintStream.h>

namespace JSC { namespace Profiler {

Bytecodes::Bytecodes(
    size_t id, const String& inferredName, const String& sourceCode, CodeBlockHash hash)
    : m_id(id)
    , m_inferredName(inferredName)
    , m_sourceCode(sourceCode)
    , m_hash(hash)
{
}

Bytecodes::~Bytecodes() { }

unsigned Bytecodes::indexForBytecodeIndex(unsigned bytecodeIndex) const
{
    return binarySearch<Bytecode, unsigned, getBytecodeIndexForBytecode>(const_cast<Bytecode*>(m_bytecode.begin()), m_bytecode.size(), bytecodeIndex) - m_bytecode.begin();
}

const Bytecode& Bytecodes::forBytecodeIndex(unsigned bytecodeIndex) const
{
    return at(indexForBytecodeIndex(bytecodeIndex));
}

void Bytecodes::dump(PrintStream& out) const
{
    out.print("#", m_hash, "(", m_id, ")");
}

JSValue Bytecodes::toJS(ExecState* exec) const
{
    JSObject* result = constructEmptyObject(exec);
    
    result->putDirect(exec->globalData(), exec->propertyNames().bytecodesID, jsNumber(m_id));
    result->putDirect(exec->globalData(), exec->propertyNames().inferredName, jsString(exec, m_inferredName));
    result->putDirect(exec->globalData(), exec->propertyNames().sourceCode, jsString(exec, m_sourceCode));
    result->putDirect(exec->globalData(), exec->propertyNames().hash, jsString(exec, String::fromUTF8(toCString(m_hash))));
    
    JSArray* stream = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_bytecode.size(); ++i)
        stream->putDirectIndex(exec, i, m_bytecode[i].toJS(exec));
    result->putDirect(exec->globalData(), exec->propertyNames().bytecode, stream);
    
    return result;
}

} } // namespace JSC::Profiler

