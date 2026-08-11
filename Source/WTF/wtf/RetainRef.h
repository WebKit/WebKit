/*
 *  Copyright (C) 2026 Apple Inc. All rights reserved.
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
 */

#pragma once

#include <wtf/Platform.h>

#if USE(CF) || defined(__OBJC__)

#include <algorithm>
#include <cstddef>
#include <type_traits>
#include <wtf/HashTraits.h>
#include <wtf/cocoa/NSTypeTraits.h>

#if USE(CF)
#include <CoreFoundation/CoreFoundation.h>
#include <wtf/cf/CFTypeTraits.h>
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

#ifndef CF_RETURNS_RETAINED
#define CF_RETURNS_RETAINED
#endif

#ifndef NS_RELEASES_ARGUMENT
#define NS_RELEASES_ARGUMENT
#endif

#ifndef NS_RETURNS_RETAINED
#define NS_RETURNS_RETAINED
#endif

// Because ARC enablement is a compile-time choice, and we compile this header
// both ways, we need a separate copy of our code when ARC is enabled.
#if __has_feature(objc_arc)
#define RetainPtr RetainPtrArc
#define RetainRef RetainRefArc
#endif

namespace WTF {

template<typename T> class RetainPtr;
template<typename T> class RetainRef;

template<typename T> using RetainPtrType = std::conditional_t<IsNSType<T> && !std::same_as<T, id>, std::remove_pointer_t<T>, T>;

template<typename T> [[nodiscard]] constexpr RetainRef<RetainPtrType<T>> adoptCFRef(T CF_RELEASES_ARGUMENT);

template<typename T> [[nodiscard]] constexpr RetainRef<RetainPtrType<T>> adoptNSRef(T NS_RELEASES_ARGUMENT);

/**
 * @brief RetainRef is the non-nullable variant of RetainPtr.
 *
 * Like RetainPtr, RetainRef is a reference-counting smart pointer for Objective-C and Core
 * Foundation (CF) types. Unlike RetainPtr, it cannot hold a null value. The non-null invariant
 * is established at construction (every constructor that takes a raw pointer asserts the
 * pointer is non-null) and preserved by all subsequent operations.
 *
 * RetainRef has no default constructor, no clear() method, and no operator bool() / operator!()
 * — it is always non-null. The only paths that may temporarily produce a null storage slot are
 * the moved-from state and the special HashTable empty/deleted values, neither of which is
 * observable through the public accessors.
 *
 * To create a RetainRef:
 * @code
 * RetainRef ref = retainRef(value);    // Retains the value (asserts value is non-null)
 * RetainRef ref = adoptCFRef(x);       // Takes ownership without retaining (asserts non-null)
 * RetainRef ref = adoptNSRef(x);       // Takes ownership without retaining (asserts non-null)
 * RetainRef ref = ptr.releaseNonNull();// Convert from a non-null RetainPtr (asserts non-null)
 * @endcode
 *
 * See RetainPtr for full documentation on retain/release semantics.
 */
template<typename T> class RetainRef {
public:
    using ValueType = std::remove_pointer_t<T>;
    using PtrType = ValueType*;

#ifdef __OBJC__
    using StorageType = PtrType;
#else
    // Type pun id to CFTypeRef in C++ files. This is valid because they're ABI equivalent.
    using StorageType = std::conditional_t<IsNSType<PtrType>, CFTypeRef, PtrType>;
#endif

    RetainRef() = delete;

    RetainRef(PtrType);

    RetainRef(const RetainRef&);
    template<typename U> RetainRef(const RetainRef<U>&);

    constexpr RetainRef(RetainRef&& o) : m_ptr(o.leakRef()) { ASSERT(m_ptr); }
    template<typename U>
        requires std::convertible_to<typename RetainRef<RetainPtrType<U>>::PtrType, PtrType>
    constexpr RetainRef(RetainRef<U>&& o) : m_ptr(o.leakRef()) { ASSERT(m_ptr); }

    // Hash table deleted/empty values, which are only constructed and never copied or destroyed.
    constexpr RetainRef(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) { }
    constexpr bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    constexpr RetainRef(HashTableEmptyValueType) : m_ptr(nullptr) { }
    constexpr bool isHashTableEmptyValue() const { return !m_ptr; }
    static constexpr StorageType hashTableEmptyValue() { return nullptr; }

    ~RetainRef();

    template<typename U = StorageType>
        requires (std::same_as<U, StorageType> && NSType<U>)
    [[nodiscard]] StorageType leakRef() NS_RETURNS_RETAINED RETURNS_NONNULL {
        ASSERT(m_ptr);
        return std::exchange(m_ptr, nullptr);
    }

    template<typename U = StorageType>
        requires (std::same_as<U, StorageType> && !NSType<U>)
    [[nodiscard]] StorageType leakRef() CF_RETURNS_RETAINED RETURNS_NONNULL {
        ASSERT(m_ptr);
        return std::exchange(m_ptr, nullptr);
    }

    PtrType autorelease() RETURNS_NONNULL;
    PtrType getAutoreleased() RETURNS_NONNULL;

#ifdef __OBJC__
    id bridgingAutorelease();
#endif

    constexpr PtrType get() const LIFETIME_BOUND RETURNS_NONNULL { ASSERT(m_ptr); return static_cast<PtrType>(const_cast<std::remove_const_t<std::remove_pointer_t<StorageType>>*>(m_ptr)); }
    constexpr PtrType ptrAllowingHashTableEmptyValue() const LIFETIME_BOUND { ASSERT(m_ptr || isHashTableEmptyValue()); return static_cast<PtrType>(const_cast<std::remove_const_t<std::remove_pointer_t<StorageType>>*>(m_ptr)); }
    constexpr PtrType operator->() const LIFETIME_BOUND { ASSERT(m_ptr); return get(); }
    constexpr operator PtrType() const LIFETIME_BOUND { ASSERT(m_ptr); return get(); }

    RetainRef& operator=(const RetainRef&);
    template<typename U> RetainRef& operator=(const RetainRef<U>&);
    RetainRef& operator=(PtrType);
    template<typename U> RetainRef& operator=(U*);

    RetainRef& operator=(RetainRef&&);
    template<typename U> RetainRef& operator=(RetainRef<U>&&);

    void swap(RetainRef&);

    template<typename U> friend constexpr RetainRef<RetainPtrType<U>> adoptCFRef(U CF_RELEASES_ARGUMENT);

    template<typename U> friend constexpr RetainRef<RetainPtrType<U>> adoptNSRef(U NS_RELEASES_ARGUMENT);

private:
    template<typename U> friend class RetainRef;
    template<typename U> friend class RetainPtr;

    enum AdoptTag { Adopt };
    constexpr RetainRef(PtrType ptr, AdoptTag) : m_ptr(ptr) { ASSERT(m_ptr); }

#if __has_feature(objc_arc)
    // ARC will try to retain/release this value, but it looks like a tagged immediate, so retain/release ends up being a no-op -- see _objc_isTaggedPointer() in <objc-internal.h>.
    template<typename U = PtrType>
        requires (std::same_as<U, PtrType> && NSType<U>)
    static constexpr PtrType hashTableDeletedValue() { return (__bridge PtrType)(void*)-1; }

    template<typename U = PtrType>
        requires (std::same_as<U, PtrType> && !NSType<U>)
    static constexpr PtrType hashTableDeletedValue() { return reinterpret_cast<PtrType>(-1); }
#else
    static constexpr PtrType hashTableDeletedValue() { return reinterpret_cast<PtrType>(-1); }
#endif

    static inline void retainFoundationPtr(CFTypeRef ptr) { CFRetain(ptr); }
    static inline void releaseFoundationPtr(CFTypeRef ptr) { CFRelease(ptr); }
    static inline void autoreleaseFoundationPtr(CFTypeRef ptr) { CFAutorelease(ptr); }

#ifdef __OBJC__
#if __has_feature(objc_arc)
    static inline void retainFoundationPtr(id) { }
    static inline void releaseFoundationPtr(id) { }
    static inline void autoreleaseFoundationPtr(id) { }
#else
    static inline void retainFoundationPtr(id ptr) { [ptr retain]; }
    static inline void releaseFoundationPtr(id ptr) { [ptr release]; }
    static inline void autoreleaseFoundationPtr(id ptr) { [ptr autorelease]; }
#endif
#endif

    StorageType m_ptr;
};

template<typename T> RetainRef(T) -> RetainRef<RetainPtrType<T>>;

// Helper function for creating a RetainRef using template argument deduction.
template<typename T> [[nodiscard]] RetainRef<RetainPtrType<T>> retainRef(T);

template<typename T> inline RetainRef<T>::~RetainRef()
{
    SUPPRESS_UNRETAINED_LOCAL if (auto ptr = std::exchange(m_ptr, nullptr))
        releaseFoundationPtr(ptr);
}

template<typename T> inline RetainRef<T>::RetainRef(PtrType ptr)
    : m_ptr(ptr)
{
    ASSERT(m_ptr);
    SUPPRESS_UNRETAINED_ARG retainFoundationPtr(m_ptr);
}

template<typename T> inline RetainRef<T>::RetainRef(const RetainRef& o)
    : m_ptr(o.m_ptr)
{
    ASSERT(m_ptr);
    SUPPRESS_UNRETAINED_ARG retainFoundationPtr(m_ptr);
}

template<typename T> template<typename U> inline RetainRef<T>::RetainRef(const RetainRef<U>& o)
    : RetainRef(o.get())
{
}

template<typename T> inline auto RetainRef<T>::autorelease() -> PtrType
{
    ASSERT(m_ptr);
    auto ptr = std::exchange(m_ptr, nullptr);
    autoreleaseFoundationPtr(ptr);
    return ptr;
}

template<typename T> inline auto RetainRef<T>::getAutoreleased() -> PtrType
{
    RetainRef copy { *this };
    return copy.autorelease();
}

#ifdef __OBJC__
// FIXME: It would be better if we could base the return type on the type that is toll-free bridged with T rather than using id.
template<typename T> inline id RetainRef<T>::bridgingAutorelease()
{
    static_assert(!IsNSType<PtrType>, "Don't use bridgingAutorelease for Objective-C pointer types.");
    return CFBridgingRelease(leakRef());
}
#endif // __OBJC__

template<typename T> inline RetainRef<T>& RetainRef<T>::operator=(const RetainRef& o)
{
    RetainRef ref = o;
    swap(ref);
    return *this;
}

template<typename T> template<typename U> inline RetainRef<T>& RetainRef<T>::operator=(const RetainRef<U>& o)
{
    RetainRef ref = o;
    swap(ref);
    return *this;
}

template<typename T> inline RetainRef<T>& RetainRef<T>::operator=(PtrType optr)
{
    RetainRef ref = optr;
    swap(ref);
    return *this;
}

template<typename T> template<typename U> inline RetainRef<T>& RetainRef<T>::operator=(U* optr)
{
    RetainRef ref = optr;
    swap(ref);
    return *this;
}

template<typename T> inline RetainRef<T>& RetainRef<T>::operator=(RetainRef&& o)
{
    RetainRef ref = WTF::move(o);
    swap(ref);
    return *this;
}

template<typename T> template<typename U> inline RetainRef<T>& RetainRef<T>::operator=(RetainRef<U>&& o)
{
    RetainRef ref = WTF::move(o);
    swap(ref);
    return *this;
}

template<typename T> inline void RetainRef<T>::swap(RetainRef& o)
{
    std::swap(m_ptr, o.m_ptr);
}

template<typename T> inline void swap(RetainRef<T>& a, RetainRef<T>& b)
{
    a.swap(b);
}

template<typename T, typename U> constexpr bool operator==(const RetainRef<T>& a, const RetainRef<U>& b)
{
    return a.get() == b.get();
}

template<typename T, typename U> constexpr bool operator==(const RetainRef<T>& a, U* b)
{
    return a.get() == b;
}

template<typename T> constexpr RetainRef<RetainPtrType<T>> adoptCFRef(T CF_RELEASES_ARGUMENT ptr)
{
    static_assert(!IsNSType<T>, "Don't use adoptCFRef with Objective-C pointer types, use adoptNSRef.");
    ASSERT(ptr);
    return { ptr, RetainRef<RetainPtrType<T>>::Adopt };
}

template<typename T> constexpr RetainRef<RetainPtrType<T>> adoptNSRef(T NS_RELEASES_ARGUMENT ptr)
{
    static_assert(IsNSType<T>, "Don't use adoptNSRef with Core Foundation pointer types, use adoptCFRef.");
    ASSERT(ptr);
    return { ptr, RetainRef<RetainPtrType<T>>::Adopt };
}

template<typename T> inline RetainRef<RetainPtrType<T>> retainRef(T ptr)
{
    return ptr;
}

template<typename T> struct IsSmartPtr<RetainRef<T>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = false;
};

template<typename T> inline constexpr bool IsRetainRef = false;
template<typename T> inline constexpr bool IsRetainRef<RetainRef<T>> = true;

template<typename P> struct HashTraits<RetainRef<P>> : SimpleClassHashTraits<RetainRef<P>> {
    static constexpr bool emptyValueIsZero = true;
    static RetainRef<P> emptyValue() { return HashTableEmptyValue; }

    template<typename>
    static void constructEmptyValue(RetainRef<P>& slot)
    {
        new (NotNull, std::addressof(slot)) RetainRef<P>(HashTableEmptyValue);
    }

    static constexpr bool hasIsEmptyValueFunction = true;
    static bool isEmptyValue(const RetainRef<P>& value) { return value.isHashTableEmptyValue(); }

    using PeekType = RetainRef<P>::PtrType;
    static PeekType peek(const RetainRef<P>& value) { return value.ptrAllowingHashTableEmptyValue(); }
    static PeekType peek(P* value) { return value; }

    using TakeType = RetainPtr<P>;
    static TakeType take(RetainRef<P>&& value) { return isEmptyValue(value) ? RetainPtr<P>() : RetainPtr<P>(value.get()); }
};

template<typename P> struct DefaultHash<RetainRef<P>> : PtrHash<RetainRef<P>> { };

} // namespace WTF

using WTF::RetainRef;
using WTF::adoptCFRef;
using WTF::retainRef;

#ifdef __OBJC__
using WTF::adoptNSRef;
#endif

#endif // USE(CF) || defined(__OBJC__)
