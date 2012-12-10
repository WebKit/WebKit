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
#include "ProfilerCompilation.h"

#include "JSGlobalObject.h"
#include <wtf/StringPrintStream.h>

namespace JSC { namespace Profiler {

Compilation::Compilation(Bytecodes* bytecodes, CompilationKind kind)
    : m_bytecodes(bytecodes)
    , m_kind(kind)
{
}

Compilation::~Compilation() { }

void Compilation::addDescription(const CompiledBytecode& compiledBytecode)
{
    m_descriptions.append(compiledBytecode);
}

ExecutionCounter* Compilation::executionCounterFor(const OriginStack& origin)
{
    HashMap<OriginStack, OwnPtr<ExecutionCounter> >::iterator iter = m_counters.find(origin);
    if (iter != m_counters.end())
        return iter->value.get();
    
    OwnPtr<ExecutionCounter> counter = adoptPtr(new ExecutionCounter());
    ExecutionCounter* result = counter.get();
    m_counters.add(origin, counter.release());
    return result;
}

void Compilation::addOSRExitSite(const void* codeAddress)
{
    m_osrExitSites.append(OSRExitSite(codeAddress));
}

OSRExit* Compilation::addOSRExit(unsigned id, const OriginStack& originStack, ExitKind exitKind, bool isWatchpoint)
{
    m_osrExits.append(OSRExit(id, originStack, exitKind, isWatchpoint));
    return &m_osrExits.last();
}

JSValue Compilation::toJS(ExecState* exec) const
{
    JSObject* result = constructEmptyObject(exec);
    
    result->putDirect(exec->globalData(), exec->propertyNames().bytecodesID, jsNumber(m_bytecodes->id()));
    result->putDirect(exec->globalData(), exec->propertyNames().compilationKind, jsString(exec, String::fromUTF8(toCString(m_kind))));
    
    JSArray* descriptions = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_descriptions.size(); ++i)
        descriptions->putDirectIndex(exec, i, m_descriptions[i].toJS(exec));
    result->putDirect(exec->globalData(), exec->propertyNames().descriptions, descriptions);
    
    JSArray* counters = constructEmptyArray(exec, 0);
    HashMap<OriginStack, OwnPtr<ExecutionCounter> >::const_iterator end = m_counters.end();
    for (HashMap<OriginStack, OwnPtr<ExecutionCounter> >::const_iterator iter = m_counters.begin(); iter != end; ++iter) {
        JSObject* counterEntry = constructEmptyObject(exec);
        counterEntry->putDirect(exec->globalData(), exec->propertyNames().origin, iter->key.toJS(exec));
        counterEntry->putDirect(exec->globalData(), exec->propertyNames().executionCount, jsNumber(iter->value->count()));
        counters->push(exec, counterEntry);
    }
    result->putDirect(exec->globalData(), exec->propertyNames().counters, counters);
    
    JSArray* exitSites = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_osrExitSites.size(); ++i)
        exitSites->putDirectIndex(exec, i, m_osrExitSites[i].toJS(exec));
    result->putDirect(exec->globalData(), exec->propertyNames().osrExitSites, exitSites);
    
    JSArray* exits = constructEmptyArray(exec, 0);
    for (unsigned i = 0; i < m_osrExits.size(); ++i)
        exits->putDirectIndex(exec, i, m_osrExits[i].toJS(exec));
    result->putDirect(exec->globalData(), exec->propertyNames().osrExits, exits);
    
    return result;
}

} } // namespace JSC::Profiler

