/*
 * Copyright (C) 2013-2019 Apple Inc. All rights reserved.
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
#include "DFGDesiredWatchpoints.h"

#if ENABLE(DFG_JIT)

#include "CodeBlock.h"
#include "DFGGraph.h"
#include "JSCInlines.h"

namespace JSC { namespace DFG {

void ArrayBufferViewWatchpointAdaptor::add(
    CodeBlock* codeBlock, JSArrayBufferView* view, CommonData& common)
{
    // view is already frozen. If it is deallocated, jettisoning happens.
    CodeBlockJettisoningWatchpoint* watchpoint = nullptr;
    {
        ConcurrentJSLocker locker(codeBlock->m_lock);
        watchpoint = common.watchpoints.add(codeBlock);
    }
    ArrayBuffer* arrayBuffer = view->possiblySharedBuffer();
    RELEASE_ASSERT(arrayBuffer);
    // FIXME: We don't need to set this watchpoint at all for shared buffers.
    // https://bugs.webkit.org/show_bug.cgi?id=164108
    arrayBuffer->neuteringWatchpointSet().add(WTFMove(watchpoint));
}

void SymbolTableAdaptor::add(
    CodeBlock* codeBlock, SymbolTable* symbolTable, CommonData& common)
{
    codeBlock->addConstant(ConcurrentJSLocker(codeBlock->m_lock), symbolTable); // For common users, it doesn't really matter if it's weak or not. If references to it go away, we go away, too.
    CodeBlockJettisoningWatchpoint* watchpoint = nullptr;
    {
        ConcurrentJSLocker locker(codeBlock->m_lock);
        watchpoint = common.watchpoints.add(codeBlock);
    }
    symbolTable->singleton().add(WTFMove(watchpoint));
}

void FunctionExecutableAdaptor::add(
    CodeBlock* codeBlock, FunctionExecutable* executable, CommonData& common)
{
    codeBlock->addConstant(ConcurrentJSLocker(codeBlock->m_lock), executable); // For common users, it doesn't really matter if it's weak or not. If references to it go away, we go away, too.
    CodeBlockJettisoningWatchpoint* watchpoint = nullptr;
    {
        ConcurrentJSLocker locker(codeBlock->m_lock);
        watchpoint = common.watchpoints.add(codeBlock);
    }
    executable->singleton().add(WTFMove(watchpoint));
}

void AdaptiveStructureWatchpointAdaptor::add(
    CodeBlock* codeBlock, const ObjectPropertyCondition& key, CommonData& common)
{
    VM& vm = codeBlock->vm();
    switch (key.kind()) {
    case PropertyCondition::Equivalence: {
        AdaptiveInferredPropertyValueWatchpoint* watchpoint = nullptr;
        {
            ConcurrentJSLocker locker(codeBlock->m_lock);
            watchpoint = common.adaptiveInferredPropertyValueWatchpoints.add(key, codeBlock);
        }
        watchpoint->install(vm);
        break;
    }
    default: {
        AdaptiveStructureWatchpoint* watchpoint = nullptr;
        {
            ConcurrentJSLocker locker(codeBlock->m_lock);
            watchpoint = common.adaptiveStructureWatchpoints.add(key, codeBlock);
        }
        watchpoint->install(vm);
        break;
    }
    }
}

DesiredWatchpoints::DesiredWatchpoints() { }
DesiredWatchpoints::~DesiredWatchpoints() { }

void DesiredWatchpoints::addLazily(WatchpointSet* set)
{
    m_sets.addLazily(set);
}

void DesiredWatchpoints::addLazily(InlineWatchpointSet& set)
{
    m_inlineSets.addLazily(&set);
}

void DesiredWatchpoints::addLazily(SymbolTable* symbolTable)
{
    m_symbolTables.addLazily(symbolTable);
}

void DesiredWatchpoints::addLazily(FunctionExecutable* executable)
{
    m_functionExecutables.addLazily(executable);
}

void DesiredWatchpoints::addLazily(JSArrayBufferView* view)
{
    m_bufferViews.addLazily(view);
}

void DesiredWatchpoints::addLazily(const ObjectPropertyCondition& key)
{
    m_adaptiveStructureSets.addLazily(key);
}

bool DesiredWatchpoints::consider(Structure* structure)
{
    if (!structure->dfgShouldWatch())
        return false;
    addLazily(structure->transitionWatchpointSet());
    return true;
}

void DesiredWatchpoints::reallyAdd(CodeBlock* codeBlock, CommonData& commonData)
{
    m_sets.reallyAdd(codeBlock, commonData);
    m_inlineSets.reallyAdd(codeBlock, commonData);
    m_symbolTables.reallyAdd(codeBlock, commonData);
    m_functionExecutables.reallyAdd(codeBlock, commonData);
    m_bufferViews.reallyAdd(codeBlock, commonData);
    m_adaptiveStructureSets.reallyAdd(codeBlock, commonData);
}

bool DesiredWatchpoints::areStillValid() const
{
    return m_sets.areStillValid()
        && m_inlineSets.areStillValid()
        && m_symbolTables.areStillValid()
        && m_functionExecutables.areStillValid()
        && m_bufferViews.areStillValid()
        && m_adaptiveStructureSets.areStillValid();
}

void DesiredWatchpoints::dumpInContext(PrintStream& out, DumpContext* context) const
{
    Prefix noPrefix(Prefix::NoHeader);
    Prefix& prefix = context && context->graph ? context->graph->prefix() : noPrefix;
    out.print(prefix, "Desired watchpoints:\n");
    out.print(prefix, "    Watchpoint sets: ", inContext(m_sets, context), "\n");
    out.print(prefix, "    Inline watchpoint sets: ", inContext(m_inlineSets, context), "\n");
    out.print(prefix, "    SymbolTables: ", inContext(m_symbolTables, context), "\n");
    out.print(prefix, "    FunctionExecutables: ", inContext(m_functionExecutables, context), "\n");
    out.print(prefix, "    Buffer views: ", inContext(m_bufferViews, context), "\n");
    out.print(prefix, "    Object property conditions: ", inContext(m_adaptiveStructureSets, context), "\n");
}

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

