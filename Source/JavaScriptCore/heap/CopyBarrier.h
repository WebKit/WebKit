/*
 * Copyright (C) 2014-2016 Apple Inc. All rights reserved.
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

#ifndef CopyBarrier_h
#define CopyBarrier_h

#include "Heap.h"

namespace JSC {

class CopyBarrierBase {
public:
    CopyBarrierBase()
        : m_value(nullptr)
    {
    }
    
    explicit operator bool() const { return m_value; }

    void* get() const
    {
        return m_value;
    }

    void clear() { m_value = nullptr; }

protected:
    CopyBarrierBase(VM& vm, const JSCell* owner, void* value)
    {
        this->set(vm, owner, value);
    }
    
    void set(VM& vm, const JSCell* owner, void* value)
    {
        this->m_value = value;
        vm.heap.writeBarrier(owner);
    }
    
    void setWithoutBarrier(void* value)
    {
        this->m_value = value;
    }

private:
    mutable void* m_value;
};

template <typename T>
class CopyBarrier : public CopyBarrierBase {
public:
    CopyBarrier()
    {
    }
    
    CopyBarrier(VM& vm, const JSCell* owner, T& value)
        : CopyBarrierBase(vm, owner, &value)
    {
    }
    
    CopyBarrier(VM& vm, const JSCell* owner, T* value)
        : CopyBarrierBase(vm, owner, value)
    {
    }

    T* get() const
    {
        return bitwise_cast<T*>(CopyBarrierBase::get());
    }

    void set(VM& vm, const JSCell* owner, T* value)
    {
        CopyBarrierBase::set(vm, owner, value);
    }
    
    void setWithoutBarrier(T* value)
    {
        CopyBarrierBase::setWithoutBarrier(value);
    }
};

} // namespace JSC

#endif // CopyBarrier_h
