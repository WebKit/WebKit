/*
 * Copyright (C) 2017-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Gigacage.h>
#include <wtf/MathExtras.h>
#include <wtf/PtrTag.h>
#include <wtf/RawPtrTraits.h>

#include <climits>

#if OS(DARWIN)
#include <mach/vm_param.h>
#endif

namespace WTF {

template<Gigacage::Kind passedKind, typename T, typename PtrTraits = RawPtrTraits<T>>
class CagedPtr {
public:
    static constexpr Gigacage::Kind kind = passedKind;
    static constexpr unsigned numberOfPointerBits = sizeof(T*) * CHAR_BIT;
    static constexpr unsigned maxNumberOfAllowedPACBits = numberOfPointerBits - OS_CONSTANT(EFFECTIVE_ADDRESS_WIDTH);
    static constexpr uintptr_t nonPACBitsMask = (1ull << (numberOfPointerBits - maxNumberOfAllowedPACBits)) - 1;

    CagedPtr() : CagedPtr(nullptr) { }
    CagedPtr(std::nullptr_t)
        : m_ptr(nullptr)
    { }

    CagedPtr(T* ptr)
        : m_ptr(ptr)
    { }

    T* get() const
    {
        ASSERT(m_ptr);
        T* ptr = PtrTraits::unwrap(m_ptr);
        return Gigacage::caged(kind, ptr);
    }

    T* getMayBeNull() const
    {
        T* ptr = PtrTraits::unwrap(m_ptr);
        if (!ptr)
            return nullptr;
        return Gigacage::caged(kind, ptr);
    }

    T* getUnsafe() const
    {
        T* ptr = PtrTraits::unwrap(m_ptr);
        return Gigacage::cagedMayBeNull(kind, ptr);
    }

    // We need the template here so that the type of U is deduced at usage time rather than class time. U should always be T.
    template<typename U = T>
    typename std::enable_if<!std::is_same<void, U>::value, T>::type&
    /* T& */ at(size_t index) const { return get()[index]; }

    CagedPtr(CagedPtr& other)
        : m_ptr(other.m_ptr)
    {
    }

    CagedPtr& operator=(const CagedPtr& ptr)
    {
        m_ptr = ptr.m_ptr;
        return *this;
    }

    CagedPtr(CagedPtr&& other)
        : m_ptr(PtrTraits::exchange(other.m_ptr, nullptr))
    {
    }

    CagedPtr& operator=(CagedPtr&& ptr)
    {
        m_ptr = PtrTraits::exchange(ptr.m_ptr, nullptr);
        return *this;
    }

    bool operator==(const CagedPtr& other) const
    {
        bool result = m_ptr == other.m_ptr;
        ASSERT(result == (getUnsafe() == other.getUnsafe()));
        return result;
    }
    
    explicit operator bool() const
    {
        return getUnsafe() != nullptr;
    }

    T* rawBits() const
    {
        return std::bit_cast<T*>(m_ptr);
    }
    
protected:
    typename PtrTraits::StorageType m_ptr;
};

} // namespace WTF

using WTF::CagedPtr;

