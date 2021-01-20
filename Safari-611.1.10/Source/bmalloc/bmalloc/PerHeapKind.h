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

#include "HeapKind.h"
#include <array>

namespace bmalloc {

template<typename T>
class PerHeapKindBase {
public:
    PerHeapKindBase(const PerHeapKindBase&) = delete;
    PerHeapKindBase& operator=(const PerHeapKindBase&) = delete;
    
    template<typename... Arguments>
    PerHeapKindBase(Arguments&&... arguments)
    {
        for (unsigned i = numHeaps; i--;)
            new (&at(i)) T(static_cast<HeapKind>(i), std::forward<Arguments>(arguments)...);
    }
    
    static size_t size() { return numHeaps; }
    
    T& at(size_t i)
    {
        return *reinterpret_cast<T*>(&m_memory[i]);
    }
    
    const T& at(size_t i) const
    {
        return *reinterpret_cast<T*>(&m_memory[i]);
    }
    
    T& at(HeapKind heapKind)
    {
        return at(static_cast<size_t>(heapKind));
    }
    
    const T& at(HeapKind heapKind) const
    {
        return at(static_cast<size_t>(heapKind));
    }
    
    T& operator[](size_t i) { return at(i); }
    const T& operator[](size_t i) const { return at(i); }
    T& operator[](HeapKind heapKind) { return at(heapKind); }
    const T& operator[](HeapKind heapKind) const { return at(heapKind); }

private:
    typedef typename std::array<typename std::aligned_storage<sizeof(T), std::alignment_of<T>::value>::type, numHeaps> Memory;
    Memory m_memory;
};

template<typename T>
class StaticPerHeapKind : public PerHeapKindBase<T> {
public:
    template<typename... Arguments>
    StaticPerHeapKind(Arguments&&... arguments)
        : PerHeapKindBase<T>(std::forward<Arguments>(arguments)...)
    {
    }
    
    ~StaticPerHeapKind() = delete;
};

template<typename T>
class PerHeapKind : public PerHeapKindBase<T> {
public:
    template<typename... Arguments>
    PerHeapKind(Arguments&&... arguments)
        : PerHeapKindBase<T>(std::forward<Arguments>(arguments)...)
    {
    }
    
    ~PerHeapKind()
    {
        for (unsigned i = numHeaps; i--;)
            this->at(i).~T();
    }
};

} // namespace bmalloc

