/*
 *  Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef WTF_PassRef_h
#define WTF_PassRef_h

#include "Assertions.h"
#include <cstddef>
#include <utility>

namespace WTF {

template<typename T> class PassRef;
template<typename T> class PassRefPtr;
template<typename T> class Ref;
template<typename T> class RefPtr;

template<typename T> PassRef<T> adoptRef(T&);

inline void adopted(const void*) { }

template<typename T> class PassRef {
public:
    PassRef(T&);
    PassRef(PassRef&&);
    template<typename U> PassRef(PassRef<U>);

    const T& get() const;
    T& get();

    void dropRef();
    T& leakRef() WARN_UNUSED_RETURN;

#ifndef NDEBUG
    ~PassRef();
#endif

private:
    friend PassRef adoptRef<T>(T&);

    template<typename U> friend class PassRef;
    template<typename U> friend class PassRefPtr;
    template<typename U> friend class Ref;
    template<typename U> friend class RefPtr;

    enum AdoptTag { Adopt };
    PassRef(T&, AdoptTag);

    T& m_reference;

#ifndef NDEBUG
    bool m_gaveUpReference;
#endif
};

template<typename T> inline PassRef<T>::PassRef(T& reference)
    : m_reference(reference)
#ifndef NDEBUG
    , m_gaveUpReference(false)
#endif
{
    reference.ref();
}

template<typename T> inline PassRef<T>::PassRef(PassRef&& other)
    : m_reference(other.leakRef())
#ifndef NDEBUG
    , m_gaveUpReference(false)
#endif
{
}

template<typename T> template<typename U> inline PassRef<T>::PassRef(PassRef<U> other)
    : m_reference(other.leakRef())
#ifndef NDEBUG
    , m_gaveUpReference(false)
#endif
{
}

#ifndef NDEBUG

template<typename T> PassRef<T>::~PassRef()
{
    ASSERT(m_gaveUpReference);
}

#endif

template<typename T> inline void PassRef<T>::dropRef()
{
    ASSERT(!m_gaveUpReference);
    m_reference.deref();
#ifndef NDEBUG
    m_gaveUpReference = true;
#endif
}

template<typename T> inline const T& PassRef<T>::get() const
{
    ASSERT(!m_gaveUpReference);
    return m_reference;
}

template<typename T> inline T& PassRef<T>::get()
{
    ASSERT(!m_gaveUpReference);
    return m_reference;
}

template<typename T> inline T& PassRef<T>::leakRef()
{
#ifndef NDEBUG
    ASSERT(!m_gaveUpReference);
    m_gaveUpReference = true;
#endif
    return m_reference;
}

template<typename T> inline PassRef<T>::PassRef(T& reference, AdoptTag)
    : m_reference(reference)
#ifndef NDEBUG
    , m_gaveUpReference(false)
#endif
{
}

template<typename T> inline PassRef<T> adoptRef(T& reference)
{
    adopted(&reference);
    return PassRef<T>(reference, PassRef<T>::Adopt);
}

#if COMPILER_SUPPORTS(CXX_VARIADIC_TEMPLATES)

template<typename T, typename... Args> inline PassRef<T> createRefCounted(Args&&... args)
{
    return adoptRef(*new T(std::forward<Args>(args)...));
}

#else

template<typename T> inline PassRef<T> createRefCounted()
{
    return adoptRef(*new T);
}

template<typename T, typename A1> inline PassRef<T> createRefCounted(A1&& a1)
{
    return adoptRef(*new T(std::forward<A1>(a1)));
}

template<typename T, typename A1, typename A2> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2)));
}

template<typename T, typename A1, typename A2, typename A3> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3, A4&& a4)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6, A7&& a7)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6), std::forward<A7>(a7)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6, A7&& a7, A8&& a8)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6), std::forward<A7>(a7), std::forward<A8>(a8)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6, A7&& a7, A8&& a8, A9&& a9)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6), std::forward<A7>(a7), std::forward<A8>(a8), std::forward<A9>(a9)));
}

template<typename T, typename A1, typename A2, typename A3, typename A4, typename A5, typename A6, typename A7, typename A8, typename A9, typename A10> inline PassRef<T> createRefCounted(A1&& a1, A2&& a2, A3&& a3, A4&& a4, A5&& a5, A6&& a6, A7&& a7, A8&& a8, A9&& a9, A10&& a10)
{
    return adoptRef(*new T(std::forward<A1>(a1), std::forward<A2>(a2), std::forward<A3>(a3), std::forward<A4>(a4), std::forward<A5>(a5), std::forward<A6>(a6), std::forward<A7>(a7), std::forward<A8>(a8), std::forward<A9>(a9), std::forward<A10>(a10)));
}

#endif

} // namespace WTF

using WTF::PassRef;
using WTF::adoptRef;
using WTF::createRefCounted;

#endif // WTF_PassRef_h
