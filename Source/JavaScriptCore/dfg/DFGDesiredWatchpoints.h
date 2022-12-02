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
class WatchpointCollector final {
    WTF_MAKE_NONCOPYABLE(WatchpointCollector);
public:
    WatchpointCollector() = default;

    void materialize()
    {
        m_watchpoints = FixedVector<CodeBlockJettisoningWatchpoint>(m_watchpointCount);
        m_adaptiveStructureWatchpoints = FixedVector<AdaptiveStructureWatchpoint>(m_adaptiveStructureWatchpointCount);
        m_adaptiveInferredPropertyValueWatchpoints = FixedVector<AdaptiveInferredPropertyValueWatchpoint>(m_adaptiveInferredPropertyValueWatchpointCount);
        m_mode = WatchpointRegistrationMode::Add;
    }

    template<typename Func>
    void addWatchpoint(const Func& function)
    {
        if (m_mode == WatchpointRegistrationMode::Add)
            function(m_watchpoints[m_watchpointIndex++]);
        else
            ++m_watchpointCount;
    }

    template<typename Func>
    void addAdaptiveStructureWatchpoint(const Func& function)
    {
        if (m_mode == WatchpointRegistrationMode::Add)
            function(m_adaptiveStructureWatchpoints[m_adaptiveStructureWatchpointsIndex++]);
        else
            ++m_adaptiveStructureWatchpointCount;
    }

    template<typename Func>
    void addAdaptiveInferredPropertyValueWatchpoint(const Func& function)
    {
        if (m_mode == WatchpointRegistrationMode::Add)
            function(m_adaptiveInferredPropertyValueWatchpoints[m_adaptiveInferredPropertyValueWatchpointsIndex++]);
        else
            ++m_adaptiveInferredPropertyValueWatchpointCount;
    }

    void finalize(CodeBlock*, CommonData&);

    WatchpointRegistrationMode mode() const { return m_mode; }

private:
    unsigned m_watchpointCount { 0 };
    unsigned m_adaptiveStructureWatchpointCount { 0 };
    unsigned m_adaptiveInferredPropertyValueWatchpointCount { 0 };

    unsigned m_watchpointIndex { 0 };
    unsigned m_adaptiveStructureWatchpointsIndex { 0 };
    unsigned m_adaptiveInferredPropertyValueWatchpointsIndex { 0 };

    FixedVector<CodeBlockJettisoningWatchpoint> m_watchpoints;
    FixedVector<AdaptiveStructureWatchpoint> m_adaptiveStructureWatchpoints;
    FixedVector<AdaptiveInferredPropertyValueWatchpoint> m_adaptiveInferredPropertyValueWatchpoints;
    WatchpointRegistrationMode m_mode { WatchpointRegistrationMode::Collect };
};

template<typename T>
struct SetPointerAdaptor {
    static void add(CodeBlock* codeBlock, T set, WatchpointCollector& collector)
    {
        collector.addWatchpoint([&](CodeBlockJettisoningWatchpoint& watchpoint) {
            {
                ConcurrentJSLocker locker(codeBlock->m_lock);
                watchpoint.initialize(codeBlock);
            }
            set->add(&watchpoint);
        });
    }
    static bool hasBeenInvalidated(T set)
    {
        return set->hasBeenInvalidated();
    }
    static void dumpInContext(PrintStream& out, T set, DumpContext*)
    {
        out.print(RawPointer(set));
    }
};

struct SymbolTableAdaptor {
    static void add(CodeBlock*, SymbolTable*, WatchpointCollector&);
    static bool hasBeenInvalidated(SymbolTable* symbolTable)
    {
        return symbolTable->singleton().hasBeenInvalidated();
    }
    static void dumpInContext(PrintStream& out, SymbolTable* symbolTable, DumpContext*)
    {
        out.print(RawPointer(symbolTable));
    }
};

struct FunctionExecutableAdaptor {
    static void add(CodeBlock*, FunctionExecutable*, WatchpointCollector&);
    static bool hasBeenInvalidated(FunctionExecutable* executable)
    {
        return executable->singleton().hasBeenInvalidated();
    }
    static void dumpInContext(PrintStream& out, FunctionExecutable* executable, DumpContext*)
    {
        out.print(RawPointer(executable));
    }
};

struct ArrayBufferViewWatchpointAdaptor {
    static void add(CodeBlock*, JSArrayBufferView*, WatchpointCollector&);
    static bool hasBeenInvalidated(JSArrayBufferView* view)
    {
        return !view->length();
    }
    static void dumpInContext(PrintStream& out, JSArrayBufferView* view, DumpContext* context)
    {
        out.print(inContext(JSValue(view), context));
    }
};

struct AdaptiveStructureWatchpointAdaptor {
    static void add(CodeBlock*, const ObjectPropertyCondition&, WatchpointCollector&);
    static bool hasBeenInvalidated(const ObjectPropertyCondition& key)
    {
        return !key.isWatchable(PropertyCondition::MakeNoChanges);
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
    typedef HashMap<WatchpointSetType, bool> StateMap;
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
    
    void reallyAdd(CodeBlock* codeBlock, WatchpointCollector& collector)
    {
        if (collector.mode() == WatchpointRegistrationMode::Add)
            RELEASE_ASSERT(!m_reallyAdded);
        
        for (auto& set : m_sets)
            Adaptor::add(codeBlock, set, collector);
        
        if (collector.mode() == WatchpointRegistrationMode::Add)
            m_reallyAdded = true;
    }
    
    bool areStillValid() const
    {
        for (auto& set : m_sets) {
            if (Adaptor::hasBeenInvalidated(set))
                return false;
        }
        
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
    void addLazily(SymbolTable*);
    void addLazily(FunctionExecutable*);
    void addLazily(JSArrayBufferView*);
    
    // It's recommended that you don't call this directly. Use Graph::watchCondition(), which does
    // the required GC magic as well as some other bookkeeping.
    void addLazily(const ObjectPropertyCondition&);

    void addLazily(DesiredGlobalProperty&&);
    
    bool consider(Structure*);
    
    void reallyAdd(CodeBlock*, DesiredIdentifiers&, CommonData*);
    
    bool areStillValid() const;
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
