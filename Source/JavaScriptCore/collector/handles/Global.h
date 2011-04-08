/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef Global_h
#define Global_h

#include "Assertions.h"
#include "Handle.h"
#include "HandleHeap.h"
#include "JSGlobalData.h"

namespace JSC {

// A Global is a persistent handle whose lifetime is not limited to any given scope.
template <typename T> class Global : public Handle<T> {
    using Handle<T>::slot;
    using Handle<T>::setSlot;

public:
    typedef typename Handle<T>::ExternalType ExternalType;
    
    Global()
        : Handle<T>()
    {
    }
    
    Global(JSGlobalData& globalData, ExternalType value = ExternalType())
        : Handle<T>(globalData.allocateGlobalHandle())
    {
        set(value);
    }

    Global(JSGlobalData& globalData, Handle<T> handle)
        : Handle<T>(globalData.allocateGlobalHandle())
    {
        set(handle.get());
    }
    
    Global(const Global& other)
        : Handle<T>()
    {
        if (!other.slot())
            return;
        setSlot(HandleHeap::heapFor(other.slot())->allocate());
        set(other.get());
    }

    template <typename U> Global(const Global<U>& other)
        : Handle<T>()
    {
        if (!other.slot())
            return;
        setSlot(HandleHeap::heapFor(other.slot())->allocate());
        set(other.get());
    }
    
    enum HashTableDeletedValueTag { HashTableDeletedValue };
    bool isHashTableDeletedValue() const { return slot() == hashTableDeletedValue(); }
    Global(HashTableDeletedValueTag)
        : Handle<T>(hashTableDeletedValue())
    {
    }

    ~Global()
    {
        clear();
    }

    void set(JSGlobalData& globalData, ExternalType value)
    {
        if (!slot())
            setSlot(globalData.allocateGlobalHandle());
        set(value);
    }

    template <typename U> Global& operator=(const Global<U>& other)
    {
        if (!other.slot()) {
            clear();
            return *this;
        }

        set(*HandleHeap::heapFor(other.slot())->globalData(), other.get());
        return *this;
    }
    
    Global& operator=(const Global& other)
    {
        if (!other.slot()) {
            clear();
            return *this;
        }

        set(*HandleHeap::heapFor(other.slot())->globalData(), other.get());
        return *this;
    }

    void clear()
    {
        if (!slot())
            return;
        HandleHeap::heapFor(slot())->deallocate(slot());
        setSlot(0);
    }

private:
    static HandleSlot hashTableDeletedValue() { return reinterpret_cast<HandleSlot>(-1); }

    void set(ExternalType externalType)
    {
        ASSERT(slot());
        JSValue value = HandleTypes<T>::toJSValue(externalType);
        HandleHeap::heapFor(slot())->writeBarrier(slot(), value);
        *slot() = value;
    }
};

}

namespace WTF {

template<typename P> struct HashTraits<JSC::Global<P> > : GenericHashTraits<JSC::Global<P> > {
    static const bool emptyValueIsZero = true;
    static JSC::Global<P> emptyValue() { return JSC::Global<P>(); }
    static void constructDeletedValue(JSC::Global<P>& slot) { new (&slot) JSC::Global<P>(JSC::Global<P>::HashTableDeletedValue); }
    static bool isDeletedValue(const JSC::Global<P>& value) { return value.isHashTableDeletedValue(); }
};

}

#endif // Global_h
