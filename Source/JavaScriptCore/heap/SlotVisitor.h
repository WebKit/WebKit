/*
 * Copyright (C) 2011, 2012, 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef SlotVisitor_h
#define SlotVisitor_h

#include "CopyToken.h"
#include "HandleTypes.h"
#include "MarkStack.h"

#include <wtf/HashSet.h>
#include <wtf/text/StringHash.h>

namespace JSC {

class ConservativeRoots;
class GCThreadSharedData;
class Heap;
template<typename T> class JITWriteBarrier;
class UnconditionalFinalizer;
template<typename T> class Weak;
class WeakReferenceHarvester;
template<typename T> class WriteBarrierBase;

class SlotVisitor {
    WTF_MAKE_NONCOPYABLE(SlotVisitor);
    friend class HeapRootVisitor; // Allowed to mark a JSValue* or JSCell** directly.

public:
    SlotVisitor(GCThreadSharedData&);
    ~SlotVisitor();

    MarkStackArray& markStack() { return m_stack; }

    Heap* heap() const;

    void append(ConservativeRoots&);
    
    template<typename T> void append(JITWriteBarrier<T>*);
    template<typename T> void append(WriteBarrierBase<T>*);
    template<typename Iterator> void append(Iterator begin , Iterator end);
    void appendValues(WriteBarrierBase<Unknown>*, size_t count);
    
    template<typename T>
    void appendUnbarrieredPointer(T**);
    void appendUnbarrieredValue(JSValue*);
    template<typename T>
    void appendUnbarrieredWeak(Weak<T>*);
    void unconditionallyAppend(JSCell*);
    
    void addOpaqueRoot(void*);
    bool containsOpaqueRoot(void*);
    TriState containsOpaqueRootTriState(void*);
    int opaqueRootCount();

    GCThreadSharedData& sharedData() const { return m_shared; }
    bool isEmpty() { return m_stack.isEmpty(); }

    void setup();
    void reset();
    void clearMarkStack();

    size_t bytesVisited() const { return m_bytesVisited; }
    size_t bytesCopied() const { return m_bytesCopied; }
    size_t visitCount() const { return m_visitCount; }

    void donate();
    void drain();
    void donateAndDrain();
    
    enum SharedDrainMode { SlaveDrain, MasterDrain };
    void drainFromShared(SharedDrainMode);

    void harvestWeakReferences();
    void finalizeUnconditionalFinalizers();

    void copyLater(JSCell*, CopyToken, void*, size_t);
    
    void reportExtraMemoryUsage(JSCell* owner, size_t);
    
    void addWeakReferenceHarvester(WeakReferenceHarvester*);
    void addUnconditionalFinalizer(UnconditionalFinalizer*);

#if ENABLE(OBJECT_MARK_LOGGING)
    inline void resetChildCount() { m_logChildCount = 0; }
    inline unsigned childCount() { return m_logChildCount; }
    inline void incrementChildCount() { m_logChildCount++; }
#endif

private:
    friend class ParallelModeEnabler;
    
    JS_EXPORT_PRIVATE static void validate(JSCell*);

    void append(JSValue*);
    void append(JSValue*, size_t count);
    void append(JSCell**);
    
    void internalAppend(void* from, JSCell*);
    void internalAppend(void* from, JSValue);
    void internalAppend(void* from, JSValue*);
    
    JS_EXPORT_PRIVATE void mergeOpaqueRoots();
    void mergeOpaqueRootsIfNecessary();
    void mergeOpaqueRootsIfProfitable();
    
    void donateKnownParallel();

    MarkStackArray m_stack;
    HashSet<void*> m_opaqueRoots; // Handle-owning data structures not visible to the garbage collector.
    
    size_t m_bytesVisited;
    size_t m_bytesCopied;
    size_t m_visitCount;
    bool m_isInParallelMode;
    
    GCThreadSharedData& m_shared;

    bool m_shouldHashCons; // Local per-thread copy of shared flag for performance reasons
    typedef HashMap<StringImpl*, JSValue> UniqueStringMap;
    UniqueStringMap m_uniqueStrings;

#if ENABLE(OBJECT_MARK_LOGGING)
    unsigned m_logChildCount;
#endif

public:
#if !ASSERT_DISABLED
    bool m_isCheckingForDefaultMarkViolation;
    bool m_isDraining;
#endif
};

class ParallelModeEnabler {
public:
    ParallelModeEnabler(SlotVisitor& stack)
        : m_stack(stack)
    {
        ASSERT(!m_stack.m_isInParallelMode);
        m_stack.m_isInParallelMode = true;
    }
    
    ~ParallelModeEnabler()
    {
        ASSERT(m_stack.m_isInParallelMode);
        m_stack.m_isInParallelMode = false;
    }
    
private:
    SlotVisitor& m_stack;
};

} // namespace JSC

#endif // SlotVisitor_h
