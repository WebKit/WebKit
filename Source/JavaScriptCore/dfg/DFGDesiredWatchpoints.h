/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef DFGDesiredWatchpoints_h
#define DFGDesiredWatchpoints_h

#if ENABLE(DFG_JIT)

#include "CodeOrigin.h"
#include "DFGCommonData.h"
#include "JSArrayBufferView.h"
#include "Watchpoint.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace JSC { namespace DFG {

template<typename WatchpointSetType>
struct WatchpointForGenericWatchpointSet {
    WatchpointForGenericWatchpointSet()
        : m_exitKind(ExitKindUnset)
        , m_set(0)
    {
    }
    
    WatchpointForGenericWatchpointSet(
        CodeOrigin codeOrigin, ExitKind exitKind, WatchpointSetType* set)
        : m_codeOrigin(codeOrigin)
        , m_exitKind(exitKind)
        , m_set(set)
    {
    }
    
    CodeOrigin m_codeOrigin;
    ExitKind m_exitKind;
    WatchpointSetType* m_set;
};

template<typename T>
struct GenericSetAdaptor {
    static void add(CodeBlock*, T* set, Watchpoint* watchpoint)
    {
        return set->add(watchpoint);
    }
    static bool hasBeenInvalidated(T* set) { return set->hasBeenInvalidated(); }
};

struct ArrayBufferViewWatchpointAdaptor {
    static void add(CodeBlock*, JSArrayBufferView*, Watchpoint*);
    static bool hasBeenInvalidated(JSArrayBufferView* view)
    {
        bool result = !view->length();
        WTF::loadLoadFence();
        return result;
    }
};

template<typename WatchpointSetType, typename Adaptor = GenericSetAdaptor<WatchpointSetType>>
class GenericDesiredWatchpoints {
    WTF_MAKE_NONCOPYABLE(GenericDesiredWatchpoints);
#if !ASSERT_DISABLED
    typedef HashMap<WatchpointSetType*, bool> StateMap;
#endif
public:
    GenericDesiredWatchpoints()
        : m_reallyAdded(false)
    {
    }
    
    void addLazily(WatchpointSetType* set)
    {
        m_sets.add(set);
    }
    
    void addLazily(CodeOrigin codeOrigin, ExitKind exitKind, WatchpointSetType* set)
    {
        m_profiledWatchpoints.append(
            WatchpointForGenericWatchpointSet<WatchpointSetType>(codeOrigin, exitKind, set));
    }
    
    void reallyAdd(CodeBlock* codeBlock, CommonData& common)
    {
        RELEASE_ASSERT(!m_reallyAdded);
        
        typename HashSet<WatchpointSetType*>::iterator iter = m_sets.begin();
        typename HashSet<WatchpointSetType*>::iterator end = m_sets.end();
        for (; iter != end; ++iter) {
            common.watchpoints.append(CodeBlockJettisoningWatchpoint(codeBlock));
            Adaptor::add(codeBlock, *iter, &common.watchpoints.last());
        }
        
        for (unsigned i = m_profiledWatchpoints.size(); i--;) {
            WatchpointForGenericWatchpointSet<WatchpointSetType> watchpoint =
                m_profiledWatchpoints[i];
            common.profiledWatchpoints.append(
                ProfiledCodeBlockJettisoningWatchpoint(watchpoint.m_codeOrigin, watchpoint.m_exitKind, codeBlock));
            Adaptor::add(codeBlock, watchpoint.m_set, &common.profiledWatchpoints.last());
        }
        
        m_reallyAdded = true;
    }
    
    bool areStillValid() const
    {
        typename HashSet<WatchpointSetType*>::iterator iter = m_sets.begin();
        typename HashSet<WatchpointSetType*>::iterator end = m_sets.end();
        for (; iter != end; ++iter) {
            if (Adaptor::hasBeenInvalidated(*iter))
                return false;
        }
        
        for (unsigned i = m_profiledWatchpoints.size(); i--;) {
            if (Adaptor::hasBeenInvalidated(m_profiledWatchpoints[i].m_set))
                return false;
        }
        
        return true;
    }
    
#if ASSERT_DISABLED
    bool isStillValid(WatchpointSetType* set)
    {
        return !Adaptor::hasBeenInvalidated(set);
    }
    
    bool shouldAssumeMixedState(WatchpointSetType*)
    {
        return true;
    }
#else
    bool isStillValid(WatchpointSetType* set)
    {
        bool result = !Adaptor::hasBeenInvalidated(set);
        m_firstKnownState.add(set, result);
        return result;
    }
    
    bool shouldAssumeMixedState(WatchpointSetType* set)
    {
        typename StateMap::iterator iter = m_firstKnownState.find(set);
        if (iter == m_firstKnownState.end())
            return false;
        
        return iter->value != !Adaptor::hasBeenInvalidated(set);
    }
#endif
    
    bool isValidOrMixed(WatchpointSetType* set)
    {
        return isStillValid(set) || shouldAssumeMixedState(set);
    }

private:
    Vector<WatchpointForGenericWatchpointSet<WatchpointSetType>> m_profiledWatchpoints;
    HashSet<WatchpointSetType*> m_sets;
#if !ASSERT_DISABLED
    StateMap m_firstKnownState;
#endif
    bool m_reallyAdded;
};

class DesiredWatchpoints {
public:
    DesiredWatchpoints();
    ~DesiredWatchpoints();
    
    void addLazily(WatchpointSet*);
    void addLazily(InlineWatchpointSet&);
    void addLazily(JSArrayBufferView*);
    void addLazily(CodeOrigin, ExitKind, WatchpointSet*);
    void addLazily(CodeOrigin, ExitKind, InlineWatchpointSet&);
    
    void reallyAdd(CodeBlock*, CommonData&);
    
    bool areStillValid() const;
    
    bool isStillValid(WatchpointSet* set)
    {
        return m_sets.isStillValid(set);
    }
    bool isStillValid(InlineWatchpointSet& set)
    {
        return m_inlineSets.isStillValid(&set);
    }
    bool isStillValid(JSArrayBufferView* view)
    {
        return m_bufferViews.isStillValid(view);
    }
    bool shouldAssumeMixedState(WatchpointSet* set)
    {
        return m_sets.shouldAssumeMixedState(set);
    }
    bool shouldAssumeMixedState(InlineWatchpointSet& set)
    {
        return m_inlineSets.shouldAssumeMixedState(&set);
    }
    bool shouldAssumeMixedState(JSArrayBufferView* view)
    {
        return m_bufferViews.shouldAssumeMixedState(view);
    }
    bool isValidOrMixed(WatchpointSet* set)
    {
        return m_sets.isValidOrMixed(set);
    }
    bool isValidOrMixed(InlineWatchpointSet& set)
    {
        return m_inlineSets.isValidOrMixed(&set);
    }
    bool isValidOrMixed(JSArrayBufferView* view)
    {
        return m_bufferViews.isValidOrMixed(view);
    }
    
private:
    GenericDesiredWatchpoints<WatchpointSet> m_sets;
    GenericDesiredWatchpoints<InlineWatchpointSet> m_inlineSets;
    GenericDesiredWatchpoints<JSArrayBufferView, ArrayBufferViewWatchpointAdaptor> m_bufferViews;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGDesiredWatchpoints_h

