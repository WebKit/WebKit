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

#include "AuxiliaryBarrier.h"
#include <wtf/CagedPtr.h>

namespace JSC {

class JSCell;
class VM;

// This is a convenient combo of AuxiliaryBarrier and CagedPtr.

template<Gigacage::Kind passedKind, typename T>
class CagedBarrierPtr {
public:
    static constexpr Gigacage::Kind kind = passedKind;
    typedef T Type;
    
    CagedBarrierPtr() { }
    
    template<typename U>
    CagedBarrierPtr(VM& vm, JSCell* cell, U&& value)
    {
        m_barrier.set(vm, cell, std::forward<U>(value));
    }
    
    void clear() { m_barrier.clear(); }
    
    template<typename U>
    void set(VM& vm, JSCell* cell, U&& value)
    {
        m_barrier.set(vm, cell, std::forward<U>(value));
    }
    
    T* get() const { return m_barrier.get().get(); }
    T* getMayBeNull() const { return m_barrier.get().getMayBeNull(); }
    
    bool operator==(const CagedBarrierPtr& other) const
    {
        return getMayBeNull() == other.getMayBeNull();
    }
    
    bool operator!=(const CagedBarrierPtr& other) const
    {
        return !(*this == other);
    }
    
    explicit operator bool() const
    {
        return *this != CagedBarrierPtr();
    }
    
    template<typename U>
    void setWithoutBarrier(U&& value) { m_barrier.setWithoutBarrier(std::forward<U>(value)); }
    
    T& operator*() const { return *get(); }
    T* operator->() const { return get(); }
    
    template<typename IndexType>
    T& operator[](IndexType index) const { return get()[index]; }
    
private:
    AuxiliaryBarrier<CagedPtr<kind, T>> m_barrier;
};

template<Gigacage::Kind passedKind>
class CagedBarrierPtr<passedKind, void> {
public:
    static constexpr Gigacage::Kind kind = passedKind;
    typedef void Type;
    
    CagedBarrierPtr() { }
    
    template<typename U>
    CagedBarrierPtr(VM& vm, JSCell* cell, U&& value)
    {
        m_barrier.set(vm, cell, std::forward<U>(value));
    }
    
    void clear() { m_barrier.clear(); }
    
    template<typename U>
    void set(VM& vm, JSCell* cell, U&& value)
    {
        m_barrier.set(vm, cell, std::forward<U>(value));
    }
    
    void* get() const { return m_barrier.get().get(); }
    void* getMayBeNull() const { return m_barrier.get().getMayBeNull(); }
    
    bool operator==(const CagedBarrierPtr& other) const
    {
        return getMayBeNull() == other.getMayBeNull();
    }
    
    bool operator!=(const CagedBarrierPtr& other) const
    {
        return !(*this == other);
    }
    
    explicit operator bool() const
    {
        return *this != CagedBarrierPtr();
    }
    
    template<typename U>
    void setWithoutBarrier(U&& value) { m_barrier.setWithoutBarrier(std::forward<U>(value)); }
    
private:
    AuxiliaryBarrier<CagedPtr<kind, void>> m_barrier;
};

} // namespace JSC
