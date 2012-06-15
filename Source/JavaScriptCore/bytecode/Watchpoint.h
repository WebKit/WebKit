/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef Watchpoint_h
#define Watchpoint_h

#include "CodeLocation.h"
#include "MacroAssembler.h"
#include <wtf/RefCounted.h>
#include <wtf/SentinelLinkedList.h>

namespace JSC {

class Watchpoint : public BasicRawSentinelNode<Watchpoint> {
public:
    Watchpoint()
        : m_source(std::numeric_limits<uintptr_t>::max())
        , m_destination(std::numeric_limits<uintptr_t>::max())
    {
    }

#if ENABLE(JIT)
    Watchpoint(MacroAssembler::Label source)
        : m_source(source.m_label.m_offset)
        , m_destination(std::numeric_limits<uintptr_t>::max())
    {
    }
    
    void setDestination(MacroAssembler::Label destination)
    {
        m_destination = destination.m_label.m_offset;
    }
    
    void correctLabels(LinkBuffer&);
#endif
    
    ~Watchpoint();
    
    void fire();
    
private:
    uintptr_t m_source;
    uintptr_t m_destination;
};

enum InitialWatchpointSetMode { InitializedWatching, InitializedBlind };

class InlineWatchpointSet;

class WatchpointSet : public RefCounted<WatchpointSet> {
public:
    WatchpointSet(InitialWatchpointSetMode);
    ~WatchpointSet();
    
    bool isStillValid() const { return !m_isInvalidated; }
    bool hasBeenInvalidated() const { return m_isInvalidated; }
    
    // As a convenience, this will ignore 0. That's because code paths in the DFG
    // that create speculation watchpoints may choose to bail out if speculation
    // had already been terminated.
    void add(Watchpoint*);
    
    // Force the watchpoint set to behave as if it was being watched even if no
    // watchpoints have been installed. This will result in invalidation if the
    // watchpoint would have fired. That's a pretty good indication that you
    // probably don't want to set watchpoints, since we typically don't want to
    // set watchpoints that we believe will actually be fired.
    void startWatching() { m_isWatched = true; }
    
    void notifyWrite()
    {
        if (!m_isWatched)
            return;
        notifyWriteSlow();
    }
    
    bool* addressOfIsWatched() { return &m_isWatched; }
    
    void notifyWriteSlow(); // Call only if you've checked isWatched.
    
private:
    void fireAllWatchpoints();
    
    friend class InlineWatchpointSet;
    
    SentinelLinkedList<Watchpoint, BasicRawSentinelNode<Watchpoint> > m_set;
    bool m_isWatched;
    bool m_isInvalidated;
};

// InlineWatchpointSet is a low-overhead, non-copyable watchpoint set in which
// it is not possible to quickly query whether it is being watched in a single
// branch. There is a fairly simple tradeoff between WatchpointSet and
// InlineWatchpointSet:
//
// Do you have to emit JIT code that rapidly tests whether the watchpoint set
// is being watched?  If so, use WatchpointSet.
//
// Do you need multiple parties to have pointers to the same WatchpointSet?
// If so, use WatchpointSet.
//
// Do you have to allocate a lot of watchpoint sets?  If so, use
// InlineWatchpointSet unless you answered "yes" to the previous questions.
//
// InlineWatchpointSet will use just one pointer-width word of memory unless
// you actually add watchpoints to it, in which case it internally inflates
// to a pointer to a WatchpointSet, and transfers its state to the
// WatchpointSet.

class InlineWatchpointSet {
    WTF_MAKE_NONCOPYABLE(InlineWatchpointSet);
public:
    InlineWatchpointSet(InitialWatchpointSetMode mode)
        : m_data((mode == InitializedWatching ? IsWatchedFlag : 0) | IsThinFlag)
    {
    }
    
    ~InlineWatchpointSet()
    {
        if (isThin())
            return;
        freeFat();
    }
    
    bool hasBeenInvalidated() const
    {
        if (isFat())
            return fat()->hasBeenInvalidated();
        return m_data & IsInvalidatedFlag;
    }
    
    bool isStillValid() const
    {
        return !hasBeenInvalidated();
    }
    
    void add(Watchpoint*);
    
    void startWatching()
    {
        if (isFat()) {
            fat()->startWatching();
            return;
        }
        m_data |= IsWatchedFlag;
    }
    
    void notifyWrite()
    {
        if (isFat()) {
            fat()->notifyWrite();
            return;
        }
        if (!(m_data & IsWatchedFlag))
            return;
        m_data |= IsInvalidatedFlag;
    }
    
private:
    static const uintptr_t IsThinFlag        = 1;
    static const uintptr_t IsInvalidatedFlag = 2;
    static const uintptr_t IsWatchedFlag     = 4;
    
    bool isThin() const { return m_data & IsThinFlag; }
    bool isFat() const { return !isThin(); };
    
    WatchpointSet* fat()
    {
        ASSERT(isFat());
        return bitwise_cast<WatchpointSet*>(m_data);
    }
    
    const WatchpointSet* fat() const
    {
        ASSERT(isFat());
        return bitwise_cast<WatchpointSet*>(m_data);
    }
    
    WatchpointSet* inflate()
    {
        if (LIKELY(isFat()))
            return fat();
        return inflateSlow();
    }
    
    JS_EXPORT_PRIVATE WatchpointSet* inflateSlow();
    JS_EXPORT_PRIVATE void freeFat();
    
    uintptr_t m_data;
};

} // namespace JSC

#endif // Watchpoint_h

