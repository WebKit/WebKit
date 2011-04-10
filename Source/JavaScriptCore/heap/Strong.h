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

#ifndef Strong_h
#define Strong_h

#include "Assertions.h"
#include "Handle.h"
#include "HandleHeap.h"
#include "JSGlobalData.h"

namespace JSC {

// A strongly referenced handle that prevents the object it points to from being garbage collected.
template <typename T> class Strong : public Handle<T> {
    using Handle<T>::slot;
    using Handle<T>::setSlot;

public:
    typedef typename Handle<T>::ExternalType ExternalType;
    
    Strong()
        : Handle<T>()
    {
    }
    
    Strong(JSGlobalData& globalData, ExternalType value = ExternalType())
        : Handle<T>(globalData.allocateGlobalHandle())
    {
        set(value);
    }

    Strong(JSGlobalData& globalData, Handle<T> handle)
        : Handle<T>(globalData.allocateGlobalHandle())
    {
        set(handle.get());
    }
    
    Strong(const Strong& other)
        : Handle<T>()
    {
        if (!other.slot())
            return;
        setSlot(HandleHeap::heapFor(other.slot())->allocate());
        set(other.get());
    }

    template <typename U> Strong(const Strong<U>& other)
        : Handle<T>()
    {
        if (!other.slot())
            return;
        setSlot(HandleHeap::heapFor(other.slot())->allocate());
        set(other.get());
    }
    
    enum HashTableDeletedValueTag { HashTableDeletedValue };
    bool isHashTableDeletedValue() const { return slot() == hashTableDeletedValue(); }
    Strong(HashTableDeletedValueTag)
        : Handle<T>(hashTableDeletedValue())
    {
    }

    ~Strong()
    {
        clear();
    }

    void set(JSGlobalData& globalData, ExternalType value)
    {
        if (!slot())
            setSlot(globalData.allocateGlobalHandle());
        set(value);
    }

    template <typename U> Strong& operator=(const Strong<U>& other)
    {
        if (!other.slot()) {
            clear();
            return *this;
        }

        set(*HandleHeap::heapFor(other.slot())->globalData(), other.get());
        return *this;
    }
    
    Strong& operator=(const Strong& other)
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

template<typename P> struct HashTraits<JSC::Strong<P> > : GenericHashTraits<JSC::Strong<P> > {
    static const bool emptyValueIsZero = true;
    static JSC::Strong<P> emptyValue() { return JSC::Strong<P>(); }
    static void constructDeletedValue(JSC::Strong<P>& slot) { new (&slot) JSC::Strong<P>(JSC::Strong<P>::HashTableDeletedValue); }
    static bool isDeletedValue(const JSC::Strong<P>& value) { return value.isHashTableDeletedValue(); }
};

}

#endif // Strong_h
