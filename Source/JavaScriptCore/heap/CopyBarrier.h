/*
 * Copyright (C) 2014, 2015 Apple Inc. All rights reserved.
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

enum class CopyState {
    // The backing store is not planned to get copied in this epoch. If you keep a pointer to the backing
    // store on the stack, it will not get copied. If you don't keep it on the stack, it may get copied
    // starting at the next handshake (that is, it may transition from ToSpace to CopyPlanned, but
    // CopyPlanned means ToSpace prior to the handshake that starts the copy phase).
    ToSpace,

    // The marking phase has selected this backing store to be copied. If we are not yet in the copying
    // phase, this backing store is still in to-space. All that is needed in such a case is to mask off the
    // low bits. If we are in the copying phase, this means that the object points to from-space. The
    // barrier should first copy the object - or wait for copying to finish - before using the object.
    CopyPlanned,

    // The object is being copied right now. Anyone wanting to use the object must wait for the object to
    // finish being copied. Notifications about copying use the ParkingLot combined with these bits. If the
    // state is CopyingAndWaiting, then when the copying finishes, whatever thread was doing it will
    // unparkAll() on the address of the CopyBarrierBase. So, to wait for copying to finish, CAS this to
    // CopyingAndWaiting and then parkConditionally on the barrier address.
    Copying,

    // The object is being copied right now, and there are one or more threads parked. Those threads want
    // to be unparked when copying is done. So, whichever thread does the copying needs to call unparkAll()
    // on the barrier address after copying is done.
    CopyingAndWaiting
};

class CopyBarrierBase {
public:
    static const unsigned spaceBits = 3;

    CopyBarrierBase()
        : m_value(nullptr)
    {
    }
    
    bool operator!() const { return !m_value; }
    
    explicit operator bool() const { return m_value; }

    void* getWithoutBarrier() const
    {
        return m_value;
    }

    // Use this version of get() if you only want to execute the barrier slow path if some condition
    // holds, and you only want to evaluate that condition after first checking the barrier's
    // condition. Usually, you just want to use get().
    template<typename Functor>
    void* getPredicated(const JSCell* owner, const Functor& functor) const
    {
        void* result = m_value;
        if (UNLIKELY(bitwise_cast<uintptr_t>(result) & spaceBits)) {
            if (functor())
                return Heap::copyBarrier(owner, m_value);
        }
        return result;
    }

    // When we are in the concurrent copying phase, this method may lock the barrier object (i.e. the field
    // pointing to copied space) and call directly into the owning object's copyBackingStore() method.
    void* get(const JSCell* owner) const
    {
        return getPredicated(owner, [] () -> bool { return true; });
    }

    CopyState copyState() const
    {
        return static_cast<CopyState>(bitwise_cast<uintptr_t>(m_value) & spaceBits);
    }

    // This only works when you know that there is nobody else concurrently messing with this CopyBarrier.
    // That's hard to guarantee, though there are a few unusual places where this ends up being safe.
    // Usually you want to use CopyBarrier::weakCAS().
    void setCopyState(CopyState copyState)
    {
        WTF::storeStoreFence();
        uintptr_t value = bitwise_cast<uintptr_t>(m_value);
        value &= ~static_cast<uintptr_t>(spaceBits);
        value |= static_cast<uintptr_t>(copyState);
        m_value = bitwise_cast<void*>(value);
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

    bool weakCASWithoutBarrier(
        void* oldPointer, CopyState oldCopyState, void* newPointer, CopyState newCopyState)
    {
        uintptr_t oldValue = bitwise_cast<uintptr_t>(oldPointer) | static_cast<uintptr_t>(oldCopyState);
        uintptr_t newValue = bitwise_cast<uintptr_t>(newPointer) | static_cast<uintptr_t>(newCopyState);
        return WTF::weakCompareAndSwap(
            &m_value, bitwise_cast<void*>(oldValue), bitwise_cast<void*>(newValue));
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

    T* getWithoutBarrier() const
    {
        return bitwise_cast<T*>(CopyBarrierBase::getWithoutBarrier());
    }
    
    T* get(const JSCell* owner) const
    {
        return bitwise_cast<T*>(CopyBarrierBase::get(owner));
    }

    template<typename Functor>
    T* getPredicated(const JSCell* owner, const Functor& functor) const
    {
        return bitwise_cast<T*>(CopyBarrierBase::getPredicated(owner, functor));
    }
    
    void set(VM& vm, const JSCell* owner, T* value)
    {
        CopyBarrierBase::set(vm, owner, value);
    }
    
    void setWithoutBarrier(T* value)
    {
        CopyBarrierBase::setWithoutBarrier(value);
    }

    bool weakCASWithoutBarrier(T* oldPointer, CopyState oldCopyState, T* newPointer, CopyState newCopyState)
    {
        return CopyBarrierBase::weakCASWithoutBarrier(oldPointer, oldCopyState, newPointer, newCopyState);
    }
};

} // namespace JSC

#endif // CopyBarrier_h
