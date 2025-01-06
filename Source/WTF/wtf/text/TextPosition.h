/*
 * Copyright (C) 2010, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>
#include <wtf/text/OrdinalNumber.h>

namespace WTF {

// TextPosition structure specifies coordinates within an text resource. It is used mostly
// for saving script source position.
class TextPosition {
    WTF_MAKE_FAST_ALLOCATED;
public:
    TextPosition(OrdinalNumber line, OrdinalNumber column)
        : m_line(line)
        , m_column(column)
    {
    }

    TextPosition() { }
    friend bool operator==(const TextPosition&, const TextPosition&) = default;
    friend std::strong_ordering operator<=>(const TextPosition& a, const TextPosition& b)
    {
        auto lineComparison = a.m_line <=> b.m_line;
        return lineComparison != std::strong_ordering::equal ? lineComparison : a.m_column <=> b.m_column;
    }

    // A value with line value less than a minimum; used as an impossible position.
    static TextPosition belowRangePosition() { return TextPosition(OrdinalNumber::beforeFirst(), OrdinalNumber::beforeFirst()); }

    OrdinalNumber m_line;
    OrdinalNumber m_column;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<TextPosition> {
    static unsigned hash(const TextPosition& key) { return pairIntHash(static_cast<unsigned>(key.m_line.zeroBasedInt()), static_cast<unsigned>(key.m_column.zeroBasedInt())); }
    static bool equal(const TextPosition& a, const TextPosition& b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<TextPosition> : GenericHashTraits<TextPosition> {
    static void constructDeletedValue(TextPosition& slot)
    {
        slot = TextPosition::belowRangePosition();
    }
    static bool isDeletedValue(const TextPosition& value)
    {
        return value == TextPosition::belowRangePosition();
    }
};

}

using WTF::TextPosition;
