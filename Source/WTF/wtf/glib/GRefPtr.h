/*
 *  Copyright (C) 2025 Igalia S.L.
 *  Copyright (C) 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *  Copyright (C) 2008 Collabora Ltd.
 *  Copyright (C) 2009 Martin Robinson
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

#if USE(GLIB)

#include <algorithm>
#include <glib-object.h>
#include <wtf/HashTraits.h>

extern "C" {
    typedef struct _GDBusConnection GDBusConnection;
    typedef struct _GDBusNodeInfo GDBusNodeInfo;

    GDBusNodeInfo* g_dbus_node_info_ref(GDBusNodeInfo*);
    void g_dbus_node_info_unref(GDBusNodeInfo*);

    // Since GLib 2.56 a g_object_ref_sink() macro may be defined which propagates
    // the type of the parameter to the returned value, but it conflicts with the
    // declaration below, causing an error when glib-object.h is included before
    // this file. Thus, add the forward declarations only when the macro is not
    // present.
#ifndef g_object_ref_sink
    void g_object_unref(gpointer);
    gpointer g_object_ref_sink(gpointer);
#endif
};

namespace WTF {

template<typename T> struct GRefPtrDefaultRefDerefTraits {
    static inline T* refIfNotNull(T* ptr)
    {
        if (ptr) [[likely]]
            g_object_ref_sink(ptr);
        return ptr;
    }

    static inline void derefIfNotNull(T* ptr)
    {
        if (ptr) [[likely]]
            g_object_unref(ptr);
    }

    static inline bool isFloating(T* ptr)
    {
        if (ptr) [[likely]]
            return g_object_is_floating(ptr);
        return false;
    }
};

template<typename T, typename GRefPtrRefDerefTraits> class GRefPtr;
template<typename T, typename GRefPtrRefDerefTraits = GRefPtrDefaultRefDerefTraits<T>> GRefPtr<T, GRefPtrRefDerefTraits> adoptGRef(T*);

// Smart pointer for C types with automatic ref-counting, especially designed for interfacing with glib-based APIs.
// An instance of GRefPtr<T> is either empty or owns a reference to object T.
//
// GRefPtr<T> relies on implementations of GRefPtrDefaultRefDerefTraits<T> and adoptGRef<T>().
// The default implementations use g_object_ref_sink(), g_object_unref() and g_object_is_floating().
// However, many specializations are available in separate headers such as:
// GRefPtrGStreamer.h, GRefPtrGtk.h and GRefPtrWPE.h.
//
// These specializations allow GRefPtr to work with types that are not derived from GObject,
// such as GstMiniObject, as well as to take advantage of wrappers with improved logging such
// as GstObject.
//
// **You must include the header containing any specific specializations you need.**
// Failing to do so will silently cause the default implementation to be used.
template<typename T, typename _GRefPtrRefDerefTraits = GRefPtrDefaultRefDerefTraits<T>> class GRefPtr {
public:
    using RefDerefTraits = _GRefPtrRefDerefTraits;
    typedef T ValueType;
    typedef ValueType* PtrType;

    constexpr GRefPtr() : m_ptr(nullptr) { }
    constexpr GRefPtr(std::nullptr_t) : m_ptr(nullptr) { }

    // Acquires an owning reference of ptr and returns the GRefPtr<T> containing it.
    // This corresponds to the semantics of transfer floating in glib and g_object_ref_sink():
    //
    // * If ptr was a floating reference, it stops being floating and ownership *moves* into GRefPtr;
    //   this corresponds to the same semantics as transfer full.
    // * If ptr was not a floating reference, the ref count is increased to accomodate the new GRefPtr
    //   in addition to any existing owners; this corresponds to the same semantics as transfer none.
    //
    // To transfer ownership of a raw pointer reference that is not floating, see adoptGRef() instead.
    GRefPtr(T* ptr /* (transfer floating) */)
        : m_ptr(RefDerefTraits::refIfNotNull(ptr))
    {
    }

    GRefPtr(const GRefPtr& o)
        : m_ptr(RefDerefTraits::refIfNotNull(o.m_ptr))
    {
    }

    GRefPtr(GRefPtr&& o)
        : m_ptr(o.leakRef())
    {
    }

    ~GRefPtr()
    {
        clear();
    }

    void clear()
    {
        RefDerefTraits::derefIfNotNull(std::exchange(m_ptr, nullptr));
    }

    // Relinquishes the owned reference as a raw pointer. GRefPtr<T> is empty afterwards.
    T* /* (transfer full) */ leakRef() WARN_UNUSED_RETURN
    {
        return std::exchange(m_ptr, nullptr);
    }

    // Increments the reference count.
    T* /* (transfer full) */ ref() WARN_UNUSED_RETURN {
        return RefDerefTraits::refIfNotNull(m_ptr);
    }

    T*& outPtr()
    {
        clear();
        return m_ptr;
    }

    // Hash table deleted values, which are only constructed and never copied or destroyed.
    GRefPtr(HashTableDeletedValueType) : m_ptr(hashTableDeletedValue()) { }
    bool isHashTableDeletedValue() const { return m_ptr == hashTableDeletedValue(); }

    // Borrows the raw pointer from GRefPtr.
    // The pointer is guaranteed to be valid for as long as GRefPtr holds an owning reference
    // to that object.
    T* /* (transfer none) */ get() const LIFETIME_BOUND { return m_ptr; }
    // Same as get(), but clang won't complain if you return the pointer to your callee.
    // Generally unsafe: to use, you must guarantee that the object will still have strong
    // references by the time your function returns.
    // Only used for C API functions returning (transfer none) of objects where the above
    // can be established, e.g. objects stored in a global dictionary.
    T* /* (transfer none) */ getUncheckedLifetime() const { return m_ptr; }
    ALWAYS_INLINE T* operator->() const { return m_ptr; }

    bool operator!() const { return !m_ptr; }
    explicit operator bool() const { return !!m_ptr; }

    // This conversion operator allows implicit conversion to bool but not to other integer types.
    typedef T* GRefPtr::*UnspecifiedBoolType;
    operator UnspecifiedBoolType() const { return m_ptr ? &GRefPtr::m_ptr : 0; }

    GRefPtr& operator=(const GRefPtr& other)
    {
        GRefPtr ptr = other;
        swap(ptr);
        return *this;
    }

    GRefPtr& operator=(GRefPtr&& other)
    {
        GRefPtr ptr = WTFMove(other);
        swap(ptr);
        return *this;
    }

    GRefPtr& operator=(T* other)
    {
        GRefPtr ptr = other;
        swap(ptr);
        return *this;
    }

    GRefPtr& operator=(std::nullptr_t)
    {
        clear();
        return *this;
    }

    void swap(GRefPtr& other)
    {
        std::swap(m_ptr, other.m_ptr);
    }

private:
    static T* hashTableDeletedValue() { return reinterpret_cast<T*>(-1); }

    friend GRefPtr adoptGRef<T, RefDerefTraits>(T*);
    enum AdoptTag { Adopt };
    GRefPtr(T* ptr, AdoptTag) : m_ptr(ptr) { }

    T* m_ptr;
};

template<typename T, typename U> inline void swap(GRefPtr<T, U>& a, GRefPtr<T, U>& b)
{
    a.swap(b);
}

template<typename T, typename U> inline bool operator==(const GRefPtr<T>& a, const GRefPtr<U>& b)
{
    return a.get() == b.get();
}

template<typename T, typename U> inline bool operator==(const GRefPtr<T>& a, U* b)
{
    return a.get() == b;
}

// Transfer ownership of a raw pointer non-floating reference into a new GRefPtr (transfer full semantics).
// This involves no refcount increases or calls to glib.
// This corresponds to the semantics of transfer full in glib.
//
// adoptGRef() must NEVER be used with floating references, as any ref_sink() would cause
// the reference to be stolen from GRefPtr.
template <typename T, typename U> GRefPtr<T, U> adoptGRef(T* ptr /* (transfer full) */)
{
    ASSERT(!U::isFloating(ptr));
    return GRefPtr<T, U>(ptr, GRefPtr<T, U>::Adopt);
}

template<typename T, typename U> struct IsSmartPtr<GRefPtr<T, U>> {
    static const bool value = true;
    static constexpr bool isNullable = true;
};

template<typename P> struct DefaultHash<GRefPtr<P>> : PtrHash<GRefPtr<P>> { };

template<typename P> struct HashTraits<GRefPtr<P>> : SimpleClassHashTraits<GRefPtr<P>> {
    static P* emptyValue() { return nullptr; }

    typedef P* PeekType;
    static PeekType peek(const GRefPtr<P>& value) { return value.get(); }
    static PeekType peek(P* value) { return value; }

    static void customDeleteBucket(GRefPtr<P>& value)
    {
        // See unique_ptr's customDeleteBucket() for an explanation.
        ASSERT(!SimpleClassHashTraits<GRefPtr<P>>::isDeletedValue(value));
        auto valueToBeDestroyed = WTFMove(value);
        SimpleClassHashTraits<GRefPtr<P>>::constructDeletedValue(value);
    }
};

#define WTF_DECLARE_GREF_TRAITS(typeName, ...) \
    template<> struct GRefPtrDefaultRefDerefTraits<typeName> { \
        static __VA_ARGS__ typeName* refIfNotNull(typeName*); \
        static __VA_ARGS__ void derefIfNotNull(typeName*); \
        static __VA_ARGS__ bool isFloating(typeName*); \
    };

#define WTF_DEFINE_GREF_TRAITS(typeName, refFunc, derefFunc, ...) \
    typeName* GRefPtrDefaultRefDerefTraits<typeName>::refIfNotNull(typeName* ptr) \
    { \
        if (ptr) [[likely]] \
            refFunc(ptr); \
        return ptr; \
    } \
    void GRefPtrDefaultRefDerefTraits<typeName>::derefIfNotNull(typeName* ptr) \
    { \
        if (ptr) [[likely]] \
            derefFunc(ptr); \
    } \
    bool GRefPtrDefaultRefDerefTraits<typeName>::isFloating(typeName* __VA_OPT__(ptr)) \
    { \
        __VA_OPT__( \
        if (ptr) [[likely]] \
            return __VA_ARGS__(ptr); \
        ) \
        return false; \
    }

#define WTF_DEFINE_GREF_TRAITS_INLINE(typeName, refFunc, derefFunc, ...) \
    template<> struct GRefPtrDefaultRefDerefTraits<typeName> { \
        static inline typeName* refIfNotNull(typeName* ptr) \
        { \
            if (ptr) [[likely]] \
                refFunc(ptr); \
            return ptr; \
        } \
        static inline void derefIfNotNull(typeName* ptr) \
        { \
            if (ptr) [[likely]] \
                derefFunc(ptr); \
        } \
        static inline bool isFloating(typeName* __VA_OPT__(ptr)) \
        { \
            __VA_OPT__( \
            if (ptr) [[likely]] \
                return __VA_ARGS__(ptr); \
            ) \
            return false; \
        } \
    };

WTF_DECLARE_GREF_TRAITS(GDBusNodeInfo, WTF_EXPORT_PRIVATE)
WTF_DECLARE_GREF_TRAITS(GResource)

WTF_DEFINE_GREF_TRAITS_INLINE(GArray, g_array_ref, g_array_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GByteArray, g_byte_array_ref, g_byte_array_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GBytes, g_bytes_ref, g_bytes_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GClosure, g_closure_ref, g_closure_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GDateTime, g_date_time_ref, g_date_time_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GHashTable, g_hash_table_ref, g_hash_table_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GMainContext, g_main_context_ref, g_main_context_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GMainLoop, g_main_loop_ref, g_main_loop_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GMappedFile, g_mapped_file_ref, g_mapped_file_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GPtrArray, g_ptr_array_ref, g_ptr_array_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GSource, g_source_ref, g_source_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GUri, g_uri_ref, g_uri_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GVariantBuilder, g_variant_builder_ref, g_variant_builder_unref)
WTF_DEFINE_GREF_TRAITS_INLINE(GVariant, g_variant_ref_sink, g_variant_unref, g_variant_is_floating)

} // namespace WTF

using WTF::GRefPtr;
using WTF::adoptGRef;

#endif // USE(GLIB)
