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

#include <wtf/Assertions.h>
#include "PassWeak.h"
#include "WeakSetInlines.h"

namespace JSC {

template<typename T> class Weak : public WeakImplAccessor<Weak<T>, T> {
    WTF_MAKE_NONCOPYABLE(Weak);
public:
    friend class WeakImplAccessor<Weak<T>, T>;
    typedef typename WeakImplAccessor<Weak<T>, T>::GetType GetType;

    Weak();
    Weak(std::nullptr_t);
    Weak(GetType, WeakHandleOwner* = 0, void* context = 0);

    enum HashTableDeletedValueTag { HashTableDeletedValue };
    bool isHashTableDeletedValue() const;
    Weak(HashTableDeletedValueTag);

    template<typename U> Weak(const PassWeak<U>&);

    ~Weak();

    void swap(Weak&);
    Weak& operator=(const PassWeak<T>&);
    
    bool operator!() const;

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef JSValue (HandleBase::*UnspecifiedBoolType);
    operator UnspecifiedBoolType*() const;

    PassWeak<T> release();
    void clear();
    
private:
    static WeakImpl* hashTableDeletedValue();

    WeakImpl* m_impl;
};

template<typename T> inline Weak<T>::Weak()
    : m_impl(0)
{
}

template<typename T> inline Weak<T>::Weak(std::nullptr_t)
    : m_impl(0)
{
}

template<typename T> inline Weak<T>::Weak(typename Weak<T>::GetType getType, WeakHandleOwner* weakOwner, void* context)
    : m_impl(getType ? WeakSet::allocate(getType, weakOwner, context) : 0)
{
}

template<typename T> inline bool Weak<T>::isHashTableDeletedValue() const
{
    return m_impl == hashTableDeletedValue();
}

template<typename T> inline Weak<T>::Weak(typename Weak<T>::HashTableDeletedValueTag)
    : m_impl(hashTableDeletedValue())
{
}

template<typename T> template<typename U>  inline Weak<T>::Weak(const PassWeak<U>& other)
    : m_impl(other.leakImpl())
{
}

template<typename T> inline Weak<T>::~Weak()
{
    clear();
}

template<class T> inline void swap(Weak<T>& a, Weak<T>& b)
{
    a.swap(b);
}

template<typename T> inline void Weak<T>::swap(Weak& other)
{
    std::swap(m_impl, other.m_impl);
}

template<typename T> inline Weak<T>& Weak<T>::operator=(const PassWeak<T>& o)
{
    clear();
    m_impl = o.leakImpl();
    return *this;
}

template<typename T> inline bool Weak<T>::operator!() const
{
    return !m_impl || !m_impl->jsValue() || m_impl->state() != WeakImpl::Live;
}

template<typename T> inline Weak<T>::operator UnspecifiedBoolType*() const
{
    return reinterpret_cast<UnspecifiedBoolType*>(!!*this);
}

template<typename T> inline PassWeak<T> Weak<T>::release()
{
    PassWeak<T> tmp = adoptWeak<T>(m_impl);
    m_impl = 0;
    return tmp;
}

template<typename T> inline void Weak<T>::clear()
{
    if (!m_impl)
        return;
    WeakSet::deallocate(m_impl);
    m_impl = 0;
}
    
template<typename T> inline WeakImpl* Weak<T>::hashTableDeletedValue()
{
    return reinterpret_cast<WeakImpl*>(-1);
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

    typedef typename StorageType::GetType PeekType;
    static PeekType peek(const StorageType& value) { return value.get(); }
    static PeekType peek(EmptyValueType) { return PeekType(); }
};

}

#endif // Weak_h
