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

// FIXME: Remove this header once rdar://109374529 is integrated.

namespace WTF {

template <typename _Tp, std::size_t _Extent = std::dynamic_extent>
using Span = typename std::span<_Tp, _Extent>;

} // namespace WTF

using WTF::Span;
