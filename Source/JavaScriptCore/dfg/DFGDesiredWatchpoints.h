/*
 * Copyright (C) 2013, 2014 Apple Inc. All rights reserved.
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

class Graph;

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
    
    void reallyAdd(CodeBlock* codeBlock, CommonData& common)
    {
        RELEASE_ASSERT(!m_reallyAdded);
        
        typename HashSet<WatchpointSetType*>::iterator iter = m_sets.begin();
        typename HashSet<WatchpointSetType*>::iterator end = m_sets.end();
        for (; iter != end; ++iter) {
            common.watchpoints.append(CodeBlockJettisoningWatchpoint(codeBlock));
            Adaptor::add(codeBlock, *iter, &common.watchpoints.last());
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
        
        return true;
    }
    
    bool isWatched(WatchpointSetType* set) const
    {
        return m_sets.contains(set);
    }

private:
    HashSet<WatchpointSetType*> m_sets;
    bool m_reallyAdded;
};

class DesiredWatchpoints {
public:
    DesiredWatchpoints();
    ~DesiredWatchpoints();
    
    void addLazily(WatchpointSet*);
    void addLazily(InlineWatchpointSet&);
    void addLazily(JSArrayBufferView*);
    
    bool consider(Structure*);
    
    void reallyAdd(CodeBlock*, CommonData&);
    
    bool areStillValid() const;
    
    bool isWatched(WatchpointSet* set)
    {
        return m_sets.isWatched(set);
    }
    bool isWatched(InlineWatchpointSet& set)
    {
        return m_inlineSets.isWatched(&set);
    }
    bool isWatched(JSArrayBufferView* view)
    {
        return m_bufferViews.isWatched(view);
    }
    
private:
    GenericDesiredWatchpoints<WatchpointSet> m_sets;
    GenericDesiredWatchpoints<InlineWatchpointSet> m_inlineSets;
    GenericDesiredWatchpoints<JSArrayBufferView, ArrayBufferViewWatchpointAdaptor> m_bufferViews;
};

} } // namespace JSC::DFG

#endif // ENABLE(DFG_JIT)

#endif // DFGDesiredWatchpoints_h

