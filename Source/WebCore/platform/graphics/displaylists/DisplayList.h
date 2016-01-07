/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#ifndef DisplayList_h
#define DisplayList_h

#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class FloatRect;
class GraphicsContext;
class TextStream;

namespace DisplayList {

class Item;

class DisplayList {
    WTF_MAKE_NONCOPYABLE(DisplayList);
    friend class Recorder;
public:
    DisplayList() = default;
    DisplayList(DisplayList&&) = default;

    DisplayList& operator=(DisplayList&&) = default;

    void dump(TextStream&) const;

    const Vector<Ref<Item>>& list() const { return m_list; }
    Item& itemAt(size_t index)
    {
        ASSERT(index < m_list.size());
        return m_list[index].get();
    }
    
    void clear() { m_list.clear(); }
    size_t size() const { return m_list.size(); }

    void removeItemsFromIndex(size_t index)
    {
        m_list.resize(index);
    }

    size_t sizeInBytes() const;

#if !defined(NDEBUG) || !LOG_DISABLED
    WTF::CString description() const;
    void dump() const;
#endif

private:
    Item& append(Ref<Item>&& item)
    {
        m_list.append(WTFMove(item));
        return m_list.last().get();
    }

    Vector<Ref<Item>>& list() { return m_list; }

    Vector<Ref<Item>> m_list;
};

} // DisplayList

TextStream& operator<<(TextStream&, const DisplayList::DisplayList&);

} // WebCore

using WebCore::DisplayList::DisplayList;

#endif /* DisplayList_h */
