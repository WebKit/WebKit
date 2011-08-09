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
    
    struct MarkSet {
        MarkSet(JSValue* values, JSValue* end);

        JSValue* m_values;
        JSValue* m_end;
    };

    template<typename T> class MarkStackArray {
    public:
        MarkStackArray();
        ~MarkStackArray();

        void expand();
        void append(const T&);

        T removeLast();
        T& last();

        bool isEmpty();
        size_t size();

        void shrinkAllocation(size_t);

    private:
        size_t m_top;
        size_t m_allocated;
        size_t m_capacity;
        T* m_data;
    };

    class MarkStack {
        WTF_MAKE_NONCOPYABLE(MarkStack);
        friend class HeapRootVisitor; // Allowed to mark a JSValue* or JSCell** directly.

    public:
        static void* allocateStack(size_t);
        static void releaseStack(void*, size_t);

        MarkStack(void* jsArrayVPtr);
        ~MarkStack();

        void append(ConservativeRoots&);
        
        template<typename T> inline void append(JITWriteBarrier<T>*);
        template<typename T> inline void append(WriteBarrierBase<T>*);
        inline void appendValues(WriteBarrierBase<Unknown>*, size_t count);
        
        bool addOpaqueRoot(void*);
        bool containsOpaqueRoot(void*);
        int opaqueRootCount();

        void reset();

    protected:
#if ENABLE(GC_VALIDATION)
        static void validateSet(JSValue*, size_t);
        static void validateValue(JSValue);
#endif

        void append(JSValue*);
        void append(JSValue*, size_t count);
        void append(JSCell**);

        void internalAppend(JSCell*);
        void internalAppend(JSValue);

        void* m_jsArrayVPtr;
        MarkStackArray<MarkSet> m_markSets;
        MarkStackArray<JSCell*> m_values;
        HashSet<void*> m_opaqueRoots; // Handle-owning data structures not visible to the garbage collector.

#if !ASSERT_DISABLED
    public:
        bool m_isCheckingForDefaultMarkViolation;
        bool m_isDraining;
#endif
    };

    inline MarkStack::MarkStack(void* jsArrayVPtr)
        : m_jsArrayVPtr(jsArrayVPtr)
#if !ASSERT_DISABLED
        , m_isCheckingForDefaultMarkViolation(false)
        , m_isDraining(false)
#endif
    {
    }

    inline MarkStack::~MarkStack()
    {
        ASSERT(m_markSets.isEmpty());
        ASSERT(m_values.isEmpty());
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

    inline MarkSet::MarkSet(JSValue* values, JSValue* end)
            : m_values(values)
            , m_end(end)
        {
            ASSERT(values);
        }

    inline void* MarkStack::allocateStack(size_t size)
    {
        return OSAllocator::reserveAndCommit(size);
    }

    inline void MarkStack::releaseStack(void* addr, size_t size)
    {
        OSAllocator::decommitAndRelease(addr, size);
    }

    template <typename T> inline MarkStackArray<T>::MarkStackArray()
        : m_top(0)
        , m_allocated(pageSize())
        , m_capacity(m_allocated / sizeof(T))
    {
        m_data = reinterpret_cast<T*>(MarkStack::allocateStack(m_allocated));
    }

    template <typename T> inline MarkStackArray<T>::~MarkStackArray()
    {
        MarkStack::releaseStack(m_data, m_allocated);
    }

    template <typename T> inline void MarkStackArray<T>::expand()
    {
        size_t oldAllocation = m_allocated;
        m_allocated *= 2;
        m_capacity = m_allocated / sizeof(T);
        void* newData = MarkStack::allocateStack(m_allocated);
        memcpy(newData, m_data, oldAllocation);
        MarkStack::releaseStack(m_data, oldAllocation);
        m_data = reinterpret_cast<T*>(newData);
    }

    template <typename T> inline void MarkStackArray<T>::append(const T& v)
    {
        if (m_top == m_capacity)
            expand();
        m_data[m_top++] = v;
    }

    template <typename T> inline T MarkStackArray<T>::removeLast()
    {
        ASSERT(m_top);
        return m_data[--m_top];
    }
    
    template <typename T> inline T& MarkStackArray<T>::last()
    {
        ASSERT(m_top);
        return m_data[m_top - 1];
    }

    template <typename T> inline bool MarkStackArray<T>::isEmpty()
    {
        return m_top == 0;
    }

    template <typename T> inline size_t MarkStackArray<T>::size()
    {
        return m_top;
    }

    template <typename T> inline void MarkStackArray<T>::shrinkAllocation(size_t size)
    {
        ASSERT(size <= m_allocated);
        ASSERT(isPageAligned(size));
        if (size == m_allocated)
            return;
#if OS(WINDOWS) || OS(SYMBIAN) || PLATFORM(BREWMP)
        // We cannot release a part of a region with VirtualFree.  To get around this,
        // we'll release the entire region and reallocate the size that we want.
        MarkStack::releaseStack(m_data, m_allocated);
        m_data = reinterpret_cast<T*>(MarkStack::allocateStack(size));
#else
        MarkStack::releaseStack(reinterpret_cast<char*>(m_data) + size, m_allocated - size);
#endif
        m_allocated = size;
        m_capacity = m_allocated / sizeof(T);
    }

    inline void MarkStack::append(JSValue* slot, size_t count)
    {
        if (!count)
            return;
#if ENABLE(GC_VALIDATION)
        validateSet(slot, count);
#endif
        m_markSets.append(MarkSet(slot, slot + count));
    }
    
    ALWAYS_INLINE void MarkStack::append(JSValue* value)
    {
        ASSERT(value);
        internalAppend(*value);
    }

    ALWAYS_INLINE void MarkStack::append(JSCell** value)
    {
        ASSERT(value);
        internalAppend(*value);
    }

    ALWAYS_INLINE void MarkStack::internalAppend(JSValue value)
    {
        ASSERT(value);
#if ENABLE(GC_VALIDATION)
        validateValue(value);
#endif
        if (value.isCell())
            internalAppend(value.asCell());
    }

    class SlotVisitor;

} // namespace JSC

#endif
