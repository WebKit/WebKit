/*
 * Copyright (C) 2009, 2012 Apple Inc. All rights reserved.
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

#ifndef Weak_h
#define Weak_h

#include "Assertions.h"
#include "Handle.h"
#include "HandleHeap.h"
#include "JSGlobalData.h"
#include "PassWeak.h"

namespace JSC {

// A weakly referenced handle that becomes 0 when the value it points to is garbage collected.
template <typename T> class Weak : public Handle<T> {
    WTF_MAKE_NONCOPYABLE(Weak);

    using Handle<T>::slot;
    using Handle<T>::setSlot;

public:
    typedef typename Handle<T>::ExternalType ExternalType;

    Weak()
        : Handle<T>()
    {
    }

    Weak(std::nullptr_t)
        : Handle<T>()
    {
    }

    Weak(JSGlobalData& globalData, ExternalType externalType = ExternalType(), WeakHandleOwner* weakOwner = 0, void* context = 0)
        : Handle<T>(globalData.heap.handleHeap()->allocate())
    {
        HandleHeap::heapFor(slot())->makeWeak(slot(), weakOwner, context);
        JSValue value = HandleTypes<T>::toJSValue(externalType);
        HandleHeap::heapFor(slot())->writeBarrier(slot(), value);
        *slot() = value;
    }

    enum AdoptTag { Adopt };
    template<typename U> Weak(AdoptTag, Handle<U> handle)
        : Handle<T>(handle.slot())
    {
        validateCell(get());
    }

    enum HashTableDeletedValueTag { HashTableDeletedValue };
    bool isHashTableDeletedValue() const { return slot() == hashTableDeletedValue(); }
    Weak(HashTableDeletedValueTag)
        : Handle<T>(hashTableDeletedValue())
    {
    }

    template<typename U> Weak(const PassWeak<U>& other)
        : Handle<T>(other.leakHandle())
    {
    }

    ~Weak()
    {
        clear();
    }

    void swap(Weak& other)
    {
        Handle<T>::swap(other);
    }

    Weak& operator=(const PassWeak<T>&);

    ExternalType get() const { return  HandleTypes<T>::getFromSlot(slot()); }
    
    PassWeak<T> release() { PassWeak<T> tmp = adoptWeak<T>(slot()); setSlot(0); return tmp; }

    void clear()
    {
        if (!slot())
            return;
        HandleHeap::heapFor(slot())->deallocate(slot());
        setSlot(0);
    }
    
    HandleSlot leakHandle()
    {
        ASSERT(HandleHeap::heapFor(slot())->hasFinalizer(slot()));
        HandleSlot result = slot();
        setSlot(0);
        return result;
    }

private:
    static HandleSlot hashTableDeletedValue() { return reinterpret_cast<HandleSlot>(-1); }
};

template<class T> inline void swap(Weak<T>& a, Weak<T>& b)
{
    a.swap(b);
}

template<typename T> inline Weak<T>& Weak<T>::operator=(const PassWeak<T>& o)
{
    clear();
    setSlot(o.leakHandle());
    return *this;
}

} // namespace JSC

namespace WTF {

template<typename T> struct VectorTraits<JSC::Weak<T> > : SimpleClassVectorTraits {
    static const bool canCompareWithMemcmp = false;
};

template<typename T> struct HashTraits<JSC::Weak<T> > : SimpleClassHashTraits<JSC::Weak<T> > {
    typedef JSC::Weak<T> StorageType;

    typedef std::nullptr_t EmptyValueType;
    static EmptyValueType emptyValue() { return nullptr; }

    typedef JSC::PassWeak<T> PassInType;
    static void store(PassInType value, StorageType& storage) { storage = value; }

    typedef JSC::PassWeak<T> PassOutType;
    static PassOutType passOut(StorageType& value) { return value.release(); }
    static PassOutType passOut(EmptyValueType) { return PassOutType(); }

    typedef typename StorageType::ExternalType PeekType;
    static PeekType peek(const StorageType& value) { return value.get(); }
    static PeekType peek(EmptyValueType) { return PeekType(); }
};

}

#endif // Weak_h
