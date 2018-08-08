/*
 * Copyright (C) 2013-2018 Apple Inc. All rights reserved.
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

#include <objc/runtime.h>
#include <type_traits>
#include <wtf/RetainPtr.h>
#include <wtf/spi/cocoa/objcSPI.h>

namespace WTF {

template<typename T> class WeakObjCPtr {
public:
    using ValueType = typename std::remove_pointer<T>::type;

    WeakObjCPtr() = default;

    WeakObjCPtr(ValueType *ptr)
#if __has_feature(objc_arc)
        : m_weakReference(ptr)
#endif
    {
#if !__has_feature(objc_arc)
        objc_initWeak(&m_weakReference, ptr);
#endif
    }

#if !__has_feature(objc_arc)
    WeakObjCPtr(const WeakObjCPtr& other)
    {
        objc_copyWeak(&m_weakReference, &other.m_weakReference);
    }

    WeakObjCPtr(WeakObjCPtr&& other)
    {
        objc_moveWeak(&m_weakReference, &other.m_weakReference);
    }

    ~WeakObjCPtr()
    {
        objc_destroyWeak(&m_weakReference);
    }
#endif

    WeakObjCPtr& operator=(ValueType *ptr)
    {
#if __has_feature(objc_arc)
        m_weakReference = ptr;
#else
        objc_storeWeak(&m_weakReference, ptr);
#endif

        return *this;
    }

    bool operator!() const
    {
        return !get();
    }

    RetainPtr<ValueType> get() const
    {
#if __has_feature(objc_arc)
        return static_cast<ValueType *>(m_weakReference);
#else
        return adoptNS(objc_loadWeakRetained(&m_weakReference));
#endif
    }

    ValueType *getAutoreleased() const
    {
#if __has_feature(objc_arc)
        return static_cast<ValueType *>(m_weakReference);
#else
        return static_cast<ValueType *>(objc_loadWeak(&m_weakReference));
#endif

    }

    explicit operator ValueType *() const { return getAutoreleased(); }

private:
#if __has_feature(objc_arc)
    mutable __weak id m_weakReference { nullptr };
#else
    mutable id m_weakReference { nullptr };
#endif
};

} // namespace WTF

using WTF::WeakObjCPtr;
