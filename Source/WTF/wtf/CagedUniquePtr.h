/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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

template<Gigacage::Kind kind, typename T, typename Enable = void>
class CagedUniquePtr : public CagedPtr<kind, T> {
public:
    CagedUniquePtr(T* ptr = nullptr)
        : CagedPtr<kind, T>(ptr)
    {
    }
    
    CagedUniquePtr(CagedUniquePtr&& ptr)
        : CagedPtr<kind, T>(ptr.m_ptr)
    {
        ptr.m_ptr = nullptr;
    }
    
    CagedUniquePtr(const CagedUniquePtr&) = delete;
    
    template<typename... Arguments>
    static CagedUniquePtr create(Arguments&&... arguments)
    {
        T* result = static_cast<T*>(Gigacage::malloc(kind, sizeof(T)));
        new (result) T(std::forward<Arguments>(arguments)...);
        return CagedUniquePtr(result);
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
        if (!this->m_ptr)
            return;
        this->m_ptr->~T();
        Gigacage::free(kind, this->m_ptr);
    }
};

template<Gigacage::Kind kind, typename T>
class CagedUniquePtr<kind, T[], typename std::enable_if<std::is_trivially_destructible<T>::value>::type> : public CagedPtr<kind, T> {
public:
    CagedUniquePtr() : CagedPtr<kind, T>() { }
    
    CagedUniquePtr(T* ptr)
        : CagedPtr<kind, T>(ptr)
    {
    }
    
    CagedUniquePtr(CagedUniquePtr&& ptr)
        : CagedPtr<kind, T>(ptr.m_ptr)
    {
        ptr.m_ptr = nullptr;
    }
    
    CagedUniquePtr(const CagedUniquePtr&) = delete;
    
    template<typename... Arguments>
    static CagedUniquePtr create(size_t count, Arguments&&... arguments)
    {
        T* result = static_cast<T*>(Gigacage::mallocArray(kind, count, sizeof(T)));
        while (count--)
            new (result + count) T(std::forward<Arguments>(arguments)...);
        return CagedUniquePtr(result);
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
        if (!this->m_ptr)
            return;
        Gigacage::free(kind, this->m_ptr);
    }
};

template<Gigacage::Kind kind, typename T>
class CagedUniquePtr<kind, T[], typename std::enable_if<!std::is_trivially_destructible<T>::value>::type> : public CagedPtr<kind, T> {
public:
    CagedUniquePtr() : CagedPtr<kind, T>() { }
    
    CagedUniquePtr(T* ptr, size_t count)
        : CagedPtr<kind, T>(ptr)
        , m_count(count)
    {
    }
    
    CagedUniquePtr(CagedUniquePtr&& ptr)
        : CagedPtr<kind, T>(ptr.m_ptr)
        , m_count(ptr.m_count)
    {
        ptr.clear();
    }
    
    CagedUniquePtr(const CagedUniquePtr&) = delete;
    
    template<typename... Arguments>
    static CagedUniquePtr create(size_t count, Arguments&&... arguments)
    {
        T* result = static_cast<T*>(Gigacage::mallocArray(kind, count, sizeof(T)));
        while (count--)
            new (result + count) T(std::forward<Arguments>(arguments)...);
        return CagedUniquePtr(result, count);
    }
    
    CagedUniquePtr& operator=(CagedUniquePtr&& ptr)
    {
        destroy();
        this->m_ptr = ptr.m_ptr;
        m_count = ptr.m_count;
        ptr.clear();
        return *this;
    }
    
    CagedUniquePtr& operator=(const CagedUniquePtr&) = delete;
    
    ~CagedUniquePtr()
    {
        destroy();
    }
    
    // FIXME: It's weird that we inherit CagedPtr::operator== and friends, which don't do anything
    // about m_count. It "works" because pointer equality is enough so long as everything is sane, but
    // it seems like a missed opportunity to assert things.
    // https://bugs.webkit.org/show_bug.cgi?id=175541
    
private:
    void destroy()
    {
        if (!this->m_ptr)
            return;
        while (m_count--)
            this->m_ptr[m_count].~T();
        Gigacage::free(kind, this->m_ptr);
    }
    
    void clear()
    {
        this->m_ptr = nullptr;
        m_count = 0;
    }
    
    size_t m_count { 0 };
};

} // namespace WTF

using WTF::CagedUniquePtr;

