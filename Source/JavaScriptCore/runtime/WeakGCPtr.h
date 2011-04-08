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

#include "Assertions.h"
#include "Handle.h"
#include "HandleHeap.h"
#include "JSGlobalData.h"

namespace JSC {

// A smart pointer that becomes 0 when the value it points to is garbage collected.
template <typename T> class WeakGCPtr : public Handle<T> {
    using Handle<T>::slot;
    using Handle<T>::setSlot;

public:
    typedef typename Handle<T>::ExternalType ExternalType;

    WeakGCPtr()
        : Handle<T>()
    {
    }

    WeakGCPtr(JSGlobalData& globalData, ExternalType value = ExternalType(), WeakHandleOwner* weakOwner = 0, void* context = 0)
        : Handle<T>(globalData.allocateGlobalHandle())
    {
        HandleHeap::heapFor(slot())->makeWeak(slot(), weakOwner, context);
        set(value);
    }

    WeakGCPtr(const WeakGCPtr& other)
        : Handle<T>()
    {
        if (!other.slot())
            return;
        setSlot(HandleHeap::heapFor(other.slot())->allocate());
        set(other.get());
    }

    template <typename U> WeakGCPtr(const WeakGCPtr<U>& other)
        : Handle<T>()
    {
        if (!other.slot())
            return;
        setSlot(HandleHeap::heapFor(other.slot())->allocate());
        set(other.get());
    }
    
    ~WeakGCPtr()
    {
        clear();
    }

    ExternalType get() const { return  HandleTypes<T>::getFromSlot(slot()); }
    
    void clear()
    {
        if (!slot())
            return;
        HandleHeap::heapFor(slot())->deallocate(slot());
        setSlot(0);
    }
    
    void set(JSGlobalData& globalData, ExternalType value, WeakHandleOwner* weakOwner = 0, void* context = 0)
    {
        if (!slot()) {
            setSlot(globalData.allocateGlobalHandle());
            HandleHeap::heapFor(slot())->makeWeak(slot(), weakOwner, context);
        }
        ASSERT(HandleHeap::heapFor(slot())->hasWeakOwner(slot(), weakOwner));
        set(value);
    }

private:
    void set(ExternalType externalType)
    {
        ASSERT(slot());
        JSValue value = HandleTypes<T>::toJSValue(externalType);
        HandleHeap::heapFor(slot())->writeBarrier(slot(), value);
        *slot() = value;
    }
};

} // namespace JSC

#endif // WeakGCPtr_h
