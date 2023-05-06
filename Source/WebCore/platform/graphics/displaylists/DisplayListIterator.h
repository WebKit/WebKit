/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#include "DisplayList.h"

namespace WebCore {
namespace DisplayList {

class DisplayList::Iterator {
public:
    enum class ImmediatelyMoveToEnd : bool { No, Yes };
    Iterator(const DisplayList& displayList, ImmediatelyMoveToEnd immediatelyMoveToEnd = ImmediatelyMoveToEnd::No)
        : m_displayList(displayList)
    {
        if (immediatelyMoveToEnd == ImmediatelyMoveToEnd::Yes)
            moveToEnd();
        else {
            moveCursorToStartOfCurrentBuffer();
            updateCurrentItem();
        }
    }

    ~Iterator()
    {
        clearCurrentItem();
    }

    bool operator==(const Iterator& other) const { return &m_displayList == &other.m_displayList && m_cursor == other.m_cursor; }
    void operator++() { advance(); }

    struct Value {
        ItemHandle item;
        size_t itemSizeInBuffer { 0 };
    };

    std::optional<Value> operator*() const
    {
        if (!m_isValid)
            return std::nullopt;
        return {{
            ItemHandle { m_currentBufferForItem },
            m_currentItemSizeInBuffer,
        }};
    }

private:
    static constexpr size_t sizeOfFixedBufferForCurrentItem = 256;

    WEBCORE_EXPORT void moveCursorToStartOfCurrentBuffer();
    WEBCORE_EXPORT void moveToEnd();
    WEBCORE_EXPORT void clearCurrentItem();
    WEBCORE_EXPORT void updateCurrentItem();
    WEBCORE_EXPORT void advance();

    bool atEnd() const;

    ItemBuffer* itemBuffer() const { return m_displayList.itemBufferIfExists(); }

    const DisplayList& m_displayList;
    uint8_t* m_cursor { nullptr };
    size_t m_readOnlyBufferIndex { 0 };
    uint8_t* m_currentEndOfBuffer { nullptr };

    uint8_t m_fixedBufferForCurrentItem[sizeOfFixedBufferForCurrentItem] { 0 };
    uint8_t* m_currentBufferForItem { nullptr };
    size_t m_currentItemSizeInBuffer { 0 };
    bool m_isValid { true };
};

}
}
