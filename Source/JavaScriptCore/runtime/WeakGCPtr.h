/*
 * Copyright (C) 2009 Apple Inc. All rights reserved.
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

#ifndef WeakGCPtr_h
#define WeakGCPtr_h

#include "Global.h"
#include "Heap.h"

namespace JSC {
// A smart pointer whose get() function returns 0 for cells that have died

template <typename T> class WeakGCPtr : public HandleConverter<WeakGCPtr<T>, T> {
    WTF_MAKE_NONCOPYABLE(WeakGCPtr);

public:
    typedef typename HandleTypes<T>::ExternalType ExternalType;
    
    WeakGCPtr()
        : m_slot(0)
    {
    }
    
    WeakGCPtr(JSGlobalData& globalData, Finalizer* finalizer = 0, void* context = 0)
        : m_slot(globalData.allocateGlobalHandle())
    {
        HandleHeap::heapFor(m_slot)->makeWeak(m_slot, finalizer, context);
    }
    
    WeakGCPtr(JSGlobalData& globalData, ExternalType value, Finalizer* finalizer = 0, void* context = 0)
        : m_slot(globalData.allocateGlobalHandle())
    {
        HandleHeap::heapFor(m_slot)->makeWeak(m_slot, finalizer, context);
        internalSet(value);
    }

    ExternalType get() const { return  HandleTypes<T>::getFromSlot(m_slot); }
    
    void clear()
    {
        if (m_slot)
            internalSet(ExternalType());
    }
    
    bool operator!() const { return !m_slot || !*m_slot; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef ExternalType (WeakGCPtr::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const { return !*this ? 0 : reinterpret_cast<UnspecifiedBoolType*>(1); }

    ~WeakGCPtr()
    {
        if (!m_slot)
            return;
        HandleHeap::heapFor(m_slot)->deallocate(m_slot);
    }

    void set(JSGlobalData& globalData, ExternalType value, Finalizer* finalizer)
    {
        if (!this->m_slot) {
            this->m_slot = globalData.allocateGlobalHandle();
            HandleHeap::heapFor(this->m_slot)->makeWeak(this->m_slot, finalizer, 0);
        } else
            ASSERT(HandleHeap::heapFor(this->m_slot)->getFinalizer(this->m_slot) == finalizer);
        this->internalSet(value);
    }

private:
    void internalSet(ExternalType value)
    {
        ASSERT(m_slot);
        JSValue newValue(HandleTypes<T>::toJSValue(value));
        HandleHeap::heapFor(m_slot)->writeBarrier(m_slot, newValue);
        *m_slot = newValue;
    }

    HandleSlot m_slot;
};

} // namespace JSC

#endif // WeakGCPtr_h
