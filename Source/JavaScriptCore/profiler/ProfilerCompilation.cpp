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
#include "ProfilerDumper.h"
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

void Compilation::addOSRExitSite(const Vector<CodePtr<JSInternalPtrTag>>& codeAddresses)
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

Ref<JSON::Value> Compilation::toJSON(Dumper& dumper) const
{
    auto result = JSON::Object::create();
    result->setDouble(dumper.keys().m_bytecodesID, m_bytecodes->id());
    result->setString(dumper.keys().m_compilationKind, String::fromUTF8(toCString(m_kind)));

    auto profiledBytecodes = JSON::Array::create();
    for (const auto& bytecode : m_profiledBytecodes)
        profiledBytecodes->pushValue(bytecode.toJSON(dumper));
    result->setValue(dumper.keys().m_profiledBytecodes, WTFMove(profiledBytecodes));

    auto descriptions = JSON::Array::create();
    for (const auto& description : m_descriptions)
        descriptions->pushValue(description.toJSON(dumper));
    result->setValue(dumper.keys().m_descriptions, WTFMove(descriptions));

    auto counters = JSON::Array::create();
    for (const auto& [key, value] : m_counters) {
        auto counterEntry = JSON::Object::create();
        counterEntry->setValue(dumper.keys().m_origin, key.toJSON(dumper));
        counterEntry->setDouble(dumper.keys().m_executionCount, value->count());
        counters->pushValue(WTFMove(counterEntry));
    }
    result->setValue(dumper.keys().m_counters, WTFMove(counters));

    auto exitSites = JSON::Array::create();
    for (const auto& osrExitSite : m_osrExitSites)
        exitSites->pushValue(osrExitSite.toJSON(dumper));
    result->setValue(dumper.keys().m_osrExitSites, WTFMove(exitSites));

    auto exits = JSON::Array::create();
    for (unsigned i = 0; i < m_osrExits.size(); ++i)
        exits->pushValue(m_osrExits[i].toJSON(dumper));
    result->setValue(dumper.keys().m_osrExits, WTFMove(exits));

    result->setDouble(dumper.keys().m_numInlinedGetByIds, m_numInlinedGetByIds);
    result->setDouble(dumper.keys().m_numInlinedPutByIds, m_numInlinedPutByIds);
    result->setDouble(dumper.keys().m_numInlinedCalls, m_numInlinedCalls);
    result->setString(dumper.keys().m_jettisonReason, String::fromUTF8(toCString(m_jettisonReason)));
    if (!m_additionalJettisonReason.isNull())
        result->setString(dumper.keys().m_additionalJettisonReason, String::fromUTF8(m_additionalJettisonReason));

    result->setValue(dumper.keys().m_uid, m_uid.toJSON(dumper));

    return result;
}

} } // namespace JSC::Profiler

