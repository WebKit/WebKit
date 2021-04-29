/*
 * Copyright (C) 2017-2020 Apple Inc. All rights reserved.
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

#include <wtf/CagedPtr.h>

namespace WTF {

template<Gigacage::Kind kind, typename T, bool shouldTag = false>
class CagedUniquePtr : public CagedPtr<kind, T, shouldTag> {
    static_assert(std::is_trivially_destructible<T>::value, "We expect the contents of a caged pointer to be trivially destructable.");
public:
    using Base = CagedPtr<kind, T, shouldTag>;
    CagedUniquePtr() = default;

    CagedUniquePtr(T* ptr, size_t size)
        : Base(ptr, size)
    { }

    CagedUniquePtr(CagedUniquePtr&& ptr)
        : Base(std::forward<CagedUniquePtr&&>(ptr))
    { }
    
    CagedUniquePtr(const CagedUniquePtr&) = delete;
    
    template<typename... Arguments>
    static CagedUniquePtr create(size_t length, Arguments&&... arguments)
    {
        T* result = static_cast<T*>(Gigacage::malloc(kind, sizeof(T) * length));
        while (length--)
            new (result + length) T(arguments...);
        return CagedUniquePtr(result, length);
    }

    template<typename... Arguments>
    static CagedUniquePtr tryCreate(size_t length, Arguments&&... arguments)
    {
        T* result = static_cast<T*>(Gigacage::tryMalloc(kind, sizeof(T) * length));
        if (!result)
            return { };
        while (length--)
            new (result + length) T(arguments...);
        return CagedUniquePtr(result, length);
    }

    CagedUniquePtr& operator=(CagedUniquePtr&& ptr)
    {
        destroy();
        this->m_ptr = ptr.m_ptr;
        ptr.m_ptr = nullptr;
        return *this;
    }
    
    CagedUniquePtr& operator=(const CagedUniquePtr&) = delete;
    
    ~CagedUniquePtr()
    {
        destroy();
    }

private:
    void destroy()
    {
        T* ptr = Base::getUnsafe();
        if (!ptr)
            return;
        ptr->~T();
        Gigacage::free(kind, ptr);
    }
};

} // namespace WTF

using WTF::CagedUniquePtr;

