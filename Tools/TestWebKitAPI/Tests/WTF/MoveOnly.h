/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef MoveOnly_h
#define MoveOnly_h

#include <wtf/HashFunctions.h>
#include <wtf/HashTraits.h>

class MoveOnly {
public:
    MoveOnly()
        : m_value(0)
    {
    }

    MoveOnly(unsigned value)
        : m_value(value)
    {
    }

    unsigned value() const
    {
        return m_value;
    }

    unsigned operator()() const
    {
        return m_value;
    }

    MoveOnly(MoveOnly&& other)
        : m_value(other.m_value)
    {
        other.m_value = 0;
    }

    MoveOnly& operator=(MoveOnly&& other)
    {
        if (this == &other)
            return *this;

        m_value = other.m_value;
        other.m_value = 0;
        return *this;
    }

    friend bool operator==(const MoveOnly& a, const MoveOnly& b)
    {
        return a.m_value == b.m_value;
    }

private:
    unsigned m_value;
};

namespace WTF {

template<> struct HashTraits<MoveOnly> : public GenericHashTraits<MoveOnly> {
    static void constructDeletedValue(MoveOnly& slot) { slot = MoveOnly(std::numeric_limits<unsigned>::max()); }
    static bool isDeletedValue(const MoveOnly& slot) { return slot.value() == std::numeric_limits<unsigned>::max(); }
};

template<> struct DefaultHash<MoveOnly> {
    static unsigned hash(const MoveOnly& key) 
    {
        return intHash(key.value());
    }

    static bool equal(const MoveOnly& a, const MoveOnly& b)
    {
        return a == b;
    }

    static const bool safeToCompareToEmptyOrDeleted = true;
};
} // namespace WTF

#endif // MoveOnly_h
