/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef PassWeak_h
#define PassWeak_h

#include "Assertions.h"
#include "Handle.h"
#include "NullPtr.h"
#include "TypeTraits.h"

namespace JSC {

template<typename T> class Weak;
template<typename T> class PassWeak;
template<typename T> PassWeak<T> adoptWeak(HandleSlot);

template<typename T> class PassWeak : public Handle<T> {
    using Handle<T>::slot;
    using Handle<T>::setSlot;

public:
    typedef typename Handle<T>::ExternalType ExternalType;

    PassWeak() : Handle<T>() { }
    PassWeak(std::nullptr_t) : Handle<T>() { }

    PassWeak(JSGlobalData& globalData, ExternalType externalType = ExternalType(), WeakHandleOwner* weakOwner = 0, void* context = 0)
        : Handle<T>(globalData.heap.handleHeap()->allocate())
    {
        HandleHeap::heapFor(slot())->makeWeak(slot(), weakOwner, context);
        JSValue value = HandleTypes<T>::toJSValue(externalType);
        HandleHeap::heapFor(slot())->writeBarrier(slot(), value);
        *slot() = value;
    }

    // It somewhat breaks the type system to allow transfer of ownership out of
    // a const PassWeak. However, it makes it much easier to work with PassWeak
    // temporaries, and we don't have a need to use real const PassWeaks anyway.
    PassWeak(const PassWeak& o) : Handle<T>(o.leakHandle()) { }
    template<typename U> PassWeak(const PassWeak<U>& o) : Handle<T>(o.leakHandle()) { }

    ~PassWeak()
    {
        if (!slot())
            return;
        HandleHeap::heapFor(slot())->deallocate(slot());
        setSlot(0);
    }

    ExternalType get() const { return  HandleTypes<T>::getFromSlot(slot()); }

    HandleSlot leakHandle() const WARN_UNUSED_RETURN;

private:
    friend PassWeak adoptWeak<T>(HandleSlot);

    explicit PassWeak(HandleSlot slot) : Handle<T>(slot) { }
};

template<typename T> inline HandleSlot PassWeak<T>::leakHandle() const
{
    HandleSlot slot = this->slot();
    const_cast<PassWeak<T>*>(this)->setSlot(0);
    return slot;
}

template<typename T> PassWeak<T> adoptWeak(HandleSlot slot)
{
    return PassWeak<T>(slot);
}

template<typename T, typename U> inline bool operator==(const PassWeak<T>& a, const PassWeak<U>& b) 
{
    return a.get() == b.get(); 
}

template<typename T, typename U> inline bool operator==(const PassWeak<T>& a, const Weak<U>& b) 
{
    return a.get() == b.get(); 
}

template<typename T, typename U> inline bool operator==(const Weak<T>& a, const PassWeak<U>& b) 
{
    return a.get() == b.get(); 
}

template<typename T, typename U> inline bool operator==(const PassWeak<T>& a, U* b) 
{
    return a.get() == b; 
}

template<typename T, typename U> inline bool operator==(T* a, const PassWeak<U>& b) 
{
    return a == b.get(); 
}

template<typename T, typename U> inline bool operator!=(const PassWeak<T>& a, const PassWeak<U>& b) 
{
    return a.get() != b.get(); 
}

template<typename T, typename U> inline bool operator!=(const PassWeak<T>& a, const Weak<U>& b) 
{
    return a.get() != b.get(); 
}

template<typename T, typename U> inline bool operator!=(const Weak<T>& a, const PassWeak<U>& b) 
{
    return a.get() != b.get(); 
}

template<typename T, typename U> inline bool operator!=(const PassWeak<T>& a, U* b)
{
    return a.get() != b; 
}

template<typename T, typename U> inline bool operator!=(T* a, const PassWeak<U>& b) 
{
    return a != b.get(); 
}

} // namespace JSC

#endif // PassWeak_h
