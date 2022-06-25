/*
 * Copyright (C) 2012-2019 Apple Inc. All rights reserved.
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

#include "JSCInlines.h"
#include "ObjectConstructor.h"
#include "ProfilerDatabase.h"
#include <wtf/StringPrintStream.h>

namespace JSC { namespace Profiler {

Compilation::Compilation(Bytecodes* bytecodes, CompilationKind kind)
    : m_kind(kind)
    , m_bytecodes(bytecodes)
    , m_numInlinedGetByIds(0)
    , m_numInlinedPutByIds(0)
    , m_numInlinedCalls(0)
    , m_jettisonReason(NotJettisoned)
    , m_uid(UID::create())
{
}

Compilation::~Compilation() { }

void Compilation::addProfiledBytecodes(Database& database, CodeBlock* profiledBlock)
{
    Bytecodes* bytecodes = database.ensureBytecodesFor(profiledBlock);
    
    // First make sure that we haven't already added profiled bytecodes for this code
    // block. We do this using an O(N) search because I suspect that this list will
    // tend to be fairly small, and the additional space costs of having a HashMap/Set
    // would be greater than the time cost of occasionally doing this search.
    
    for (unsigned i = m_profiledBytecodes.size(); i--;) {
        if (m_profiledBytecodes[i].bytecodes() == bytecodes)
            return;
    }
    
    m_profiledBytecodes.append(ProfiledBytecodes(bytecodes, profiledBlock));
}

void Compilation::addDescription(const CompiledBytecode& compiledBytecode)
{
    m_descriptions.append(compiledBytecode);
}

void Compilation::addDescription(const OriginStack& stack, const CString& description)
{
    addDescription(CompiledBytecode(stack, description));
}

ExecutionCounter* Compilation::executionCounterFor(const OriginStack& origin)
{
    std::unique_ptr<ExecutionCounter>& counter = m_counters.add(origin, nullptr).iterator->value;
    if (!counter)
        counter = makeUnique<ExecutionCounter>();

    return counter.get();
}

void Compilation::addOSRExitSite(const Vector<MacroAssemblerCodePtr<JSInternalPtrTag>>& codeAddresses)
{
    m_osrExitSites.append(OSRExitSite(codeAddresses));
}

OSRExit* Compilation::addOSRExit(unsigned id, const OriginStack& originStack, ExitKind exitKind, bool isWatchpoint)
{
    m_osrExits.append(OSRExit(id, originStack, exitKind, isWatchpoint));
    return &m_osrExits.last();
}

void Compilation::setJettisonReason(JettisonReason jettisonReason, const FireDetail* detail)
{
    if (m_jettisonReason != NotJettisoned)
        return; // We only care about the original jettison reason.
    
    m_jettisonReason = jettisonReason;
    if (detail)
        m_additionalJettisonReason = toCString(*detail);
    else
        m_additionalJettisonReason = CString();
}

void Compilation::dump(PrintStream& out) const
{
    out.print("Comp", m_uid);
}

JSValue Compilation::toJS(JSGlobalObject* globalObject) const
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    JSObject* result = constructEmptyObject(globalObject);
    RETURN_IF_EXCEPTION(scope, { });
    result->putDirect(vm, vm.propertyNames->bytecodesID, jsNumber(m_bytecodes->id()));
    result->putDirect(vm, vm.propertyNames->compilationKind, jsString(vm, String::fromUTF8(toCString(m_kind))));
    
    JSArray* profiledBytecodes = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });
    for (unsigned i = 0; i < m_profiledBytecodes.size(); ++i) {
        auto value = m_profiledBytecodes[i].toJS(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        profiledBytecodes->putDirectIndex(globalObject, i, value);
        RETURN_IF_EXCEPTION(scope, { });
    }
    result->putDirect(vm, vm.propertyNames->profiledBytecodes, profiledBytecodes);
    
    JSArray* descriptions = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });
    for (unsigned i = 0; i < m_descriptions.size(); ++i) {
        auto value = m_descriptions[i].toJS(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        descriptions->putDirectIndex(globalObject, i, value);
        RETURN_IF_EXCEPTION(scope, { });
    }
    result->putDirect(vm, vm.propertyNames->descriptions, descriptions);
    
    JSArray* counters = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });
    for (auto it = m_counters.begin(), end = m_counters.end(); it != end; ++it) {
        JSObject* counterEntry = constructEmptyObject(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        auto value = it->key.toJS(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        counterEntry->putDirect(vm, vm.propertyNames->origin, value);
        counterEntry->putDirect(vm, vm.propertyNames->executionCount, jsNumber(it->value->count()));
        counters->push(globalObject, counterEntry);
        RETURN_IF_EXCEPTION(scope, { });
    }
    result->putDirect(vm, vm.propertyNames->counters, counters);
    
    JSArray* exitSites = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });
    for (unsigned i = 0; i < m_osrExitSites.size(); ++i) {
        auto value = m_osrExitSites[i].toJS(globalObject);
        RETURN_IF_EXCEPTION(scope, { });
        exitSites->putDirectIndex(globalObject, i, value);
        RETURN_IF_EXCEPTION(scope, { });
    }
    result->putDirect(vm, vm.propertyNames->osrExitSites, exitSites);
    
    JSArray* exits = constructEmptyArray(globalObject, nullptr);
    RETURN_IF_EXCEPTION(scope, { });
    for (unsigned i = 0; i < m_osrExits.size(); ++i) {
        exits->putDirectIndex(globalObject, i, m_osrExits[i].toJS(globalObject));
        RETURN_IF_EXCEPTION(scope, { });
    }
    result->putDirect(vm, vm.propertyNames->osrExits, exits);
    
    result->putDirect(vm, vm.propertyNames->numInlinedGetByIds, jsNumber(m_numInlinedGetByIds));
    result->putDirect(vm, vm.propertyNames->numInlinedPutByIds, jsNumber(m_numInlinedPutByIds));
    result->putDirect(vm, vm.propertyNames->numInlinedCalls, jsNumber(m_numInlinedCalls));
    result->putDirect(vm, vm.propertyNames->jettisonReason, jsString(vm, String::fromUTF8(toCString(m_jettisonReason))));
    if (!m_additionalJettisonReason.isNull())
        result->putDirect(vm, vm.propertyNames->additionalJettisonReason, jsString(vm, String::fromUTF8(m_additionalJettisonReason)));
    
    result->putDirect(vm, vm.propertyNames->uid, m_uid.toJS(globalObject));
    
    return result;
}

} } // namespace JSC::Profiler

