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

#pragma once

#include "DisplayListItems.h"
#include <wtf/FastMalloc.h>
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WTF {
class TextStream;
}

namespace WebCore {

class FloatRect;
class GraphicsContext;

namespace DisplayList {

class Item;

enum AsTextFlag {
    None                            = 0,
    IncludesPlatformOperations      = 1 << 0,
};

typedef unsigned AsTextFlags;

class DisplayList {
    WTF_MAKE_NONCOPYABLE(DisplayList); WTF_MAKE_FAST_ALLOCATED;
    friend class Recorder;
    friend class Replayer;
public:
    DisplayList() = default;
    DisplayList(DisplayList&&) = default;

    DisplayList& operator=(DisplayList&&) = default;

    void dump(WTF::TextStream&) const;

    const Vector<Ref<Item>>& list() const { return m_list; }
    Item& itemAt(size_t index)
    {
        ASSERT(index < m_list.size());
        return m_list[index].get();
    }

    void clear();
    void removeItemsFromIndex(size_t);

    size_t itemCount() const { return m_list.size(); }
    size_t sizeInBytes() const;
    
    String asText(AsTextFlags) const;

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

    // Less efficient append, only used for tracking replay.
    void appendItem(Item& item)
    {
        m_list.append(item);
    }

    static bool shouldDumpForFlags(AsTextFlags, const Item&);

    Vector<Ref<Item>>& list() { return m_list; }

    Vector<Ref<Item>> m_list;
};

} // DisplayList

WTF::TextStream& operator<<(WTF::TextStream&, const DisplayList::DisplayList&);

} // WebCore

using WebCore::DisplayList::DisplayList;

