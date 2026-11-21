/*
 *  Copyright (C) 2005-2025 Apple Inc. All rights reserved.
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

#include <wtf/NeverDestroyed.h>
#include <wtf/RetainRef.h>
#include <wtf/SwiftBridging.h>

namespace WTF {

template<typename T> [[nodiscard]] constexpr RetainPtr<RetainPtrType<T>> adoptCF(T CF_RELEASES_ARGUMENT);

template<typename T> [[nodiscard]] constexpr RetainPtr<RetainPtrType<T>> adoptNS(T NS_RELEASES_ARGUMENT);

/**
 * @brief RetainPtr is a reference-counting smart pointer for Objective-C and Core Foundation (CF) types.
 *
 * It extends the lifetime of the referenced object by retaining it on construction and releasing it on
 * destruction.
 *
 * RetainPtr can hold either Objective-C types (e.g., RetainPtr<NSDictionary>, RetainPtr<UIView>,
 * RetainPtr<AVPlayer>) or CF types (e.g., RetainPtr<CFDictionaryRef>). CF types include not only Core
 * Foundation types, but also types from other Apple frameworks that follow CF-style reference counting,
 * such as Core Graphics (CGImageRef), Core Text (CTFontRef), Core Media (CMSampleBufferRef), Core Video
 * (CVPixelBufferRef), IOSurface, Security (SecAccessControlRef), and others.
 *
 * To create a RetainPtr, use one of the following:
 * @code
 * RetainPtr ptr = value;      // Retains the value (increments the ref count)
 * RetainPtr ptr = adoptCF(x); // Takes ownership without retaining
 * RetainPtr ptr = adoptNS(x); // Takes ownership without retaining
 * @endcode
 *
 * Use adoptCF() or adoptNS() when you receive an object that you already own (i.e., the object was
 * returned to you with a +1 retain count). This includes objects from Create/Copy CF functions (e.g.,
 * CFStringCreateCopy()) and Objective-C alloc/init/copy/new methods (e.g., [[NSString alloc] init]).
 * Using the regular RetainPtr constructor instead of adoptCF()/adoptNS() would add an extra retain,
 * causing a leak when the RetainPtr is destroyed. Use the regular constructor when you want to add a
 * reference to an object you don't already own (e.g., a parameter passed to your function or a property
 * getter).
 *
 * @note For libdispatch types (dispatch_queue_t, dispatch_source_t, etc.), XPC types (xpc_connection_t,
 * xpc_object_t, etc.), and Network framework types (nw_endpoint_t, nw_path_t, etc.), use OSObjectPtr
 * instead of RetainPtr.
 *
 * @note RetainPtr is compatible with ARC (Automatic Reference Counting) and will automatically use the
 * appropriate retain/release semantics based on the compilation mode.
 */
template<typename T> class SWIFT_SELF_CONTAINED RetainPtr {
public:
    using ValueType = std::remove_pointer_t<T>;
    using PtrType = ValueType*;

#ifdef __OBJC__
    using StorageType = PtrType;
#else
    // Type pun id to CFTypeRef in C++ files. This is valid because they're ABI equivalent.
    using StorageType = std::conditional_t<IsNSType<PtrType>, CFTypeRef, PtrType>;
#endif

    RetainPtr() = default;
    RetainPtr(PtrType);

    RetainPtr(const RetainPtr&);
    template<typename U> RetainPtr(const RetainPtr<U>&);

    constexpr RetainPtr(RetainPtr&& o) : m_ptr(o.leakRef()) { }
    template<typename U>
        requires std::convertible_to<typename RetainPtr<RetainPtrType<U>>::PtrType, PtrType>
    constexpr RetainPtr(RetainPtr<U>&& o) : m_ptr(o.leakRef()) { }
    template<typename U>
        requires std::convertible_to<typename RetainRef<RetainPtrType<U>>::PtrType, PtrType>
    constexpr RetainPtr(RetainRef<U>&& o) : m_ptr(o.leakRef()) { }

    ~RetainPtr();

    void clear();

    // Pair NS_RETURNS_RETAINED with id and CF_RETURNS_RETAINED with CFTypeRef.
    // Also use a templated return type to defer instantiation because merely
    // declaring the wrong combination can trigger a warning (even if `requires`
    // prevents calling it).
    template<typename U = StorageType>
        requires (std::same_as<U, StorageType> && NSType<U>)
    [[nodiscard]] U leakRef() NS_RETURNS_RETAINED {
        return std::exchange(m_ptr, nullptr);
    }

    template<typename U = StorageType>
        requires (std::same_as<U, StorageType> && !NSType<U>)
    [[nodiscard]] U leakRef() CF_RETURNS_RETAINED {
        return std::exchange(m_ptr, nullptr);
    }

    PtrType autorelease();
    PtrType getAutoreleased();

    RetainRef<T> releaseNonNull()
    {
        ASSERT(m_ptr);
        auto ptr = get();
        m_ptr = nullptr;
        return RetainRef<T>(ptr, RetainRef<T>::Adopt);
    }

#ifdef __OBJC__
    id bridgingAutorelease();
#endif

    constexpr PtrType get() const LIFETIME_BOUND { return static_cast<PtrType>(const_cast<std::remove_const_t<std::remove_pointer_t<StorageType>>*>(m_ptr)); }
    constexpr PtrType unsafeGet() const { return static_cast<PtrType>(const_cast<std::remove_const_t<std::remove_pointer_t<StorageType>>*>(m_ptr)); } // FIXME: Replace with get() then remove.
    constexpr PtrType operator->() const LIFETIME_BOUND { return get(); }
    constexpr operator PtrType() const LIFETIME_BOUND { return get(); }
    constexpr explicit operator bool() const { return m_ptr; }

    constexpr bool operator!() const { return !m_ptr; }

    RetainPtr& operator=(const RetainPtr&);
    template<typename U> RetainPtr& operator=(const RetainPtr<U>&);
    RetainPtr& operator=(PtrType);
    template<typename U> RetainPtr& operator=(U*);

    RetainPtr& operator=(RetainPtr&&);
    template<typename U> RetainPtr& operator=(RetainPtr<U>&&);
    template<typename U> RetainPtr& operator=(RetainRef<U>&&);

    void swap(RetainPtr&);

    template<typename U> friend constexpr RetainPtr<RetainPtrType<U>> adoptCF(U CF_RELEASES_ARGUMENT);

    template<typename U> friend constexpr RetainPtr<RetainPtrType<U>> adoptNS(U NS_RELEASES_ARGUMENT);

private:
    enum AdoptTag { Adopt };
    template<typename U = PtrType>
        requires (std::same_as<U, PtrType> && NSType<U>)
    constexpr RetainPtr(U NS_RELEASES_ARGUMENT ptr, AdoptTag) : m_ptr(WTF::move(ptr)) { }
    template<typename U = PtrType>
        requires (std::same_as<U, PtrType> && !NSType<U>)
    constexpr RetainPtr(U CF_RELEASES_ARGUMENT ptr, AdoptTag) : m_ptr(ptr) { }

    static constexpr bool storageIsManagedByARC = __has_feature(objc_arc) && IsNSType<PtrType>;

    StorageType m_ptr { nullptr };
};

template<typename T> RetainPtr(T) -> RetainPtr<RetainPtrType<T>>;
template<typename U> RetainPtr(RetainRef<U>&&) -> RetainPtr<U>;

// Helper function for creating a RetainPtr using template argument deduction.
template<typename T> [[nodiscard]] RetainPtr<RetainPtrType<T>> retainPtr(T);

template<typename T> inline RetainPtr<T>::~RetainPtr()
{
    SUPPRESS_UNRETAINED_LOCAL if (auto ptr = std::exchange(m_ptr, nullptr)) {
        if constexpr (!storageIsManagedByARC)
            CFRelease(ptr);
    }
}

template<typename T> inline RetainPtr<T>::RetainPtr(PtrType ptr)
    : m_ptr(ptr)
{
    if constexpr (!storageIsManagedByARC) {
        if (m_ptr)
            SUPPRESS_UNRETAINED_ARG CFRetain(m_ptr);
    }
}

template<typename T> inline RetainPtr<T>::RetainPtr(const RetainPtr& o)
    : m_ptr(o.m_ptr)
{
    if constexpr (!storageIsManagedByARC) {
        if (m_ptr)
            SUPPRESS_UNRETAINED_ARG CFRetain(m_ptr);
    }
}

template<typename T> template<typename U> inline RetainPtr<T>::RetainPtr(const RetainPtr<U>& o)
    : RetainPtr(o.get())
{
}

template<typename T> inline void RetainPtr<T>::clear()
{
    if (auto ptr = std::exchange(m_ptr, nullptr)) {
        if constexpr (!storageIsManagedByARC)
            CFRelease(ptr);
    }
}

template<typename T> inline auto RetainPtr<T>::autorelease() -> PtrType
{
    auto ptr = std::exchange(m_ptr, nullptr);
    if constexpr (!storageIsManagedByARC) {
        if (ptr)
            CFAutorelease(ptr);
    }
    return ptr;
}

template<typename T> inline auto RetainPtr<T>::getAutoreleased() -> PtrType
{
    RetainPtr copy { *this };
    return copy.autorelease();
}

#ifdef __OBJC__
// FIXME: It would be better if we could base the return type on the type that is toll-free bridged with T rather than using id.
template<typename T> inline id RetainPtr<T>::bridgingAutorelease()
{
    static_assert(!IsNSType<PtrType>, "Don't use bridgingAutorelease for Objective-C pointer types.");
    return CFBridgingRelease(leakRef());
}
#endif // __OBJC__

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
    RetainPtr ptr = WTF::move(o);
    swap(ptr);
    return *this;
}

template<typename T> template<typename U> inline RetainPtr<T>& RetainPtr<T>::operator=(RetainPtr<U>&& o)
{
    RetainPtr ptr = WTF::move(o);
    swap(ptr);
    return *this;
}

template<typename T> template<typename U> inline RetainPtr<T>& RetainPtr<T>::operator=(RetainRef<U>&& o)
{
    RetainPtr ptr = WTF::move(o);
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

template<typename T> constexpr RetainPtr<RetainPtrType<T>> adoptCF(T CF_RELEASES_ARGUMENT ptr)
{
    static_assert(!IsNSType<T>, "Don't use adoptCF with Objective-C pointer types, use adoptNS.");
    return { ptr, RetainPtr<RetainPtrType<T>>::Adopt };
}

template<typename T> constexpr RetainPtr<RetainPtrType<T>> adoptNS(T NS_RELEASES_ARGUMENT ptr)
{
    static_assert(IsNSType<T>, "Don't use adoptNS with Core Foundation pointer types, use adoptCF.");
    return { ptr, RetainPtr<RetainPtrType<T>>::Adopt };
}

template<typename T> inline RetainPtr<RetainPtrType<T>> retainPtr(T ptr)
{
    return ptr;
}

#if USE(CF)
template<CFType T>
ALWAYS_INLINE CLANG_POINTER_CONVERSION RetainPtr<RetainPtrType<T>> protect(T ptr)
{
    return ptr;
}
#endif

#ifdef __OBJC__
template<NSType T>
ALWAYS_INLINE CLANG_POINTER_CONVERSION RetainPtr<RetainPtrType<T>> protect(T ptr)
{
    return ptr;
}
#endif

template<typename T>
ALWAYS_INLINE CLANG_POINTER_CONVERSION RetainPtr<T> protect(const RetainPtr<T>& ptr)
{
    return ptr;
}

template<typename T>
RetainPtr<T> protect(RetainPtr<T>&&)
{
    static_assert(WTF::unreachableForType<T>, "Calling protect() on an rvalue is unnecessary; the caller already owns the value.");
}

template<typename T> struct IsSmartPtr<RetainPtr<T>> {
    static constexpr bool value = true;
    static constexpr bool isNullable = true;
};

template<typename T> inline constexpr bool IsRetainPtr = false;
template<typename T> inline constexpr bool IsRetainPtr<RetainPtr<T>> = true;

template<typename P> struct HashTraits<RetainPtr<P>> : GenericHashTraits<RetainPtr<P>> {
    static constexpr bool emptyValueIsZero = true;
    static RetainPtr<P>::PtrType emptyValue() { return nullptr; }
    static bool isEmptyValue(const RetainPtr<P>& value) { return !value; }

    using PeekType = RetainPtr<P>::PtrType;
    static PeekType peek(const RetainPtr<P>& value) { return value.get(); }
    static PeekType peek(P* value) { return value; }
};

template<typename P> struct DefaultHash<RetainPtr<P>> : PtrHash<RetainPtr<P>> { };

template<typename P> struct RetainPtrObjectHashTraits : GenericHashTraits<RetainPtr<P>> {
    static constexpr bool emptyValueIsZero = true;
    static const RetainPtr<P>& emptyValue()
    {
        static NeverDestroyed<RetainPtr<P>> null;
        return null;
    }

    static bool isEmptyValue(const RetainPtr<P>& value) { return !value; }
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

template<typename T, typename U>
NODELETE SUPPRESS_NODELETE ALWAYS_INLINE void lazyInitialize(const RetainPtr<T>& ptr, RetainPtr<U>&& obj)
{
    RELEASE_ASSERT(!ptr);
    const_cast<RetainPtr<T>&>(ptr) = std::move(obj);
}

} // namespace WTF

using WTF::RetainPtr;
using WTF::adoptCF;
using WTF::lazyInitialize;
using WTF::protect;
using WTF::retainPtr;
using WTF::safeCFEqual;
using WTF::safeCFHash;

#ifdef __OBJC__
using WTF::adoptNS;
#endif

#endif // USE(CF) || defined(__OBJC__)
