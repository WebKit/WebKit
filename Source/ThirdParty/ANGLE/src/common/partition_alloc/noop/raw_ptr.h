// Copyright 2026 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMMON_PARTITION_ALLOC_NOOP_RAW_PTR_H_
#define COMMON_PARTITION_ALLOC_NOOP_RAW_PTR_H_

// `raw_ptr<T>` is a non-owning smart pointer that has improved memory-safety
// over raw pointers. See the documentation for details:
// https://source.chromium.org/chromium/chromium/src/+/main:base/memory/raw_ptr.md
//
// Here, ANGLE provides a "no-op" implementation when PartitionAlloc is absent
// (e.g. in embedder checkouts).
//
// This file is adapted from Skia's src/partition_alloc/noop/raw_ptr.h:
// https://skia.googlesource.com/skia/+/refs/heads/main/src/partition_alloc/noop/raw_ptr.h
// and Chromium's PartitionAlloc pointers implementation:
// https://source.chromium.org/chromium/chromium/src/+/main:base/allocator/partition_allocator/src/partition_alloc/pointers/raw_ptr_noop_impl.h

#include <cstddef>
#include <cstdint>
#include <functional>
#include <type_traits>
#include <utility>

#include "common/angleutils.h"
#include "common/partition_alloc/noop/raw_ptr_exclusion.h"
#include "common/unsafe_buffers.h"

namespace partition_alloc
{
namespace internal
{
using RawPtrTraits = int;

// Disables dangling pointer detection, but keeps other raw_ptr protections.
constexpr RawPtrTraits DisableDanglingPtrDetection = 1 << 0;

// Annotates known dangling raw_ptr that haven't been triaged yet.
constexpr RawPtrTraits DanglingUntriaged = 1 << 0;

// Annotates known dangling raw_ptr that are never released.
constexpr RawPtrTraits LeakedDanglingUntriaged = 1 << 0;

// Pointer arithmetic is discouraged and disabled by default.
constexpr RawPtrTraits AllowPtrArithmetic = 1 << 3;

// This type trait verifies a type can be used as a pointer offset.
//
// We support pointer offsets in signed (ptrdiff_t) or unsigned (size_t) values.
// Smaller types are also allowed, but bool is excluded.
template <typename Z>
inline constexpr bool is_offset_type =
    std::is_integral_v<Z> && !std::is_same_v<Z, bool> && sizeof(Z) <= sizeof(ptrdiff_t);

// `raw_ptr<T>` is a non-owning smart pointer that has improved memory-safety
// over raw pointers. See the documentation for details:
// https://source.chromium.org/chromium/chromium/src/+/main:base/memory/raw_ptr.md
//
// raw_ptr<T> is marked as [[gsl::Pointer]] which allows the compiler to catch
// some bugs where the raw_ptr holds a dangling pointer to a temporary object.
// However the [[gsl::Pointer]] analysis expects that such types do not have a
// non-default move constructor/assignment. Thus, it's possible to get an error
// where the pointer is not actually dangling, and have to work around the
// compiler. We have not managed to construct such an example in Chromium yet.
template <typename T, RawPtrTraits Traits = 0>
class ANGLE_TRIVIAL_ABI ANGLE_GSL_POINTER raw_ptr
{
  public:
    ANGLE_INLINE constexpr raw_ptr() noexcept = default;

    // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
    // NOLINTNEXTLINE
    ANGLE_INLINE constexpr raw_ptr(std::nullptr_t) noexcept {}

    // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
    // NOLINTNEXTLINE
    ANGLE_INLINE constexpr raw_ptr(T *p) noexcept : wrapped_ptr_(p) {}

    ANGLE_INLINE constexpr raw_ptr(const raw_ptr &ptr) noexcept = default;

    // Contrary to T*, we do implement "zero on move". This avoids the behavior to diverge
    // depending on whether this implementation or PartitionAlloc's one is used.
    ANGLE_INLINE constexpr raw_ptr(raw_ptr &&ptr) noexcept : wrapped_ptr_(ptr.wrapped_ptr_)
    {
        ptr.wrapped_ptr_ = nullptr;
    }

    // Deliberately implicit in order to support implicit upcast.
    template <typename U,
              typename Unused = std::enable_if_t<std::is_convertible_v<U *, T *> &&
                                                 !std::is_void_v<typename std::remove_cv<T>::type>>>
    // NOLINTNEXTLINE
    ANGLE_INLINE constexpr raw_ptr(const raw_ptr<U, Traits> &ptr) noexcept : wrapped_ptr_(ptr.get())
    {}

    // Deliberately implicit in order to support implicit upcast.
    template <typename U,
              typename Unused = std::enable_if_t<std::is_convertible_v<U *, T *> &&
                                                 !std::is_void_v<typename std::remove_cv<T>::type>>>
    // NOLINTNEXTLINE
    ANGLE_INLINE constexpr raw_ptr(raw_ptr<U, Traits> &&ptr) noexcept
        : wrapped_ptr_(ptr.wrapped_ptr_)
    {
        // Contrary to T*, we do implement "zero on move". This avoids the behavior to diverge
        // depending on whether this implementation or PartitionAlloc's one is used.
        ptr.wrapped_ptr_ = nullptr;
    }

    ANGLE_INLINE constexpr raw_ptr &operator=(std::nullptr_t) noexcept
    {
        wrapped_ptr_ = nullptr;
        return *this;
    }

    ANGLE_INLINE constexpr raw_ptr &operator=(T *p) noexcept
    {
        wrapped_ptr_ = p;
        return *this;
    }

    ANGLE_INLINE constexpr raw_ptr &operator=(const raw_ptr &ptr) noexcept = default;

    ANGLE_INLINE constexpr raw_ptr &operator=(raw_ptr &&ptr) noexcept
    {
        // Contrary to T*, we do implement "zero on move". This avoids the behavior to diverge
        // depending on whether this implementation or PartitionAlloc's one is used.
        if (this != &ptr)
        {
            wrapped_ptr_     = ptr.wrapped_ptr_;
            ptr.wrapped_ptr_ = nullptr;
        }
        return *this;
    }

    // Upcast assignment
    template <typename U,
              typename Unused = std::enable_if_t<std::is_convertible_v<U *, T *> &&
                                                 !std::is_void_v<typename std::remove_cv<T>::type>>>
    ANGLE_INLINE constexpr raw_ptr &operator=(const raw_ptr<U, Traits> &ptr) noexcept
    {
        wrapped_ptr_ = ptr.wrapped_ptr_;
        return *this;
    }

    template <typename U,
              typename Unused = std::enable_if_t<std::is_convertible_v<U *, T *> &&
                                                 !std::is_void_v<typename std::remove_cv<T>::type>>>
    ANGLE_INLINE constexpr raw_ptr &operator=(raw_ptr<U, Traits> &&ptr) noexcept
    {
        wrapped_ptr_     = ptr.wrapped_ptr_;
        ptr.wrapped_ptr_ = nullptr;
        return *this;
    }

    ANGLE_INLINE constexpr explicit operator bool() const
    {
        return static_cast<bool>(wrapped_ptr_);
    }

    template <typename U      = T,
              typename Unused = std::enable_if_t<!std::is_void_v<typename std::remove_cv<U>::type>>>
    ANGLE_INLINE constexpr U &operator*() const
    {
        return *wrapped_ptr_;
    }
    ANGLE_INLINE constexpr T *operator->() const { return wrapped_ptr_; }

    // Deliberately implicit, because raw_ptr is supposed to resemble raw ptr.
    // NOLINTNEXTLINE
    ANGLE_INLINE constexpr operator T *() const { return wrapped_ptr_; }

    template <typename U>
    ANGLE_INLINE constexpr explicit operator U *() const
    {
        // This operator may be invoked from static_cast, meaning the types may not be implicitly
        // convertible, hence the need for static_cast here.
        return static_cast<U *>(wrapped_ptr_);
    }

    // The operators below deliberately perform pointer arithmetic: it is what they are for. The
    // bounds are only known by the caller, which remains subject to the unsafe buffers warnings.
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE constexpr raw_ptr &operator++()
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot increment raw_ptr unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(++wrapped_ptr_);
        return *this;
    }
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE constexpr raw_ptr &operator--()
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot decrement raw_ptr unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(--wrapped_ptr_);
        return *this;
    }
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE constexpr raw_ptr operator++(int /* post_increment */)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot increment raw_ptr unless AllowPtrArithmetic trait is present.");
        raw_ptr result = *this;
        ++(*this);
        return result;
    }
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE constexpr raw_ptr operator--(int /* post_decrement */)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot decrement raw_ptr unless AllowPtrArithmetic trait is present.");
        raw_ptr result = *this;
        --(*this);
        return result;
    }
    template <typename Z, typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE constexpr raw_ptr &operator+=(Z delta)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot increment raw_ptr unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(wrapped_ptr_ += delta);
        return *this;
    }
    template <typename Z, typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE constexpr raw_ptr &operator-=(Z delta)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot decrement raw_ptr unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(wrapped_ptr_ -= delta);
        return *this;
    }

    template <
        typename Z,
        typename U      = T,
        typename Unused = std::enable_if_t<!std::is_void_v<typename std::remove_cv<U>::type> &&
                                           partition_alloc::internal::is_offset_type<Z>>>
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE constexpr U &operator[](Z delta) const
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot index raw_ptr unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(return wrapped_ptr_[delta]);
    }

    // Stop referencing the underlying pointer and free its memory. Compared to raw delete calls,
    // this avoids the raw_ptr to be temporarily dangling during the free operation, which will lead
    // to taking the slower path that involves quarantine.
    ANGLE_INLINE constexpr void ClearAndDelete() noexcept { delete ExtractAsDangling(); }
    ANGLE_INLINE constexpr void ClearAndDeleteArray() noexcept { delete[] ExtractAsDangling(); }

    // Clear the underlying pointer and return a temporary raw_ptr instance allowed to dangle.
    ANGLE_INLINE constexpr raw_ptr<T, Traits | DisableDanglingPtrDetection>
    ExtractAsDangling() noexcept
    {
        T *ptr       = wrapped_ptr_;
        wrapped_ptr_ = nullptr;
        return raw_ptr<T, Traits | DisableDanglingPtrDetection>(ptr);
    }

    template <typename Z, typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE friend constexpr raw_ptr operator+(const raw_ptr &p,
                                                                              Z delta)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot add to raw_ptr unless AllowPtrArithmetic trait is present.");
        raw_ptr result = p;
        result += delta;
        return result;
    }
    template <typename Z, typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE friend constexpr raw_ptr operator+(Z delta,
                                                                              const raw_ptr &p)
    {
        return p + delta;
    }
    template <typename Z, typename = std::enable_if_t<partition_alloc::internal::is_offset_type<Z>>>
    ANGLE_UNSAFE_BUFFER_USAGE ANGLE_INLINE friend constexpr raw_ptr operator-(const raw_ptr &p,
                                                                              Z delta)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot subtract from raw_ptr unless AllowPtrArithmetic trait is present.");
        raw_ptr result = p;
        result -= delta;
        return result;
    }
    ANGLE_INLINE friend constexpr ptrdiff_t operator-(const raw_ptr &p1, const raw_ptr &p2)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot subtract raw_ptrs unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(return p1.wrapped_ptr_ - p2.wrapped_ptr_);
    }
    ANGLE_INLINE friend constexpr ptrdiff_t operator-(T *p1, const raw_ptr &p2)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot subtract raw_ptrs unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(return p1 - p2.wrapped_ptr_);
    }
    ANGLE_INLINE friend constexpr ptrdiff_t operator-(const raw_ptr &p1, T *p2)
    {
        static_assert((Traits & AllowPtrArithmetic) == AllowPtrArithmetic,
                      "cannot subtract raw_ptrs unless AllowPtrArithmetic trait is present.");
        // SAFETY: Bounds are the caller's responsibility, as with a raw pointer.
        ANGLE_UNSAFE_BUFFERS(return p1.wrapped_ptr_ - p2);
    }

    // Comparison operators between raw_ptr and raw_ptr<U>/U*/std::nullptr_t.
    template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
    friend bool operator==(const raw_ptr<U, R1> &lhs, const raw_ptr<V, R2> &rhs);
    template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
    friend bool operator!=(const raw_ptr<U, R1> &lhs, const raw_ptr<V, R2> &rhs);
    template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
    friend bool operator<(const raw_ptr<U, R1> &lhs, const raw_ptr<V, R2> &rhs);
    template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
    friend bool operator>(const raw_ptr<U, R1> &lhs, const raw_ptr<V, R2> &rhs);
    template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
    friend bool operator<=(const raw_ptr<U, R1> &lhs, const raw_ptr<V, R2> &rhs);
    template <typename U, typename V, RawPtrTraits R1, RawPtrTraits R2>
    friend bool operator>=(const raw_ptr<U, R1> &lhs, const raw_ptr<V, R2> &rhs);

    // Comparisons with U*. These operators also handle the case where the RHS is T*.
    template <typename U>
    ANGLE_INLINE friend bool operator==(const raw_ptr &lhs, U *rhs)
    {
        return lhs.wrapped_ptr_ == rhs;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator!=(const raw_ptr &lhs, U *rhs)
    {
        return !(lhs == rhs);
    }
    template <typename U>
    ANGLE_INLINE friend bool operator==(U *lhs, const raw_ptr &rhs)
    {
        return rhs == lhs;  // Reverse order to call the operator above.
    }
    template <typename U>
    ANGLE_INLINE friend bool operator!=(U *lhs, const raw_ptr &rhs)
    {
        return rhs != lhs;  // Reverse order to call the operator above.
    }
    template <typename U>
    ANGLE_INLINE friend bool operator<(const raw_ptr &lhs, U *rhs)
    {
        return lhs.wrapped_ptr_ < rhs;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator<=(const raw_ptr &lhs, U *rhs)
    {
        return lhs.wrapped_ptr_ <= rhs;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator>(const raw_ptr &lhs, U *rhs)
    {
        return lhs.wrapped_ptr_ > rhs;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator>=(const raw_ptr &lhs, U *rhs)
    {
        return lhs.wrapped_ptr_ >= rhs;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator<(U *lhs, const raw_ptr &rhs)
    {
        return lhs < rhs.wrapped_ptr_;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator<=(U *lhs, const raw_ptr &rhs)
    {
        return lhs <= rhs.wrapped_ptr_;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator>(U *lhs, const raw_ptr &rhs)
    {
        return lhs > rhs.wrapped_ptr_;
    }
    template <typename U>
    ANGLE_INLINE friend bool operator>=(U *lhs, const raw_ptr &rhs)
    {
        return lhs >= rhs.wrapped_ptr_;
    }

    // Comparisons with `std::nullptr_t`.
    ANGLE_INLINE friend bool operator==(const raw_ptr &lhs, std::nullptr_t) { return !lhs; }
    ANGLE_INLINE friend bool operator!=(const raw_ptr &lhs, std::nullptr_t)
    {
        return lhs.wrapped_ptr_ != nullptr;
    }
    ANGLE_INLINE friend bool operator==(std::nullptr_t, const raw_ptr &rhs) { return !rhs; }
    ANGLE_INLINE friend bool operator!=(std::nullptr_t, const raw_ptr &rhs)
    {
        return rhs.wrapped_ptr_ != nullptr;
    }

    ANGLE_INLINE friend constexpr void swap(raw_ptr &lhs, raw_ptr &rhs) noexcept
    {
        std::swap(lhs.wrapped_ptr_, rhs.wrapped_ptr_);
    }

    ANGLE_INLINE constexpr T *get() const { return wrapped_ptr_; }

  private:
    // This field is not a raw_ptr<> because we are implementing it.
    //
    // Please note we do initialize the pointers on construction. It means we don't have
    // uninitialized raw_ptr<T>. This is important if we don't want Chrome and ANGLE standalone to
    // behave differently than other embedders.
    RAW_PTR_EXCLUSION T *wrapped_ptr_ = nullptr;

    template <typename U, RawPtrTraits R>
    friend class raw_ptr;
};

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
ANGLE_INLINE bool operator==(const raw_ptr<U, Traits1> &lhs, const raw_ptr<V, Traits2> &rhs)
{
    return lhs.wrapped_ptr_ == rhs.wrapped_ptr_;
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
ANGLE_INLINE bool operator!=(const raw_ptr<U, Traits1> &lhs, const raw_ptr<V, Traits2> &rhs)
{
    return !(lhs == rhs);
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
ANGLE_INLINE bool operator<(const raw_ptr<U, Traits1> &lhs, const raw_ptr<V, Traits2> &rhs)
{
    return lhs.wrapped_ptr_ < rhs.wrapped_ptr_;
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
ANGLE_INLINE bool operator>(const raw_ptr<U, Traits1> &lhs, const raw_ptr<V, Traits2> &rhs)
{
    return lhs.wrapped_ptr_ > rhs.wrapped_ptr_;
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
ANGLE_INLINE bool operator<=(const raw_ptr<U, Traits1> &lhs, const raw_ptr<V, Traits2> &rhs)
{
    return lhs.wrapped_ptr_ <= rhs.wrapped_ptr_;
}

template <typename U, typename V, RawPtrTraits Traits1, RawPtrTraits Traits2>
ANGLE_INLINE bool operator>=(const raw_ptr<U, Traits1> &lhs, const raw_ptr<V, Traits2> &rhs)
{
    return lhs.wrapped_ptr_ >= rhs.wrapped_ptr_;
}

}  // namespace internal
}  // namespace partition_alloc

using partition_alloc::internal::AllowPtrArithmetic;
using partition_alloc::internal::DanglingUntriaged;
using partition_alloc::internal::DisableDanglingPtrDetection;
using partition_alloc::internal::LeakedDanglingUntriaged;
using partition_alloc::internal::raw_ptr;
using partition_alloc::internal::RawPtrTraits;

namespace std
{

// Override so set/map lookups do not create extra raw_ptr. This also allows dangling pointers to be
// used for lookup.
template <typename T, RawPtrTraits Traits>
struct less<raw_ptr<T, Traits>>
{
    using is_transparent = void;

    bool operator()(const raw_ptr<T, Traits> &lhs, const raw_ptr<T, Traits> &rhs) const
    {
        return lhs < rhs;
    }
    bool operator()(T *lhs, const raw_ptr<T, Traits> &rhs) const { return lhs < rhs; }
    bool operator()(const raw_ptr<T, Traits> &lhs, T *rhs) const { return lhs < rhs; }
};

template <typename T, RawPtrTraits Traits>
struct hash<raw_ptr<T, Traits>>
{
    std::size_t operator()(const raw_ptr<T, Traits> &ptr) const { return hash<T *>()(ptr.get()); }
};

// Define for cases where raw_ptr<T> holds a pointer to an array of type T. This is consistent with
// definition of std::iterator_traits<T*>. Algorithms like std::binary_search need that.
template <typename T, RawPtrTraits Traits>
struct iterator_traits<raw_ptr<T, Traits>>
{
    using difference_type   = ptrdiff_t;
    using value_type        = std::remove_cv_t<T>;
    using pointer           = T *;
    using reference         = T &;
    using iterator_category = std::random_access_iterator_tag;
};

// Specialize std::pointer_traits. The latter is required to obtain the underlying raw pointer in
// the std::to_address(pointer) overload. Implementing the pointer_traits is the standard blessed
// way to customize `std::to_address(pointer)` in C++20 [1].
//
// [1] https://wg21.link/pointer.traits.optmem
template <typename T, RawPtrTraits Traits>
struct pointer_traits<raw_ptr<T, Traits>>
{
    using pointer         = raw_ptr<T, Traits>;
    using element_type    = T;
    using difference_type = ptrdiff_t;

    template <typename U>
    using rebind = ::raw_ptr<U, Traits>;

    template <typename U = element_type, typename = std::enable_if_t<!std::is_void_v<U>>>
    static constexpr pointer pointer_to(U &r) noexcept
    {
        return pointer(&r);
    }
    static constexpr element_type *to_address(pointer p) noexcept { return p.get(); }
};

}  // namespace std

#endif  // COMMON_PARTITION_ALLOC_NOOP_RAW_PTR_H_
