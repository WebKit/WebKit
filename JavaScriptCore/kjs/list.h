/*
 *  Copyright (C) 1999-2001 Harri Porten (porten@kde.org)
 *  Copyright (C) 2003, 2007 Apple Computer, Inc.
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

#ifndef KJS_LIST_H
#define KJS_LIST_H

#include <kjs/value.h>
#include <wtf/HashSet.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>

namespace KJS {

    class JSValue;
    class List;
    
    class List : Noncopyable {
    private:
        typedef Vector<JSValue*, 8> VectorType;
        typedef HashSet<List*> ListSet;

    public:
        typedef VectorType::iterator iterator;
        typedef VectorType::const_iterator const_iterator;

        List()
            : m_isInMarkSet(false)
        {
        }

        ~List()
        {
            if (m_isInMarkSet)
                markSet().remove(this);
        }

        size_t size() const { return m_vector.size(); }
        bool isEmpty() const { return m_vector.isEmpty(); }

        JSValue* at(size_t i) const
        {
            if (i < m_vector.size())
                return m_vector.at(i);
            return jsUndefined();
        }

        JSValue* operator[](int i) const { return at(i); }

        void clear() { m_vector.clear(); }

        void append(JSValue* v)
        {
            if (m_vector.size() < m_vector.capacity())
                m_vector.uncheckedAppend(v);
            else
                // Putting the slow "expand and append" case all in one 
                // function measurably improves the performance of the fast 
                // "just append" case.
                expandAndAppend(v);
        }

        void getSlice(int startIndex, List& result) const;

        iterator begin() { return m_vector.begin(); }
        iterator end() { return m_vector.end(); }

        const_iterator begin() const { return m_vector.begin(); }
        const_iterator end() const { return m_vector.end(); }

        static void markProtectedLists()
        {
            if (!markSet().size())
                return;
            markProtectedListsSlowCase();
        }

    private:
        static ListSet& markSet();
        static void markProtectedListsSlowCase();

        void expandAndAppend(JSValue*);

        VectorType m_vector;
        bool m_isInMarkSet;

    private:
        // Prohibits new / delete, which would break GC.
        void* operator new(size_t);
        void operator delete(void*);

        void* operator new[](size_t);
        void operator delete[](void*);

        void* operator new(size_t, void*);
        void operator delete(void*, size_t);
    };
    
} // namespace KJS

#endif // KJS_LIST_H
