/*
 *  Copyright (C) 2005-2021 Apple Inc. All rights reserved.
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

#pragma once

#include <wtf/Platform.h>

#if USE(CF) || defined(__OBJC__)

#include <algorithm>
#include <cstddef>
#include <wtf/HashTraits.h>
#include <wtf/NeverDestroyed.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#endif

#ifdef __OBJC__
#import <Foundation/Foundation.h>
#endif

#ifndef CF_BRIDGED_TYPE
#define CF_BRIDGED_TYPE(T)
#endif

#ifndef CF_RELEASES_ARGUMENT
#define CF_RELEASES_ARGUMENT
#endif

#ifndef NS_RELEASES_ARGUMENT
#define NS_RELEASES_ARGUMENT
#endif

#ifndef __OBJC__
typedef struct objc_object *id;
#endif

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define adoptNS adoptNSArc
#endif

namespace WTF {

// Unlike most most of our smart pointers, RetainPtr can take either the pointer type or the pointed-to type,
// so both RetainPtr<NSDictionary> and RetainPtr<CFDictionaryRef> will work.

template<typename T> class RetainPtr;

template<typename T> constexpr RetainPtr<T> adoptCF(T CF_RELEASES_ARGUMENT) WARN_UNUSED_RETURN;

#ifdef __OBJC__
template<typename T> RetainPtr<typename RetainPtr<T>::HelperPtrType> adoptNS(T NS_RELEASES_ARGUMENT) WARN_UNUSED_RETURN;
#endif

template<typename T> class RetainPtr {
public:
    using ValueType = std::remove_pointer_t<T>;
    using PtrType = ValueType*;

#ifdef __OBJC__
    using HelperPtrType = typename std::conditional_t<std::is_convertible_v<T, id> && !std::is_same_v<T, id>, std::remove_pointer_t<T>, T>;
#else
    using HelperPtrType = PtrType;
#endif

    RetainPtr() = default;
    RetainPtr(PtrType);

    RetainPtr(const RetainPtr&);
    template<typename U> RetainPtr(const RetainPtr<U>&);

    constexpr RetainPtr(RetainPtr&& o) : m_ptr(toStorageType(o.leakRef())) { }
    template<typename U> constexpr RetainPtr(RetainPtr<U>&& o) : m_ptr(toStorageType(checkType(o.leakRef()))) { }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    constexpr RetainPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) { }
    constexpr bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    ~RetainPtr();

    void clear();
    PtrType leakRef() WARN_UNUSED_RETURN;
    PtrType autorelease();

#ifdef __OBJC__
    id bridgingAutorelease();
#endif

    constexpr PtrType get() const { return fromStorageType(m_ptr); }
    constexpr PtrType operator->() const { return fromStorageType(m_ptr); }
    constexpr explicit operator PtrType() const { return fromStorageType(m_ptr); }
    constexpr explicit operator bool() const { return m_ptr; }

    constexpr bool operator!() const { return !m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    // FIXME: Eventually we should remove this; it's an outdated technique and less needed since we have explicit operator bool.
    typedef CFTypeRef RetainPtr::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_ptr ? &RetainPtr::m_ptr : nullptr; }

    RetainPtr& operator=(const RetainPtr&);
    template<typename U> RetainPtr& operator=(const RetainPtr<U>&);
    RetainPtr& operator=(PtrType);
    template<typename U> RetainPtr& operator=(U*);

    RetainPtr& operator=(RetainPtr&&);
    template<typename U> RetainPtr& operator=(RetainPtr<U>&&);

    void swap(RetainPtr&);

    template<typename U> friend constexpr RetainPtr<U> adoptCF(U CF_RELEASES_ARGUMENT) WARN_UNUSED_RETURN;

#ifdef __OBJC__
    template<typename U> friend RetainPtr<typename RetainPtr<U>::HelperPtrType> adoptNS(U NS_RELEASES_ARGUMENT) WARN_UNUSED_RETURN;
#endif

private:
    enum AdoptTag { Adopt };
    constexpr RetainPtr(PtrType ptr, AdoptTag) : m_ptr(toStorageType(ptr)) { }

    static constexpr PtrType checkType(PtrType ptr) { return ptr; }

    static constexpr PtrType hashTableDeletedValue() { return reinterpret_cast<PtrType>(-1); }

#ifdef __OBJC__
    template<typename U> constexpr std::enable_if_t<std::is_convertible_v<U, id>, PtrType> fromStorageTypeHelper(CFTypeRef ptr) const
    {
        return (__bridge PtrType)const_cast<CF_BRIDGED_TYPE(id) void*>(ptr);
    }
    template<typename U> constexpr std::enable_if_t<!std::is_convertible_v<U, id>, PtrType> fromStorageTypeHelper(CFTypeRef ptr) const
    {
        return (PtrType)const_cast<CF_BRIDGED_TYPE(id) void*>(ptr);
    }
    constexpr PtrType fromStorageType(CFTypeRef ptr) const { return fromStorageTypeHelper<PtrType>(ptr); }
    constexpr CFTypeRef toStorageType(id ptr) const { return (__bridge CFTypeRef)ptr; }
    constexpr CFTypeRef toStorageType(CFTypeRef ptr) const { return (CFTypeRef)ptr; }
#else
    constexpr PtrType fromStorageType(CFTypeRef ptr) const
    {
        return (PtrType)const_cast<CF_BRIDGED_TYPE(id) void*>(ptr);
    }
    constexpr CFTypeRef toStorageType(PtrType ptr) const { return (CFTypeRef)ptr; }
#endif

    CFTypeRef m_ptr { nullptr };
};

template<typename T> RetainPtr(T) -> RetainPtr<std::remove_pointer_t<T>>;

// Helper function for creating a RetainPtr using template argument deduction.
template<typename T> RetainPtr<typename RetainPtr<T>::HelperPtrType> retainPtr(T) WARN_UNUSED_RETURN;

template<typename T> inline RetainPtr<T>::~RetainPtr()
{
    if (auto ptr = std::exchange(m_ptr, nullptr))
        CFRelease(ptr);
}

template<typename T> inline RetainPtr<T>::RetainPtr(PtrType ptr)
    : m_ptr(toStorageType(ptr))
{
    if (m_ptr)
        CFRetain(m_ptr);
}

template<typename T> inline RetainPtr<T>::RetainPtr(const RetainPtr& o)
    : RetainPtr(o.get())
{
}

template<typename T> template<typename U> inline RetainPtr<T>::RetainPtr(const RetainPtr<U>& o)
    : RetainPtr(o.get())
{
}

template<typename T> inline void RetainPtr<T>::clear()
{
    if (auto ptr = std::exchange(m_ptr, nullptr))
        CFRelease(ptr);
}

template<typename T> inline auto RetainPtr<T>::leakRef() -> PtrType
{
    return fromStorageType(std::exchange(m_ptr, nullptr));
}

template<typename T> inline auto RetainPtr<T>::autorelease() -> PtrType
{
#ifdef __OBJC__
    if constexpr (std::is_convertible_v<PtrType, id>)
        return CFBridgingRelease(std::exchange(m_ptr, nullptr));
#endif
    if (m_ptr)
        CFAutorelease(m_ptr);
    return leakRef();
}

#ifdef __OBJC__

// FIXME: It would be better if we could base the return type on the type that is toll-free bridged with T rather than using id.
template<typename T> inline id RetainPtr<T>::bridgingAutorelease()
{
    static_assert((!std::is_convertible<PtrType, id>::value), "Don't use bridgingAutorelease for Objective-C pointer types.");
    return CFBridgingRelease(leakRef());
}

#endif

template<typename T> inline RetainPtr<T>& RetainPtr<T>::operator=(const RetainPtr& o)
{
    RetainPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T> template<typename U> inline RetainPtr<T>& RetainPtr<T>::operator=(const RetainPtr<U>& o)
{
    RetainPtr ptr = o;
    swap(ptr);
    return *this;
}

template<typename T> inline RetainPtr<T>& RetainPtr<T>::operator=(PtrType optr)
{
    RetainPtr ptr = optr;
    swap(ptr);
    return *this;
}

template<typename T> template<typename U> inline RetainPtr<T>& RetainPtr<T>::operator=(U* optr)
{
    RetainPtr ptr = optr;
    swap(ptr);
    return *this;
}

template<typename T> inline RetainPtr<T>& RetainPtr<T>::operator=(RetainPtr&& o)
{
    RetainPtr ptr = WTFMove(o);
    swap(ptr);
    return *this;
}

template<typename T> template<typename U> inline RetainPtr<T>& RetainPtr<T>::operator=(RetainPtr<U>&& o)
{
    RetainPtr ptr = WTFMove(o);
    swap(ptr);
    return *this;
}

template<typename T> inline void RetainPtr<T>::swap(RetainPtr& o)
{
    std::swap(m_ptr, o.m_ptr);
}

template<typename T> inline void swap(RetainPtr<T>& a, RetainPtr<T>& b)
{
    a.swap(b);
}

template<typename T, typename U> constexpr bool operator==(const RetainPtr<T>& a, const RetainPtr<U>& b)
{ 
    return a.get() == b.get(); 
}

template<typename T, typename U> constexpr bool operator==(const RetainPtr<T>& a, U* b)
{
    return a.get() == b; 
}

template<typename T, typename U> constexpr bool operator==(T* a, const RetainPtr<U>& b)
{
    return a == b.get(); 
}

template<typename T, typename U> constexpr bool operator!=(const RetainPtr<T>& a, const RetainPtr<U>& b)
{ 
    return a.get() != b.get(); 
}

template<typename T, typename U> constexpr bool operator!=(const RetainPtr<T>& a, U* b)
{
    return a.get() != b; 
}

template<typename T, typename U> constexpr bool operator!=(T* a, const RetainPtr<U>& b)
{ 
    return a != b.get(); 
}

template<typename T> constexpr RetainPtr<T> adoptCF(T CF_RELEASES_ARGUMENT ptr)
{
#ifdef __OBJC__
    static_assert((!std::is_convertible<T, id>::value), "Don't use adoptCF with Objective-C pointer types, use adoptNS.");
#endif
    return RetainPtr<T>(ptr, RetainPtr<T>::Adopt);
}

#ifdef __OBJC__
template<typename T> inline RetainPtr<typename RetainPtr<T>::HelperPtrType> adoptNS(T NS_RELEASES_ARGUMENT ptr)
{
#if __has_feature(objc_arc)
    return ptr;
#elif defined(OBJC_NO_GC)
    using ReturnType = RetainPtr<typename RetainPtr<T>::HelperPtrType>;
    return ReturnType { ptr, ReturnType::Adopt };
#else
    RetainPtr<typename RetainPtr<T>::HelperPtrType> result = ptr;
    [ptr release];
    return result;
#endif
}
#endif

template<typename T> inline RetainPtr<typename RetainPtr<T>::HelperPtrType> retainPtr(T ptr)
{
    return ptr;
}

template<typename T> struct IsSmartPtr<RetainPtr<T>> {
    static constexpr bool value = true;
};

template<typename P> struct HashTraits<RetainPtr<P>> : SimpleClassHashTraits<RetainPtr<P>> {
};

template<typename P> struct DefaultHash<RetainPtr<P>> : PtrHash<RetainPtr<P>> { };

template<typename P> struct RetainPtrObjectHashTraits : SimpleClassHashTraits<RetainPtr<P>> {
    static const RetainPtr<P>& emptyValue()
    {
        static NeverDestroyed<RetainPtr<P>> null;
        return null;
    }
};

template<typename P> struct RetainPtrObjectHash {
    static unsigned hash(const RetainPtr<P>& o)
    {
        ASSERT_WITH_MESSAGE(o.get(), "attempt to use null RetainPtr in HashTable");
        return static_cast<unsigned>(CFHash(o.get()));
    }
    static bool equal(const RetainPtr<P>& a, const RetainPtr<P>& b)
    {
        return CFEqual(a.get(), b.get());
    }
    static constexpr bool safeToCompareToEmptyOrDeleted = false;
};

inline bool safeCFEqual(CFTypeRef a, CFTypeRef b)
{
    return (!a && !b) || (a && b && CFEqual(a, b));
}

inline CFHashCode safeCFHash(CFTypeRef a)
{
    return a ? CFHash(a) : 0;
}

#ifdef __OBJC__
// FIXME: Move to TypeCastsCocoa.h once all clients include that header.
template<typename T> T* dynamic_objc_cast(id object)
{
    if (![object isKindOfClass:[T class]])
        return nullptr;

    return reinterpret_cast<T*>(object);
}
#endif

} // namespace WTF

using WTF::RetainPtr;
using WTF::adoptCF;
using WTF::retainPtr;
using WTF::safeCFEqual;
using WTF::safeCFHash;

#ifdef __OBJC__
using WTF::adoptNS;
using WTF::dynamic_objc_cast;
#endif

#endif // USE(CF) || defined(__OBJC__)
