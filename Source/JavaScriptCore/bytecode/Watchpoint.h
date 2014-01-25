/*
 * Copyright (C) 2012, 2013 Apple Inc. All rights reserved.
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

#ifndef Watchpoint_h
#define Watchpoint_h

#include <wtf/Atomics.h>
#include <wtf/SentinelLinkedList.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace JSC {

class Watchpoint : public BasicRawSentinelNode<Watchpoint> {
public:
    Watchpoint()
    {
    }
    
    virtual ~Watchpoint();

    void fire() { fireInternal(); }
    
protected:
    virtual void fireInternal() = 0;
};

enum WatchpointState {
    ClearWatchpoint,
    IsWatched,
    IsInvalidated
};

class InlineWatchpointSet;

class WatchpointSet : public ThreadSafeRefCounted<WatchpointSet> {
    friend class LLIntOffsetsExtractor;
public:
    WatchpointSet(WatchpointState);
    ~WatchpointSet(); // Note that this will not fire any of the watchpoints; if you need to know when a WatchpointSet dies then you need a separate mechanism for this.
    
    // It is safe to call this from another thread. It may return an old
    // state. Guarantees that if *first* read the state() of the thing being
    // watched and it returned IsWatched and *second* you actually read its
    // value then it's safe to assume that if the state being watched changes
    // then also the watchpoint state() will change to IsInvalidated.
    WatchpointState state() const
    {
        WTF::loadLoadFence();
        WatchpointState result = static_cast<WatchpointState>(m_state);
        WTF::loadLoadFence();
        return result;
    }
    
    // It is safe to call this from another thread.  It may return true
    // even if the set actually had been invalidated, but that ought to happen
    // only in the case of races, and should be rare. Guarantees that if you
    // call this after observing something that must imply that the set is
    // invalidated, then you will see this return false. This is ensured by
    // issuing a load-load fence prior to querying the state.
    bool isStillValid() const
    {
        return state() != IsInvalidated;
    }
    // Like isStillValid(), may be called from another thread.
    bool hasBeenInvalidated() const { return !isStillValid(); }
    
    // As a convenience, this will ignore 0. That's because code paths in the DFG
    // that create speculation watchpoints may choose to bail out if speculation
    // had already been terminated.
    void add(Watchpoint*);
    
    // Force the watchpoint set to behave as if it was being watched even if no
    // watchpoints have been installed. This will result in invalidation if the
    // watchpoint would have fired. That's a pretty good indication that you
    // probably don't want to set watchpoints, since we typically don't want to
    // set watchpoints that we believe will actually be fired.
    void startWatching()
    {
        ASSERT(state() != IsInvalidated);
        m_state = IsWatched;
    }
    
    void fireAll()
    {
        if (state() != IsWatched)
            return;
        fireAllSlow();
    }
    
    void touch()
    {
        if (state() == ClearWatchpoint)
            startWatching();
        else
            fireAll();
    }
    
    void invalidate()
    {
        if (state() == IsWatched)
            fireAll();
        m_state = IsInvalidated;
    }

    int8_t* addressOfState() { return &m_state; }
    int8_t* addressOfSetIsNotEmpty() { return &m_setIsNotEmpty; }
    
    JS_EXPORT_PRIVATE void fireAllSlow(); // Call only if you've checked isWatched.
    
private:
    void fireAllWatchpoints();
    
    friend class InlineWatchpointSet;

    int8_t m_state;
    int8_t m_setIsNotEmpty;

    SentinelLinkedList<Watchpoint, BasicRawSentinelNode<Watchpoint>> m_set;
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
    InlineWatchpointSet(WatchpointState state)
        : m_data(encodeState(state))
    {
    }
    
    ~InlineWatchpointSet()
    {
        if (isThin())
            return;
        freeFat();
    }
    
    // It is safe to call this from another thread.  It may return false
    // even if the set actually had been invalidated, but that ought to happen
    // only in the case of races, and should be rare.
    bool hasBeenInvalidated() const
    {
        WTF::loadLoadFence();
        uintptr_t data = m_data;
        if (isFat(data)) {
            WTF::loadLoadFence();
            return fat(data)->hasBeenInvalidated();
        }
        return decodeState(data) == IsInvalidated;
    }
    
    // Like hasBeenInvalidated(), may be called from another thread.
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
        ASSERT(decodeState(m_data) != IsInvalidated);
        m_data = encodeState(IsWatched);
    }
    
    void fireAll()
    {
        if (isFat()) {
            fat()->fireAll();
            return;
        }
        if (decodeState(m_data) == ClearWatchpoint)
            return;
        m_data = encodeState(IsInvalidated);
        WTF::storeStoreFence();
    }
    
    void touch()
    {
        if (isFat()) {
            fat()->touch();
            return;
        }
        if (decodeState(m_data) == ClearWatchpoint)
            m_data = encodeState(IsWatched);
        else
            m_data = encodeState(IsInvalidated);
        WTF::storeStoreFence();
    }
    
private:
    static const uintptr_t IsThinFlag        = 1;
    static const uintptr_t StateMask         = 6;
    static const uintptr_t StateShift        = 1;
    
    static bool isThin(uintptr_t data) { return data & IsThinFlag; }
    static bool isFat(uintptr_t data) { return !isThin(data); }
    
    static WatchpointState decodeState(uintptr_t data)
    {
        ASSERT(isThin(data));
        return static_cast<WatchpointState>((data & StateMask) >> StateShift);
    }
    
    static uintptr_t encodeState(WatchpointState state)
    {
        return (state << StateShift) | IsThinFlag;
    }
    
    bool isThin() const { return isThin(m_data); }
    bool isFat() const { return isFat(m_data); };
    
    static WatchpointSet* fat(uintptr_t data)
    {
        return bitwise_cast<WatchpointSet*>(data);
    }
    
    WatchpointSet* fat()
    {
        ASSERT(isFat());
        return fat(m_data);
    }
    
    const WatchpointSet* fat() const
    {
        ASSERT(isFat());
        return fat(m_data);
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

