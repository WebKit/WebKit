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
#include "ProfilerDatabase.h"

#include "CodeBlock.h"
#include "JSONObject.h"
#include "Operations.h"

namespace JSC { namespace Profiler {

Database::Database(JSGlobalData& globalData)
    : m_globalData(globalData)
{
}

Database::~Database()
{
}

Bytecodes* Database::ensureBytecodesFor(CodeBlock* codeBlock)
{
    codeBlock = codeBlock->baselineVersion();
    
    HashMap<CodeBlock*, Bytecodes*>::iterator iter = m_bytecodesMap.find(codeBlock);
    if (iter != m_bytecodesMap.end())
        return iter->value;
    
    m_bytecodes.append(Bytecodes(m_bytecodes.size(), codeBlock));
    Bytecodes* result = &m_bytecodes.last();
    
    m_bytecodesMap.add(codeBlock, result);
    
    return result;
}

void Database::notifyDestruction(CodeBlock* codeBlock)
{
    m_bytecodesMap.remove(codeBlock);
}

PassRefPtr<Compilation> Database::newCompilation(Bytecodes* bytecodes, CompilationKind kind)
{
    RefPtr<Compilation> compilation = adoptRef(new Compilation(bytecodes, kind));
    m_compilations.append(compilation);
    return compilation.release();
}

PassRefPtr<Compilation> Database::newCompilation(CodeBlock* codeBlock, CompilationKind kind)
{
    return newCompilation(ensureBytecodesFor(codeBlock), kind);
}

JSValue Database::toJS(ExecState* exec) const
{
    JSObject* result = constructEmptyObject(exec);
    
    JSArray* bytecodes = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_bytecodes.size(); ++i)
        bytecodes->putDirectIndex(exec, i, m_bytecodes[i].toJS(exec));
    result->putDirect(exec->globalData(), exec->propertyNames().bytecodes, bytecodes);
    
    JSArray* compilations = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_compilations.size(); ++i)
        compilations->putDirectIndex(exec, i, m_compilations[i]->toJS(exec));
    result->putDirect(exec->globalData(), exec->propertyNames().compilations, compilations);
    
    return result;
}

String Database::toJSON() const
{
    JSGlobalObject* globalObject = JSGlobalObject::create(
        m_globalData, JSGlobalObject::createStructure(m_globalData, jsNull()));
    
    return JSONStringify(globalObject->globalExec(), toJS(globalObject->globalExec()), 0);
}

bool Database::save(const char* filename) const
{
    OwnPtr<FilePrintStream> out = FilePrintStream::open(filename, "w");
    if (!out)
        return false;
    
    out->print(toJSON());
    return true;
}

} } // namespace JSC::Profiler

