/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

#include <wtf/text/StringHasher.h>
#include <wtf/text/WYHash.h>

namespace WTF {

template<typename T, typename Converter>
unsigned StringHasher::computeHashAndMaskTop8Bits(std::span<const T> data)
{
    return WYHash::computeHashAndMaskTop8Bits<T, Converter>(data);
}

template<typename T, unsigned characterCount>
constexpr unsigned StringHasher::computeLiteralHashAndMaskTop8Bits(const T (&characters)[characterCount])
{
    constexpr unsigned characterCountWithoutNull = characterCount - 1;
    return WYHash::computeHashAndMaskTop8Bits<T>(unsafeMakeSpan(characters, characterCountWithoutNull));
}

} // namespace WTF

using WTF::StringHasher;
