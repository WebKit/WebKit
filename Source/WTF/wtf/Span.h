// -*- C++ -*-
//===------------------------------ span ---------------------------------===//
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
//
//===---------------------------------------------------------------------===//

#pragma once

#include <span>

namespace WTF {

inline constexpr std::size_t dynamic_extent = std::dynamic_extent;

template <typename _Tp, std::size_t _Extent = std::dynamic_extent>
using Span = typename std::span<_Tp, _Extent>;

template <typename _Tp>
auto makeSpan(_Tp* __ptr, std::size_t __count)
{
    return std::span(__ptr, __count);
}

template <typename _Tp>
auto makeSpan(_Tp* __f, _Tp* __l)
{
    return std::span(__f, __l);
}

template<typename _Element_Type, std::size_t _Extent>
auto makeSpan(std::array<_Element_Type, _Extent>& __a)
{
    return std::span(__a);
}

template<typename _Element_Type, std::size_t _Extent>
auto makeSpan(const std::array<_Element_Type, _Extent>& __a)
{
    return std::span(__a);
}

template<class _Container>
auto makeSpan(_Container& __c)
{
    return std::span<typename _Container::value_type>(std::data(__c), std::size(__c));
}

template<class _Container>
auto makeSpan(const _Container& __c)
{
    return std::span<typename _Container::value_type>(std::data(__c), std::size(__c));
}

template <class _Tp, std::size_t _Extent>
auto asBytes(std::span<_Tp, _Extent> __s) { return std::as_bytes(__s); }

template <class _Tp, std::size_t _Extent>
auto asWritableBytes(std::span<_Tp, _Extent> __s) { return std::as_writable_bytes(__s); }

} // namespace WTF

using WTF::Span;
using WTF::asBytes;
using WTF::asWritableBytes;
using WTF::makeSpan;
