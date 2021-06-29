// -*- C++ -*-
//===------------------------------ span ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#ifndef _WTF_LIBCPP_SPAN
#define _WTF_LIBCPP_SPAN

// Imports a copy of <span> from e892705d74c7366a1404a3b3471001edaa7659f8 of the
// libc++ project (https://github.com/llvm/llvm-project.git) and modifies it to
// work on top of any standard library implementation.
//
// - Renames macros with _LIBCPP_ prefix to use the prefix _WTF_LIBCPP_
// - Renames std::span to WTF::Span
// - Renames std::as_bytes/std::as_writable_bytes to WTF::asBytes/WTF::asWritableBytes.
// - Stop #including internal libc++ headers.
// - Remove check for > c++17.
// - Remove push/pop availability of min/max macros.

#include <array>        // for array
#include <cstddef>      // for byte
#include <iterator>     // for iterators
#include <type_traits>  // for remove_cv, etc

// Adds some macro defines usually defined in __config.

#define _WTF_LIBCPP_ABI_SPAN_POINTER_ITERATORS
#define _WTF_LIBCPP_BEGIN_NAMESPACE namespace WTF {
#define _WTF_LIBCPP_DEBUG_LEVEL 0
#define _WTF_LIBCPP_END_NAMESPACE  }
#define _WTF_LIBCPP_HAS_NO_RANGES
#define _WTF_LIBCPP_INLINE_VISIBILITY
#define _WTF_LIBCPP_TEMPLATE_VIS
#define _WTF_VSTD std

#include <wtf/Assertions.h>
#define _WTF_LIBCPP_ASSERT ASSERT

_WTF_LIBCPP_BEGIN_NAMESPACE

inline constexpr std::size_t dynamic_extent = std::numeric_limits<std::size_t>::max();
template <typename _Tp, std::size_t _Extent = dynamic_extent> class Span;


template <class _Tp>
struct __is_span_impl : public std::false_type {};

template <class _Tp, std::size_t _Extent>
struct __is_span_impl<Span<_Tp, _Extent>> : public std::true_type {};

template <class _Tp>
struct __is_span : public __is_span_impl<std::remove_cv_t<_Tp>> {};

template <class _Tp>
struct __is_std_array_impl : public std::false_type {};

template <class _Tp, std::size_t _Sz>
struct __is_std_array_impl<std::array<_Tp, _Sz>> : public std::true_type {};

template <class _Tp>
struct __is_std_array : public __is_std_array_impl<std::remove_cv_t<_Tp>> {};

template <class _Tp, class _ElementType, class = void>
struct __is_span_compatible_container : public std::false_type {};

template <class _Tp, class _ElementType>
struct __is_span_compatible_container<_Tp, _ElementType,
        std::void_t<
        // is not a specialization of Span
            typename std::enable_if<!__is_span<_Tp>::value, std::nullptr_t>::type,
        // is not a specialization of array
            typename std::enable_if<!__is_std_array<_Tp>::value, std::nullptr_t>::type,
        // sd::is_array_v<Container> is false,
            typename std::enable_if<!std::is_array_v<_Tp>, std::nullptr_t>::type,
        // std::data(cont) and std::size(cont) are well formed
            decltype(std::data(std::declval<_Tp>())),
            decltype(std::size(std::declval<_Tp>())),
        // remove_pointer_t<decltype(data(cont))>(*)[] is convertible to ElementType(*)[]
            typename std::enable_if<
                std::is_convertible_v<std::remove_pointer_t<decltype(std::data(std::declval<_Tp &>()))>(*)[],
                                 _ElementType(*)[]>,
                std::nullptr_t>::type
        >>
    : public std::true_type {};


template <typename _Tp, std::size_t _Extent>
class _WTF_LIBCPP_TEMPLATE_VIS Span {
public:
//  constants and types
    using element_type           = _Tp;
    using value_type             = std::remove_cv_t<_Tp>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using pointer                = _Tp *;
    using const_pointer          = const _Tp *;
    using reference              = _Tp &;
    using const_reference        = const _Tp &;
#if (_WTF_LIBCPP_DEBUG_LEVEL == 2) || defined(_WTF_LIBCPP_ABI_SPAN_POINTER_ITERATORS)
    using iterator               = pointer;
#else
    using iterator               = __wrap_iter<pointer>;
#endif
    using reverse_iterator       = _WTF_VSTD::reverse_iterator<iterator>;

    static constexpr size_type extent = _Extent;

// [span.cons], span constructors, copy, assignment, and destructor
    template <std::size_t _Sz = _Extent, std::enable_if_t<_Sz == 0, std::nullptr_t> = nullptr>
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr Span() noexcept : __data{nullptr} {}

    constexpr Span           (const Span&) noexcept = default;
    constexpr Span& operator=(const Span&) noexcept = default;

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr explicit Span(pointer __ptr, size_type __count) : __data{__ptr}
        { (void)__count; _WTF_LIBCPP_ASSERT(_Extent == __count, "size mismatch in Span's constructor (ptr, len)"); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr explicit Span(pointer __f, pointer __l) : __data{__f}
        { (void)__l;     _WTF_LIBCPP_ASSERT(_Extent == std::distance(__f, __l), "size mismatch in Span's constructor (ptr, ptr)"); }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr Span(element_type (&__arr)[_Extent])          noexcept : __data{__arr} {}

    template <class _OtherElementType,
              std::enable_if_t<std::is_convertible_v<_OtherElementType(*)[], element_type (*)[]>, std::nullptr_t> = nullptr>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span(std::array<_OtherElementType, _Extent>& __arr) noexcept : __data{__arr.data()} {}

    template <class _OtherElementType,
              std::enable_if_t<std::is_convertible_v<const _OtherElementType(*)[], element_type (*)[]>, std::nullptr_t> = nullptr>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span(const std::array<_OtherElementType, _Extent>& __arr) noexcept : __data{__arr.data()} {}

    template <class _Container>
    _WTF_LIBCPP_INLINE_VISIBILITY
        constexpr explicit Span(      _Container& __c,
            std::enable_if_t<__is_span_compatible_container<_Container, _Tp>::value, std::nullptr_t> = nullptr)
        : __data{_WTF_VSTD::data(__c)} {
            _WTF_LIBCPP_ASSERT(_Extent == _WTF_VSTD::size(__c), "size mismatch in Span's constructor (range)");
        }

    template <class _Container>
    _WTF_LIBCPP_INLINE_VISIBILITY
        constexpr explicit Span(const _Container& __c,
            std::enable_if_t<__is_span_compatible_container<const _Container, _Tp>::value, std::nullptr_t> = nullptr)
        : __data{_WTF_VSTD::data(__c)} {
            _WTF_LIBCPP_ASSERT(_Extent == _WTF_VSTD::size(__c), "size mismatch in Span's constructor (range)");
        }

    template <class _OtherElementType>
    _WTF_LIBCPP_INLINE_VISIBILITY
        constexpr Span(const Span<_OtherElementType, _Extent>& __other,
                       std::enable_if_t<
                          std::is_convertible_v<_OtherElementType(*)[], element_type (*)[]>,
                          std::nullptr_t> = nullptr)
        : __data{__other.data()} {}

    template <class _OtherElementType>
    _WTF_LIBCPP_INLINE_VISIBILITY
        constexpr explicit Span(const Span<_OtherElementType>& __other,
                       std::enable_if_t<
                          std::is_convertible_v<_OtherElementType(*)[], element_type (*)[]>,
                          std::nullptr_t> = nullptr) noexcept
        : __data{__other.data()} { _WTF_LIBCPP_ASSERT(_Extent == __other.size(), "size mismatch in Span's constructor (other Span)"); }


//  ~Span() noexcept = default;

    template <std::size_t _Count>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, _Count> first() const noexcept
    {
        static_assert(_Count <= _Extent, "Count out of range in Span::first()");
        return Span<element_type, _Count>{data(), _Count};
    }

    template <std::size_t _Count>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, _Count> last() const noexcept
    {
        static_assert(_Count <= _Extent, "Count out of range in Span::last()");
        return Span<element_type, _Count>{data() + size() - _Count, _Count};
    }

    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, dynamic_extent> first(size_type __count) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__count <= size(), "Count out of range in Span::first(count)");
        return {data(), __count};
    }

    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, dynamic_extent> last(size_type __count) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__count <= size(), "Count out of range in Span::last(count)");
        return {data() + size() - __count, __count};
    }

    template <std::size_t _Offset, std::size_t _Count = dynamic_extent>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr auto subspan() const noexcept
        -> Span<element_type, _Count != dynamic_extent ? _Count : _Extent - _Offset>
    {
        static_assert(_Offset <= _Extent, "Offset out of range in Span::subspan()");
        static_assert(_Count == dynamic_extent || _Count <= _Extent - _Offset, "Offset + count out of range in Span::subspan()");

        using _ReturnType = Span<element_type, _Count != dynamic_extent ? _Count : _Extent - _Offset>;
        return _ReturnType{data() + _Offset, _Count == dynamic_extent ? size() - _Offset : _Count};
    }


    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, dynamic_extent>
       subspan(size_type __offset, size_type __count = dynamic_extent) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__offset <= size(), "Offset out of range in Span::subspan(offset, count)");
        _WTF_LIBCPP_ASSERT(__count  <= size() || __count == dynamic_extent, "Count out of range in Span::subspan(offset, count)");
        if (__count == dynamic_extent)
            return {data() + __offset, size() - __offset};
        _WTF_LIBCPP_ASSERT(__count <= size() - __offset, "Offset + count out of range in Span::subspan(offset, count)");
        return {data() + __offset, __count};
    }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr size_type size()       const noexcept { return _Extent; }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr size_type size_bytes() const noexcept { return _Extent * sizeof(element_type); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr bool empty()           const noexcept { return _Extent == 0; }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reference operator[](size_type __idx) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__idx < size(), "Span<T,N>[] index out of bounds");
        return __data[__idx];
    }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reference front() const noexcept
    {
        _WTF_LIBCPP_ASSERT(!empty(), "Span<T, N>::front() on empty Span");
        return __data[0];
    }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reference back() const noexcept
    {
        _WTF_LIBCPP_ASSERT(!empty(), "Span<T, N>::back() on empty Span");
        return __data[size()-1];
    }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr pointer data()                         const noexcept { return __data; }

// [span.iter], Span iterator support
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr iterator                 begin() const noexcept { return iterator(data()); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr iterator                   end() const noexcept { return iterator(data() + size()); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reverse_iterator        rbegin() const noexcept { return reverse_iterator(end()); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reverse_iterator          rend() const noexcept { return reverse_iterator(begin()); }

private:
    pointer    __data;

};


template <typename _Tp>
class _WTF_LIBCPP_TEMPLATE_VIS Span<_Tp, dynamic_extent> {
private:

public:
//  constants and types
    using element_type           = _Tp;
    using value_type             = std::remove_cv_t<_Tp>;
    using size_type              = std::size_t;
    using difference_type        = std::ptrdiff_t;
    using pointer                = _Tp *;
    using const_pointer          = const _Tp *;
    using reference              = _Tp &;
    using const_reference        = const _Tp &;
#if (_WTF_LIBCPP_DEBUG_LEVEL == 2) || defined(_WTF_LIBCPP_ABI_SPAN_POINTER_ITERATORS)
    using iterator               = pointer;
#else
    using iterator               = __wrap_iter<pointer>;
#endif
    using reverse_iterator       = _WTF_VSTD::reverse_iterator<iterator>;

    static constexpr size_type extent = dynamic_extent;

// [span.cons], Span constructors, copy, assignment, and destructor
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr Span() noexcept : __data{nullptr}, __size{0} {}

    constexpr Span           (const Span&) noexcept = default;
    constexpr Span& operator=(const Span&) noexcept = default;

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr Span(pointer __ptr, size_type __count) : __data{__ptr}, __size{__count} {}
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr Span(pointer __f, pointer __l) : __data{__f}, __size{static_cast<std::size_t>(std::distance(__f, __l))} {}

    template <std::size_t _Sz>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span(element_type (&__arr)[_Sz])          noexcept : __data{__arr}, __size{_Sz} {}

    template <class _OtherElementType, std::size_t _Sz,
              std::enable_if_t<std::is_convertible_v<_OtherElementType(*)[], element_type (*)[]>, std::nullptr_t> = nullptr>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span(std::array<_OtherElementType, _Sz>& __arr) noexcept : __data{__arr.data()}, __size{_Sz} {}

    template <class _OtherElementType, std::size_t _Sz,
              std::enable_if_t<std::is_convertible_v<const _OtherElementType(*)[], element_type (*)[]>, std::nullptr_t> = nullptr>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span(const std::array<_OtherElementType, _Sz>& __arr) noexcept : __data{__arr.data()}, __size{_Sz} {}

    template <class _Container>
    _WTF_LIBCPP_INLINE_VISIBILITY
        constexpr Span(      _Container& __c,
            std::enable_if_t<__is_span_compatible_container<_Container, _Tp>::value, std::nullptr_t> = nullptr)
        : __data{_WTF_VSTD::data(__c)}, __size{(size_type) _WTF_VSTD::size(__c)} {}

    template <class _Container>
    _WTF_LIBCPP_INLINE_VISIBILITY
        constexpr Span(const _Container& __c,
            std::enable_if_t<__is_span_compatible_container<const _Container, _Tp>::value, std::nullptr_t> = nullptr)
        : __data{_WTF_VSTD::data(__c)}, __size{(size_type) _WTF_VSTD::size(__c)} {}


    template <class _OtherElementType, std::size_t _OtherExtent>
    _WTF_LIBCPP_INLINE_VISIBILITY
        constexpr Span(const Span<_OtherElementType, _OtherExtent>& __other,
                       std::enable_if_t<
                          std::is_convertible_v<_OtherElementType(*)[], element_type (*)[]>,
                          std::nullptr_t> = nullptr) noexcept
        : __data{__other.data()}, __size{__other.size()} {}

//    ~Span() noexcept = default;

    template <std::size_t _Count>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, _Count> first() const noexcept
    {
        _WTF_LIBCPP_ASSERT(_Count <= size(), "Count out of range in Span::first()");
        return Span<element_type, _Count>{data(), _Count};
    }

    template <std::size_t _Count>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, _Count> last() const noexcept
    {
        _WTF_LIBCPP_ASSERT(_Count <= size(), "Count out of range in Span::last()");
        return Span<element_type, _Count>{data() + size() - _Count, _Count};
    }

    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, dynamic_extent> first(size_type __count) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__count <= size(), "Count out of range in Span::first(count)");
        return {data(), __count};
    }

    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, dynamic_extent> last (size_type __count) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__count <= size(), "Count out of range in Span::last(count)");
        return {data() + size() - __count, __count};
    }

    template <std::size_t _Offset, std::size_t _Count = dynamic_extent>
    _WTF_LIBCPP_INLINE_VISIBILITY
    constexpr Span<element_type, _Count> subspan() const noexcept
    {
        _WTF_LIBCPP_ASSERT(_Offset <= size(), "Offset out of range in Span::subspan()");
        _WTF_LIBCPP_ASSERT(_Count == dynamic_extent || _Count <= size() - _Offset, "Offset + count out of range in Span::subspan()");
        return Span<element_type, _Count>{data() + _Offset, _Count == dynamic_extent ? size() - _Offset : _Count};
    }

    constexpr Span<element_type, dynamic_extent>
    _WTF_LIBCPP_INLINE_VISIBILITY
    subspan(size_type __offset, size_type __count = dynamic_extent) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__offset <= size(), "Offset out of range in Span::subspan(offset, count)");
        _WTF_LIBCPP_ASSERT(__count  <= size() || __count == dynamic_extent, "count out of range in Span::subspan(offset, count)");
        if (__count == dynamic_extent)
            return {data() + __offset, size() - __offset};
        _WTF_LIBCPP_ASSERT(__count <= size() - __offset, "Offset + count out of range in Span::subspan(offset, count)");
        return {data() + __offset, __count};
    }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr size_type size()       const noexcept { return __size; }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr size_type size_bytes() const noexcept { return __size * sizeof(element_type); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr bool empty()           const noexcept { return __size == 0; }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reference operator[](size_type __idx) const noexcept
    {
        _WTF_LIBCPP_ASSERT(__idx < size(), "Span<T>[] index out of bounds");
        return __data[__idx];
    }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reference front() const noexcept
    {
        _WTF_LIBCPP_ASSERT(!empty(), "Span<T>[].front() on empty Span");
        return __data[0];
    }

    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reference back() const noexcept
    {
        _WTF_LIBCPP_ASSERT(!empty(), "Span<T>[].back() on empty Span");
        return __data[size()-1];
    }


    _WTF_LIBCPP_INLINE_VISIBILITY constexpr pointer data()                         const noexcept { return __data; }

// [span.iter], Span iterator support
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr iterator                 begin() const noexcept { return iterator(data()); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr iterator                   end() const noexcept { return iterator(data() + size()); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reverse_iterator        rbegin() const noexcept { return reverse_iterator(end()); }
    _WTF_LIBCPP_INLINE_VISIBILITY constexpr reverse_iterator          rend() const noexcept { return reverse_iterator(begin()); }

private:
    pointer   __data;
    size_type __size;
};

#if !defined(_WTF_LIBCPP_HAS_NO_RANGES)
template <class _Tp, std::size_t _Extent>
inline constexpr bool std::ranges::enable_borrowed_range<Span<_Tp, _Extent> > = true;
#endif // !defined(_WTF_LIBCPP_HAS_NO_RANGES)

//  asBytes & asWritableBytes
template <class _Tp, std::size_t _Extent>
_WTF_LIBCPP_INLINE_VISIBILITY
auto asBytes(Span<_Tp, _Extent> __s) noexcept
-> Span<const std::byte, _Extent == dynamic_extent ? dynamic_extent : _Extent * sizeof(_Tp)>
{ return { reinterpret_cast<const std::byte *>(__s.data()), __s.size_bytes() }; }

template <class _Tp, std::size_t _Extent>
_WTF_LIBCPP_INLINE_VISIBILITY
auto asWritableBytes(Span<_Tp, _Extent> __s) noexcept
-> std::enable_if_t<!std::is_const_v<_Tp>, Span<std::byte, _Extent == dynamic_extent ? dynamic_extent : _Extent * sizeof(_Tp)>>
{ return { reinterpret_cast<std::byte *>(__s.data()), __s.size_bytes() }; }

//  Deduction guides
template<class _Tp, std::size_t _Sz>
    Span(_Tp (&)[_Sz]) -> Span<_Tp, _Sz>;

template<class _Tp, std::size_t _Sz>
    Span(std::array<_Tp, _Sz>&) -> Span<_Tp, _Sz>;

template<class _Tp, std::size_t _Sz>
    Span(const std::array<_Tp, _Sz>&) -> Span<const _Tp, _Sz>;

template<class _Container>
    Span(_Container&) -> Span<typename _Container::value_type>;

template<class _Container>
    Span(const _Container&) -> Span<const typename _Container::value_type>;

_WTF_LIBCPP_END_NAMESPACE

using WTF::Span;
using WTF::asBytes;
using WTF::asWritableBytes;

#endif // _WTF_LIBCPP_SPAN
