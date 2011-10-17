/*
 * Copyright (C) 2009, 2011 Apple Inc. All rights reserved.
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

#ifndef MarkStack_h
#define MarkStack_h

#include "HandleTypes.h"
#include "JSValue.h"
#include "Register.h"
#include "VTableSpectrum.h"
#include "WeakReferenceHarvester.h"
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Vector.h>
#include <wtf/Noncopyable.h>
#include <wtf/OSAllocator.h>
#include <wtf/PageBlock.h>

namespace JSC {

    class ConservativeRoots;
    class JSGlobalData;
    class MarkStack;
    class Register;
    template<typename T> class WriteBarrierBase;
    template<typename T> class JITWriteBarrier;
    
    class MarkStackArray {
    public:
        MarkStackArray();
        ~MarkStackArray();

        void expand();
        void append(const JSCell*);

        const JSCell* removeLast();

        bool isEmpty();

        void shrinkAllocation(size_t);

    private:
        const JSCell** m_data;
        size_t m_top;
        size_t m_capacity;
        size_t m_allocated;
    };

    class MarkStack {
        WTF_MAKE_NONCOPYABLE(MarkStack);
        friend class HeapRootVisitor; // Allowed to mark a JSValue* or JSCell** directly.

    public:
        static void* allocateStack(size_t);
        static void releaseStack(void*, size_t);

        MarkStack(void* jsArrayVPtr, void* jsFinalObjectVPtr, void* jsStringVPtr);
        ~MarkStack();

        void append(ConservativeRoots&);
        
        template<typename T> void append(JITWriteBarrier<T>*);
        template<typename T> void append(WriteBarrierBase<T>*);
        void appendValues(WriteBarrierBase<Unknown>*, size_t count);
        
        template<typename T>
        void appendUnbarrieredPointer(T**);
        
        bool addOpaqueRoot(void*);
        bool containsOpaqueRoot(void*);
        int opaqueRootCount();

        void reset();

        size_t visitCount() const { return m_visitCount; }

#if ENABLE(SIMPLE_HEAP_PROFILING)
        VTableSpectrum m_visitedTypeCounts;
#endif

        void addWeakReferenceHarvester(WeakReferenceHarvester* weakReferenceHarvester)
        {
            if (weakReferenceHarvester->m_nextAndFlag & 1)
                return;
            weakReferenceHarvester->m_nextAndFlag = reinterpret_cast<uintptr_t>(m_firstWeakReferenceHarvester) | 1;
            m_firstWeakReferenceHarvester = weakReferenceHarvester;
        }

    protected:
        static void validate(JSCell*);

        void append(JSValue*);
        void append(JSValue*, size_t count);
        void append(JSCell**);

        void internalAppend(JSCell*);
        void internalAppend(JSValue);

        MarkStackArray m_stack;
        void* m_jsArrayVPtr;
        void* m_jsFinalObjectVPtr;
        void* m_jsStringVPtr;
        HashSet<void*> m_opaqueRoots; // Handle-owning data structures not visible to the garbage collector.
        WeakReferenceHarvester* m_firstWeakReferenceHarvester;
        
#if !ASSERT_DISABLED
    public:
        bool m_isCheckingForDefaultMarkViolation;
        bool m_isDraining;
#endif
    protected:
        size_t m_visitCount;
    };

    inline MarkStack::MarkStack(void* jsArrayVPtr, void* jsFinalObjectVPtr, void* jsStringVPtr)
        : m_jsArrayVPtr(jsArrayVPtr)
        , m_jsFinalObjectVPtr(jsFinalObjectVPtr)
        , m_jsStringVPtr(jsStringVPtr)
        , m_firstWeakReferenceHarvester(0)
#if !ASSERT_DISABLED
        , m_isCheckingForDefaultMarkViolation(false)
        , m_isDraining(false)
#endif
        , m_visitCount(0)
    {
    }

    inline MarkStack::~MarkStack()
    {
        ASSERT(m_stack.isEmpty());
    }

    inline bool MarkStack::addOpaqueRoot(void* root)
    {
        return m_opaqueRoots.add(root).second;
    }

    inline bool MarkStack::containsOpaqueRoot(void* root)
    {
        return m_opaqueRoots.contains(root);
    }

    inline int MarkStack::opaqueRootCount()
    {
        return m_opaqueRoots.size();
    }

    inline void* MarkStack::allocateStack(size_t size)
    {
        return OSAllocator::reserveAndCommit(size);
    }

    inline void MarkStack::releaseStack(void* addr, size_t size)
    {
        OSAllocator::decommitAndRelease(addr, size);
    }

    inline void MarkStackArray::append(const JSCell* cell)
    {
        if (m_top == m_capacity)
            expand();
        m_data[m_top++] = cell;
    }

    inline const JSCell* MarkStackArray::removeLast()
    {
        ASSERT(m_top);
        return m_data[--m_top];
    }
    
    inline bool MarkStackArray::isEmpty()
    {
        return !m_top;
    }

    ALWAYS_INLINE void MarkStack::append(JSValue* slot, size_t count)
    {
        for (size_t i = 0; i < count; ++i) {
            JSValue& value = slot[i];
            if (!value)
                continue;
            internalAppend(value);
        }
    }

    template<typename T>
    inline void MarkStack::appendUnbarrieredPointer(T** slot)
    {
        ASSERT(slot);
        JSCell* cell = *slot;
        if (cell)
            internalAppend(cell);
    }
    
    ALWAYS_INLINE void MarkStack::append(JSValue* slot)
    {
        ASSERT(slot);
        internalAppend(*slot);
    }

    ALWAYS_INLINE void MarkStack::append(JSCell** slot)
    {
        ASSERT(slot);
        internalAppend(*slot);
    }

    ALWAYS_INLINE void MarkStack::internalAppend(JSValue value)
    {
        ASSERT(value);
        if (!value.isCell())
            return;
        internalAppend(value.asCell());
    }

    class SlotVisitor;

} // namespace JSC

#endif
