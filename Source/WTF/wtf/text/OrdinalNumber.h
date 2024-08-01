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

#include <compare>
#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>

namespace WTF {

// An abstract number of element in a sequence. The sequence has a first element.
// This type should be used instead of integer because 2 contradicting traditions can
// call a first element '0' or '1' which makes integer type ambiguous.
class OrdinalNumber {
    WTF_MAKE_FAST_ALLOCATED;
public:
    static OrdinalNumber beforeFirst() { return OrdinalNumber(-1); }
    static OrdinalNumber fromZeroBasedInt(int zeroBasedInt) { return OrdinalNumber(zeroBasedInt); }
    static OrdinalNumber fromOneBasedInt(int oneBasedInt) { return OrdinalNumber(oneBasedInt - 1); }

    OrdinalNumber() : m_zeroBasedValue(0) { }

    int zeroBasedInt() const { return m_zeroBasedValue; }
    int oneBasedInt() const { return m_zeroBasedValue + 1; }

    friend bool operator==(OrdinalNumber, OrdinalNumber) = default;
    friend std::strong_ordering operator<=>(OrdinalNumber, OrdinalNumber) = default;

private:
    OrdinalNumber(int zeroBasedInt) : m_zeroBasedValue(zeroBasedInt) { }
    int m_zeroBasedValue;
};

template<typename T> struct DefaultHash;
template<> struct DefaultHash<OrdinalNumber> {
    static unsigned hash(OrdinalNumber key) { return intHash(static_cast<unsigned>(key.zeroBasedInt())); }
    static bool equal(OrdinalNumber a, OrdinalNumber b) { return a == b; }
    static constexpr bool safeToCompareToEmptyOrDeleted = true;
};

template<typename T> struct HashTraits;
template<> struct HashTraits<OrdinalNumber> : GenericHashTraits<OrdinalNumber> {
    static void constructDeletedValue(OrdinalNumber& slot)
    {
        slot = OrdinalNumber::beforeFirst();
    }
    static bool isDeletedValue(OrdinalNumber value)
    {
        return value == OrdinalNumber::beforeFirst();
    }
};

}

using WTF::OrdinalNumber;
