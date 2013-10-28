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

#ifndef WTF_Ref_h
#define WTF_Ref_h

#include "Noncopyable.h"

namespace WTF {

template<typename T> class PassRef;

template<typename T> class Ref {
    WTF_MAKE_NONCOPYABLE(Ref)
public:
    Ref(T& object) : m_ptr(&object) { m_ptr->ref(); }
    template<typename U> Ref(PassRef<U> reference) : m_ptr(&reference.leakRef()) { }

    ~Ref() { m_ptr->deref(); }

    Ref& operator=(T& object)
    {
        object.ref();
        m_ptr->deref();
        m_ptr = &object;
        return *this;
    }
    template<typename U> Ref& operator=(PassRef<U> reference)
    {
        m_ptr->deref();
        m_ptr = &reference.leakRef();
        return *this;
    }

    const T* operator->() const { return m_ptr; }
    T* operator->() { return m_ptr; }

    const T& get() const { return *m_ptr; }
    T& get() { return *m_ptr; }

    template<typename U> PassRef<T> replace(PassRef<U>) WARN_UNUSED_RETURN;

private:
    T* m_ptr;
};

template<typename T> template<typename U> inline PassRef<T> Ref<T>::replace(PassRef<U> reference)
{
    auto oldReference = adoptRef(*m_ptr);
    m_ptr = &reference.leakRef();
    return oldReference;
}

} // namespace WTF

using WTF::Ref;

#endif // WTF_Ref_h
