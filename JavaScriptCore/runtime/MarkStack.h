/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#include "JSValue.h"
#include <wtf/Noncopyable.h>

namespace JSC {

    class JSGlobalData;
    class Register;
    
    enum MarkSetProperties { MayContainNullValues, NoNullValues };
    
    class MarkStack : Noncopyable {
    public:
        MarkStack(void* jsArrayVPtr)
            : m_jsArrayVPtr(jsArrayVPtr)
#ifndef NDEBUG
            , m_isCheckingForDefaultMarkViolation(false)
#endif
        {
        }

        ALWAYS_INLINE void append(JSValue);
        ALWAYS_INLINE void append(JSCell*);
        
        ALWAYS_INLINE void appendValues(Register* values, size_t count, MarkSetProperties properties = NoNullValues)
        {
            appendValues(reinterpret_cast<JSValue*>(values), count, properties);
        }

        ALWAYS_INLINE void appendValues(JSValue* values, size_t count, MarkSetProperties properties = NoNullValues)
        {
            if (count)
                m_markSets.append(MarkSet(values, values + count, properties));
        }

        inline void drain();
        void compact();

        ~MarkStack()
        {
            ASSERT(m_markSets.isEmpty());
            ASSERT(m_values.isEmpty());
        }

    private:
        void markChildren(JSCell*);

        struct MarkSet {
            MarkSet(JSValue* values, JSValue* end, MarkSetProperties properties)
                : m_values(values)
                , m_end(end)
                , m_properties(properties)
            {
                ASSERT(values);
            }
            JSValue* m_values;
            JSValue* m_end;
            MarkSetProperties m_properties;
        };

        static void* allocateStack(size_t size);
        static void releaseStack(void* addr, size_t size);

        static void initializePagesize();
        static size_t pageSize()
        {
            if (!s_pageSize)
                initializePagesize();
            return s_pageSize;
        }

        template <typename T> struct MarkStackArray {
            MarkStackArray()
                : m_top(0)
                , m_allocated(MarkStack::pageSize())
                , m_capacity(m_allocated / sizeof(T))
            {
                m_data = reinterpret_cast<T*>(allocateStack(m_allocated));
            }

            ~MarkStackArray()
            {
                releaseStack(m_data, m_allocated);
            }

            void expand()
            {
                size_t oldAllocation = m_allocated;
                m_allocated *= 2;
                m_capacity = m_allocated / sizeof(T);
                void* newData = allocateStack(m_allocated);
                memcpy(newData, m_data, oldAllocation);
                releaseStack(m_data, oldAllocation);
                m_data = reinterpret_cast<T*>(newData);
            }

            inline void append(const T& v)
            {
                if (m_top == m_capacity)
                    expand();
                m_data[m_top++] = v;
            }

            inline T removeLast()
            {
                ASSERT(m_top);
                return m_data[--m_top];
            }
            
            inline T& last()
            {
                ASSERT(m_top);
                return m_data[m_top - 1];
            }

            inline bool isEmpty()
            {
                return m_top == 0;
            }

            inline size_t size() { return m_top; }

            inline void shrinkAllocation(size_t size)
            {
                ASSERT(size <= m_allocated);
                ASSERT(0 == (size % MarkStack::pageSize()));
                if (size == m_allocated)
                    return;
#if PLATFORM(WIN) || PLATFORM(SYMBIAN)
                // We cannot release a part of a region with VirtualFree.  To get around this,
                // we'll release the entire region and reallocate the size that we want.
                releaseStack(m_data, m_allocated);
                m_data = reinterpret_cast<T*>(allocateStack(size));
#else
                releaseStack(reinterpret_cast<char*>(m_data) + size, m_allocated - size);
#endif
                m_allocated = size;
                m_capacity = m_allocated / sizeof(T);
            }

        private:
            size_t m_top;
            size_t m_allocated;
            size_t m_capacity;
            T* m_data;
        };

        void* m_jsArrayVPtr;
        MarkStackArray<MarkSet> m_markSets;
        MarkStackArray<JSCell*> m_values;
        static size_t s_pageSize;

#ifndef NDEBUG
    public:
        bool m_isCheckingForDefaultMarkViolation;
#endif
    };
}

#endif
