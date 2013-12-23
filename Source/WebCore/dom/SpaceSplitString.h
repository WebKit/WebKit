/*
 * Copyright (C) 2007, 2008, 2010, 2011, 2012, 2013 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef SpaceSplitString_h
#define SpaceSplitString_h

#include <wtf/Assertions.h>
#include <wtf/MainThread.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/AtomicString.h>

namespace WebCore {

class SpaceSplitStringData {
    WTF_MAKE_NONCOPYABLE(SpaceSplitStringData);
    WTF_MAKE_FAST_ALLOCATED;
public:
    static PassRefPtr<SpaceSplitStringData> create(const AtomicString&);

    bool contains(const AtomicString& string)
    {
        const AtomicString* data = tokenArrayStart();
        unsigned i = 0;
        do {
            if (data[i] == string)
                return true;
            ++i;
        } while (i < m_size);
        return false;
    }

    bool containsAll(SpaceSplitStringData&);

    unsigned size() const { return m_size; }
    static ptrdiff_t sizeMemoryOffset() { return OBJECT_OFFSETOF(SpaceSplitStringData, m_size); }

    const AtomicString& operator[](size_t i)
    {
        RELEASE_ASSERT(i < m_size);
        return tokenArrayStart()[i];
    }

    inline void ref()
    {
        ASSERT(isMainThread());
        ASSERT(m_refCount);
        ++m_refCount;
    }

    inline void deref()
    {
        ASSERT(isMainThread());
        ASSERT(m_refCount);
        unsigned tempRefCount = m_refCount - 1;
        if (!tempRefCount) {
            SpaceSplitStringData::destroy(this);
            return;
        }
        m_refCount = tempRefCount;
    }

    static ptrdiff_t tokensMemoryOffset() { return sizeof(SpaceSplitStringData); }

private:
    static PassRefPtr<SpaceSplitStringData> create(const AtomicString&, unsigned tokenCount);
    SpaceSplitStringData(const AtomicString& string, unsigned size)
        : m_keyString(string)
        , m_refCount(1)
        , m_size(size)
    {
        ASSERT(!string.isEmpty());
        ASSERT_WITH_MESSAGE(m_size, "SpaceSplitStringData should never be empty by definition. There is no difference between empty and null.");
    }

    inline ~SpaceSplitStringData() { }
    static void destroy(SpaceSplitStringData*);

    AtomicString* tokenArrayStart() { return reinterpret_cast<AtomicString*>(this + 1); }

    AtomicString m_keyString;
    unsigned m_refCount;
    unsigned m_size;
};

class SpaceSplitString {
public:
    SpaceSplitString() { }
    SpaceSplitString(const AtomicString& string, bool shouldFoldCase) { set(string, shouldFoldCase); }

    bool operator!=(const SpaceSplitString& other) const { return m_data != other.m_data; }

    void set(const AtomicString&, bool shouldFoldCase);
    void clear() { m_data.clear(); }

    bool contains(const AtomicString& string) const { return m_data && m_data->contains(string); }
    bool containsAll(const SpaceSplitString& names) const { return !names.m_data || (m_data && m_data->containsAll(*names.m_data)); }

    size_t size() const { return m_data ? m_data->size() : 0; }
    bool isEmpty() const { return !m_data; }
    const AtomicString& operator[](size_t i) const
    {
        ASSERT_WITH_SECURITY_IMPLICATION(m_data);
        return (*m_data)[i];
    }

    static bool spaceSplitStringContainsValue(const String& spaceSplitString, const char* value, unsigned length, bool shouldFoldCase);
    template<size_t length>
    static bool spaceSplitStringContainsValue(const String& spaceSplitString, const char (&value)[length], bool shouldFoldCase)
    {
        return spaceSplitStringContainsValue(spaceSplitString, value, length - 1, shouldFoldCase);
    }

private:

    RefPtr<SpaceSplitStringData> m_data;
};

} // namespace WebCore

#endif // SpaceSplitString_h
