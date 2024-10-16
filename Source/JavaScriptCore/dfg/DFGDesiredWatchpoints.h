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

#pragma once

#if ENABLE(DFG_JIT)

#include "DFGCommonData.h"
#include "DFGDesiredGlobalProperties.h"
#include "FunctionExecutable.h"
#include "JSArrayBufferView.h"
#include "ObjectPropertyCondition.h"
#include "SymbolTable.h"
#include "Watchpoint.h"
#include <wtf/CommaPrinter.h>
#include <wtf/HashSet.h>

namespace JSC { namespace DFG {

class Graph;
struct Prefix;

enum class WatchpointRegistrationMode : uint8_t { Collect, Add };
struct DesiredWatchpointCounts {
    unsigned m_watchpointCount { 0 };
    unsigned m_adaptiveStructureWatchpointCount { 0 };
    unsigned m_adaptiveInferredPropertyValueWatchpointCount { 0 };
};

class WatchpointCollector final {
    WTF_MAKE_NONCOPYABLE(WatchpointCollector);
public:
    WatchpointCollector(CommonData& commonData)
        : m_watchpoints(WTFMove(commonData.m_watchpoints))
        , m_adaptiveStructureWatchpoints(WTFMove(commonData.m_adaptiveStructureWatchpoints))
        , m_adaptiveInferredPropertyValueWatchpoints(WTFMove(commonData.m_adaptiveInferredPropertyValueWatchpoints))
    { }

    DesiredWatchpointCounts counts() { return m_counts; }

    void materialize()
    {
        m_mode = WatchpointRegistrationMode::Add;
    }

    void finalize(CommonData& commonData)
    {
        commonData.m_watchpoints = WTFMove(m_watchpoints);
        commonData.m_adaptiveStructureWatchpoints = WTFMove(m_adaptiveStructureWatchpoints);
        commonData.m_adaptiveInferredPropertyValueWatchpoints = WTFMove(m_adaptiveInferredPropertyValueWatchpoints);
    }

    template<typename Func>
    bool addWatchpoint(const Func& function)
    {
        if (m_mode == WatchpointRegistrationMode::Add)
            return function(m_watchpoints[m_watchpointIndex++]);
        ++m_counts.m_watchpointCount;
        return true;
    }

    template<typename Func>
    bool addAdaptiveStructureWatchpoint(const Func& function)
    {
        if (m_mode == WatchpointRegistrationMode::Add)
            return function(m_adaptiveStructureWatchpoints[m_adaptiveStructureWatchpointsIndex++]);
        ++m_counts.m_adaptiveStructureWatchpointCount;
        return true;
    }

    template<typename Func>
    bool addAdaptiveInferredPropertyValueWatchpoint(const Func& function)
    {
        if (m_mode == WatchpointRegistrationMode::Add)
            return function(m_adaptiveInferredPropertyValueWatchpoints[m_adaptiveInferredPropertyValueWatchpointsIndex++]);
        ++m_counts.m_adaptiveInferredPropertyValueWatchpointCount;
        return true;
    }

    WatchpointRegistrationMode mode() const { return m_mode; }
    Concurrency concurrency() const { return m_mode == WatchpointRegistrationMode::Add ? Concurrency::MainThread : Concurrency::ConcurrentThread; }

private:
    DesiredWatchpointCounts m_counts;

    unsigned m_watchpointIndex { 0 };
    unsigned m_adaptiveStructureWatchpointsIndex { 0 };
    unsigned m_adaptiveInferredPropertyValueWatchpointsIndex { 0 };
    WatchpointRegistrationMode m_mode { WatchpointRegistrationMode::Collect };
    FixedVector<CodeBlockJettisoningWatchpoint> m_watchpoints;
    FixedVector<AdaptiveStructureWatchpoint> m_adaptiveStructureWatchpoints;
    FixedVector<AdaptiveInferredPropertyValueWatchpoint> m_adaptiveInferredPropertyValueWatchpoints;
};

template<typename T>
struct SetPointerAdaptor {
    static bool add(CodeBlock* codeBlock, T set, WatchpointCollector& collector)
    {
        return collector.addWatchpoint([&](CodeBlockJettisoningWatchpoint& watchpoint) {
            if (hasBeenInvalidated(set, collector.concurrency()))
                return false;

            {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                watchpoint.initialize(codeBlock);
            }
            set->add(&watchpoint);
            return true;
        });
    }
    static bool hasBeenInvalidated(T set, Concurrency)
    {
        return set->hasBeenInvalidated();
    }
    static void dumpInContext(PrintStream& out, T set, DumpContext*)
    {
        out.print(RawPointer(set));
    }
};

struct SymbolTableAdaptor {
    static bool add(CodeBlock*, SymbolTable*, WatchpointCollector&);
    static bool hasBeenInvalidated(SymbolTable* symbolTable, Concurrency)
    {
        return symbolTable->singleton().hasBeenInvalidated();
    }
    static void dumpInContext(PrintStream& out, SymbolTable* symbolTable, DumpContext*)
    {
        out.print(RawPointer(symbolTable));
    }
};

struct FunctionExecutableAdaptor {
    static bool add(CodeBlock*, FunctionExecutable*, WatchpointCollector&);
    static bool hasBeenInvalidated(FunctionExecutable* executable, Concurrency)
    {
        return executable->singleton().hasBeenInvalidated();
    }
    static void dumpInContext(PrintStream& out, FunctionExecutable* executable, DumpContext*)
    {
        out.print(RawPointer(executable));
    }
};

struct ArrayBufferViewWatchpointAdaptor {
    static bool add(CodeBlock*, JSArrayBufferView*, WatchpointCollector&);
    static bool hasBeenInvalidated(JSArrayBufferView* view, Concurrency)
    {
        return !view->length();
    }
    static void dumpInContext(PrintStream& out, JSArrayBufferView* view, DumpContext* context)
    {
        out.print(inContext(JSValue(view), context));
    }
};

struct AdaptiveStructureWatchpointAdaptor {
    static bool add(CodeBlock*, const ObjectPropertyCondition&, WatchpointCollector&);
    static bool hasBeenInvalidated(const ObjectPropertyCondition& key, Concurrency concurrency)
    {
        return !key.isWatchable(PropertyCondition::MakeNoChanges, concurrency);
    }
    static void dumpInContext(
        PrintStream& out, const ObjectPropertyCondition& key, DumpContext* context)
    {
        out.print(inContext(key, context));
    }
};

template<typename WatchpointSetType, typename Adaptor = SetPointerAdaptor<WatchpointSetType>>
class GenericDesiredWatchpoints {
#if ASSERT_ENABLED
    typedef UncheckedKeyHashMap<WatchpointSetType, bool> StateMap;
#endif
public:
    GenericDesiredWatchpoints()
        : m_reallyAdded(false)
    {
    }
    
    void addLazily(const WatchpointSetType& set)
    {
        m_sets.add(set);
    }
    
    bool reallyAdd(CodeBlock* codeBlock, WatchpointCollector& collector)
    {
        if (collector.mode() == WatchpointRegistrationMode::Add)
            RELEASE_ASSERT(!m_reallyAdded);
        
        for (auto& set : m_sets) {
            if (!Adaptor::add(codeBlock, set, collector))
                return false;
        }
        
        if (collector.mode() == WatchpointRegistrationMode::Add)
            m_reallyAdded = true;

        return true;
    }
    
    bool isWatched(const WatchpointSetType& set) const
    {
        return m_sets.contains(set);
    }

    void dumpInContext(PrintStream& out, DumpContext* context) const
    {
        CommaPrinter comma;
        for (const WatchpointSetType& entry : m_sets) {
            out.print(comma);
            Adaptor::dumpInContext(out, entry, context);
        }
    }

private:
    HashSet<WatchpointSetType> m_sets;
    bool m_reallyAdded;
};

class DesiredWatchpoints {
public:
    DesiredWatchpoints();
    ~DesiredWatchpoints();
    
    void addLazily(WatchpointSet&);
    void addLazily(InlineWatchpointSet&);
    void addLazily(Graph&, SymbolTable*);
    void addLazily(Graph&, FunctionExecutable*);
    void addLazily(JSArrayBufferView*);
    
    // It's recommended that you don't call this directly. Use Graph::watchCondition(), which does
    // the required GC magic as well as some other bookkeeping.
    void addLazily(const ObjectPropertyCondition&);

    void addLazily(DesiredGlobalProperty&&);
    
    bool consider(Structure*);

    void countWatchpoints(CodeBlock*, DesiredIdentifiers&, CommonData*);
    
    bool reallyAdd(CodeBlock*, DesiredIdentifiers&, CommonData*);
    
    bool areStillValidOnMainThread(VM&, DesiredIdentifiers&);
    
    bool isWatched(WatchpointSet& set)
    {
        return m_sets.isWatched(&set);
    }
    bool isWatched(InlineWatchpointSet& set)
    {
        return m_inlineSets.isWatched(&set);
    }
    bool isWatched(SymbolTable* symbolTable)
    {
        return m_symbolTables.isWatched(symbolTable);
    }
    bool isWatched(FunctionExecutable* executable)
    {
        return m_functionExecutables.isWatched(executable);
    }
    bool isWatched(JSArrayBufferView* view)
    {
        return m_bufferViews.isWatched(view);
    }
    bool isWatched(const ObjectPropertyCondition& key)
    {
        return m_adaptiveStructureSets.isWatched(key);
    }
    void dumpInContext(PrintStream&, DumpContext*) const;
    
private:
    GenericDesiredWatchpoints<WatchpointSet*> m_sets;
    GenericDesiredWatchpoints<InlineWatchpointSet*> m_inlineSets;
    GenericDesiredWatchpoints<SymbolTable*, SymbolTableAdaptor> m_symbolTables;
    GenericDesiredWatchpoints<FunctionExecutable*, FunctionExecutableAdaptor> m_functionExecutables;
    GenericDesiredWatchpoints<JSArrayBufferView*, ArrayBufferViewWatchpointAdaptor> m_bufferViews;
    GenericDesiredWatchpoints<ObjectPropertyCondition, AdaptiveStructureWatchpointAdaptor> m_adaptiveStructureSets;
    DesiredGlobalProperties m_globalProperties;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)
