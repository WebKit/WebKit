/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef WeakObjCPtr_h
#define WeakObjCPtr_h

#include <objc/runtime.h>
#include <type_traits>
#include <wtf/RetainPtr.h>

#if __has_include(<objc/objc-internal.h>)
#include <objc/objc-internal.h>
#else
extern "C" {
id objc_loadWeakRetained(id*);
id objc_initWeak(id*, id);
void objc_destroyWeak(id*);
}
#endif

namespace WebKit {

template<typename T> class WeakObjCPtr {
public:
    typedef typename std::remove_pointer<T>::type ValueType;

    WeakObjCPtr()
        : m_weakReference(nullptr)
    {
    }

    WeakObjCPtr(ValueType *ptr)
    {
        objc_initWeak(&m_weakReference, ptr);
    }

    ~WeakObjCPtr()
    {
        objc_destroyWeak(&m_weakReference);
    }

    WeakObjCPtr& operator=(ValueType *ptr)
    {
        objc_storeWeak(&m_weakReference, ptr);

        return *this;
    }

    RetainPtr<ValueType> get() const
    {
        return adoptNS(objc_loadWeakRetained(const_cast<id*>(&m_weakReference)));
    }

    ValueType *getAutoreleased() const
    {
        return static_cast<ValueType *>(objc_loadWeak(const_cast<id*>(&m_weakReference)));
    }

private:
    id m_weakReference;
};

} // namespace WebKit

#endif // WeakObjCPtr_h
