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

#ifndef OptionSet_h
#define OptionSet_h

#include <type_traits>

namespace WTF {

template<typename T> class OptionSet {
    static_assert(std::is_enum<T>::value, "T is not an enum type");
    typedef typename std::make_unsigned<typename std::underlying_type<T>::type>::type StorageType;

public:
    static OptionSet fromRaw(StorageType storageType)
    {
        return static_cast<T>(storageType);
    }

    constexpr OptionSet()
        : m_storage(0)
    {
    }

    constexpr OptionSet(T t)
        : m_storage(static_cast<StorageType>(t))
    {
    }

    constexpr StorageType toRaw() const { return m_storage; }

    constexpr bool contains(OptionSet optionSet) const
    {
        return m_storage & optionSet.m_storage;
    }

    friend OptionSet& operator|=(OptionSet& lhs, OptionSet rhs)
    {
        lhs.m_storage |= rhs.m_storage;

        return lhs;
    }

private:
    StorageType m_storage;
};

}

using WTF::OptionSet;

#endif // OptionSet_h
