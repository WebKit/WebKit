/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#ifndef CopyWriteBarrier_h
#define CopyWriteBarrier_h

#include "Heap.h"

namespace JSC {

template <typename T>
class CopyWriteBarrier {
public:
    CopyWriteBarrier()
        : m_value(0)
    {
    }
    
    CopyWriteBarrier(VM& vm, const JSCell* owner, T& value)
    {
        this->set(vm, owner, &value);
    }
    
    CopyWriteBarrier(VM& vm, const JSCell* owner, T* value)
    {
        this->set(vm, owner, value);
    }
    
    bool operator!() const { return !m_value; }
    
    typedef T* (CopyWriteBarrier::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return m_value ? reinterpret_cast<UnspecifiedBoolType*>(1) : 0; }
    
    T* get() const
    {
        return m_value;
    }
    
    T* operator*() const
    {
        return get();
    }
    
    T* operator->() const
    {
        return get();
    }
    
    void set(VM&, const JSCell* owner, T* value)
    {
        this->m_value = value;
        Heap::writeBarrier(owner);
    }
    
    void setWithoutWriteBarrier(T* value)
    {
        this->m_value = value;
    }
    
    void clear() { m_value = 0; }

private:
    T* m_value;
};

} // namespace JSC

#endif // CopyWriteBarrier_h
