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

bool ArrayBufferViewWatchpointAdaptor::add(CodeBlock* codeBlock, JSArrayBufferView* view, WatchpointCollector& collector)
{
    return collector.addWatchpoint([&](CodeBlockJettisoningWatchpoint& watchpoint) {
        if (hasBeenInvalidated(view, collector.concurrency()))
            return false;

        // view is already frozen. If it is deallocated, jettisoning happens.
        watchpoint.initialize(codeBlock);
        ArrayBuffer* arrayBuffer = view->possiblySharedBuffer();
        if (!arrayBuffer) {
            watchpoint.fire(codeBlock->vm(), StringFireDetail("ArrayBuffer could not be allocated, probably because of OOM."));
            return true;
        }

        // FIXME: We don't need to set this watchpoint at all for shared buffers.
        // https://bugs.webkit.org/show_bug.cgi?id=164108
        arrayBuffer->detachingWatchpointSet().add(&watchpoint);
        return true;
    });
}

bool SymbolTableAdaptor::add(CodeBlock* codeBlock, SymbolTable* symbolTable, WatchpointCollector& collector)
{
    return collector.addWatchpoint([&](CodeBlockJettisoningWatchpoint& watchpoint) {
        if (hasBeenInvalidated(symbolTable, collector.concurrency()))
            return false;

        // symbolTable is already frozen strongly.
        watchpoint.initialize(codeBlock);
        symbolTable->singleton().add(&watchpoint);
        return true;
    });
}

bool FunctionExecutableAdaptor::add(CodeBlock* codeBlock, FunctionExecutable* executable, WatchpointCollector& collector)
{
    return collector.addWatchpoint([&](CodeBlockJettisoningWatchpoint& watchpoint) {
        if (hasBeenInvalidated(executable, collector.concurrency()))
            return false;

        // executable is already frozen strongly.
        watchpoint.initialize(codeBlock);
        executable->singleton().add(&watchpoint);

        return true;
    });
}

bool AdaptiveStructureWatchpointAdaptor::add(CodeBlock* codeBlock, const ObjectPropertyCondition& key, WatchpointCollector& collector)
{
    VM& vm = codeBlock->vm();
    switch (key.kind()) {
    case PropertyCondition::Equivalence: {
        return collector.addAdaptiveInferredPropertyValueWatchpoint([&](AdaptiveInferredPropertyValueWatchpoint& watchpoint) {
            if (hasBeenInvalidated(key, collector.concurrency()))
                return false;

            watchpoint.initialize(key, codeBlock);
            watchpoint.install(vm);
            return true;
        });
    }
    default: {
        return collector.addAdaptiveStructureWatchpoint([&](AdaptiveStructureWatchpoint& watchpoint) {
            if (hasBeenInvalidated(key, collector.concurrency()))
                return false;

            watchpoint.initialize(key, codeBlock);
            watchpoint.install(vm);
            return true;
        });
    }
    }
    return true;
}

DesiredWatchpoints::DesiredWatchpoints() = default;
DesiredWatchpoints::~DesiredWatchpoints() = default;

void DesiredWatchpoints::addLazily(WatchpointSet& set)
{
    m_sets.addLazily(&set);
}

void DesiredWatchpoints::addLazily(InlineWatchpointSet& set)
{
    m_inlineSets.addLazily(&set);
}

void DesiredWatchpoints::addLazily(Graph& graph, SymbolTable* symbolTable)
{
    graph.freezeStrong(symbolTable); // Keep this strongly.
    m_symbolTables.addLazily(symbolTable);
}

void DesiredWatchpoints::addLazily(Graph& graph, FunctionExecutable* executable)
{
    graph.freezeStrong(executable); // Keep this strongly.
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

void DesiredWatchpoints::addLazily(DesiredGlobalProperty&& property)
{
    m_globalProperties.addLazily(WTFMove(property));
}

bool DesiredWatchpoints::consider(Structure* structure)
{
    if (!structure->dfgShouldWatch())
        return false;
    addLazily(structure->transitionWatchpointSet());
    return true;
}

void DesiredWatchpoints::countWatchpoints(CodeBlock* codeBlock, DesiredIdentifiers& identifiers, CommonData* commonData)
{
    WatchpointCollector collector(*commonData);

    m_sets.reallyAdd(codeBlock, collector);
    m_inlineSets.reallyAdd(codeBlock, collector);
    m_symbolTables.reallyAdd(codeBlock, collector);
    m_functionExecutables.reallyAdd(codeBlock, collector);
    m_bufferViews.reallyAdd(codeBlock, collector);
    m_adaptiveStructureSets.reallyAdd(codeBlock, collector);
    m_globalProperties.reallyAdd(codeBlock, identifiers, collector);

    auto counts = collector.counts();
    commonData->m_watchpoints = FixedVector<CodeBlockJettisoningWatchpoint>(counts.m_watchpointCount);
    commonData->m_adaptiveStructureWatchpoints = FixedVector<AdaptiveStructureWatchpoint>(counts.m_adaptiveStructureWatchpointCount);
    commonData->m_adaptiveInferredPropertyValueWatchpoints = FixedVector<AdaptiveInferredPropertyValueWatchpoint>(counts.m_adaptiveInferredPropertyValueWatchpointCount);
}

bool DesiredWatchpoints::reallyAdd(CodeBlock* codeBlock, DesiredIdentifiers& identifiers, CommonData* commonData)
{
    WatchpointCollector collector(*commonData);
    collector.materialize();

    if (!m_sets.reallyAdd(codeBlock, collector))
        return false;

    if (!m_inlineSets.reallyAdd(codeBlock, collector))
        return false;

    if (!m_symbolTables.reallyAdd(codeBlock, collector))
        return false;

    if (!m_functionExecutables.reallyAdd(codeBlock, collector))
        return false;

    if (!m_bufferViews.reallyAdd(codeBlock, collector))
        return false;

    if (!m_adaptiveStructureSets.reallyAdd(codeBlock, collector))
        return false;

    if (!m_globalProperties.reallyAdd(codeBlock, identifiers, collector))
        return false;

    collector.finalize(*commonData);

    return true;
}

bool DesiredWatchpoints::areStillValidOnMainThread(VM& vm, DesiredIdentifiers& identifiers)
{
    return m_globalProperties.isStillValidOnMainThread(vm, identifiers);
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

