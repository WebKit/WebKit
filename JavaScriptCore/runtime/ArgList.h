/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007, 2008 Apple Computer, Inc.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef ArgList_h
#define ArgList_h

#include "JSImmediate.h"
#include "Register.h"

#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace JSC {
    
    class ArgList : Noncopyable {
    private:
        static const unsigned inlineCapacity = 8;
        typedef Vector<Register, inlineCapacity> VectorType;
        typedef HashSet<ArgList*> ListSet;

    public:
        typedef VectorType::iterator iterator;
        typedef VectorType::const_iterator const_iterator;

        // Constructor for a read-write list, to which you may append values.
        // FIXME: Remove all clients of this API, then remove this API.
        ArgList()
            : m_markSet(0)
#ifndef NDEBUG
            , m_isReadOnly(false)
#endif
        {
            m_buffer = m_vector.data();
            m_size = 0;
        }

        // Constructor for a read-only list whose data has already been allocated elsewhere.
        ArgList(Register* buffer, size_t size)
            : m_buffer(buffer)
            , m_size(size)
            , m_markSet(0)
#ifndef NDEBUG
            , m_isReadOnly(true)
#endif
        {
        }

        void initialize(Register* buffer, size_t size)
        {
            ASSERT(!m_markSet);
            ASSERT(isEmpty());

            m_buffer = buffer;
            m_size = size;
#ifndef NDEBUG
            m_isReadOnly = true;
#endif
        }

        ~ArgList()
        {
            if (m_markSet)
                m_markSet->remove(this);
        }

        size_t size() const { return m_size; }
        bool isEmpty() const { return !m_size; }

        JSValue* at(ExecState* exec, size_t i) const
        {
            if (i < m_size)
                return m_buffer[i].jsValue(exec);
            return jsUndefined();
        }

        void clear()
        {
            m_vector.clear();
            m_buffer = 0;
            m_size = 0;
        }

        void append(JSValue* v)
        {
            ASSERT(!m_isReadOnly);
            
            if (m_size < inlineCapacity) {
                m_vector.uncheckedAppend(v);
                ++m_size;
            } else {
                // Putting this case all in one function measurably improves
                // the performance of the fast "just append to inline buffer" case.
                slowAppend(v);
                ++m_size;
            }
        }

        void getSlice(int startIndex, ArgList& result) const;

        iterator begin() { return m_buffer; }
        iterator end() { return m_buffer + m_size; }

        const_iterator begin() const { return m_buffer; }
        const_iterator end() const { return m_buffer + m_size; }

        static void markLists(ListSet&);

    private:
        void slowAppend(JSValue*);
        
        Register* m_buffer;
        size_t m_size;

        VectorType m_vector;
        ListSet* m_markSet;
#ifndef NDEBUG
        bool m_isReadOnly;
#endif

    private:
        // Prohibits new / delete, which would break GC.
        friend class JSGlobalData;
        
        void* operator new(size_t size)
        {
            return fastMalloc(size);
        }
        void operator delete(void* p)
        {
            fastFree(p);
        }

        void* operator new[](size_t);
        void operator delete[](void*);

        void* operator new(size_t, void*);
        void operator delete(void*, size_t);
    };

} // namespace JSC

#endif // ArgList_h
