/*
 * (C) 1999 Lars Knoll (knoll@kde.org)
 * Copyright (C) 2004-2023 Apple Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#pragma once

// This file would be called String.h, but that conflicts with <string.h>
// on systems without case-sensitive file systems.
//
// There is no String class anymore: strings are RefPtr<StringImpl> (and RefPtr<AtomStringImpl> for atoms),
// with null-safe free functions declared in StringFunctions.h. This header provides those, plus the shared
// null and empty strings and a few other string helpers.

#include <span>

#include <wtf/Compiler.h>
#include <wtf/text/AtomStringImpl.h>
#include <wtf/text/IntegerToStringConversion.h>
#include <wtf/text/StringImpl.h>

#ifdef __OBJC__
#include <objc/objc.h>
#include <wtf/RetainPtr.h>
#endif

#if OS(WINDOWS)
#include <wtf/text/win/WCharStringExtras.h>
#endif

namespace WTF {

// Declarations of string operations

WTF_EXPORT_PRIVATE double charactersToDouble(std::span<const Latin1Character>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE double charactersToDouble(std::span<const char16_t>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE double charactersToFixedDouble(std::span<const Latin1Character>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE double charactersToFixedDouble(std::span<const char16_t>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const Latin1Character>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const char16_t>, bool* ok = nullptr);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const Latin1Character>, size_t& parsedLength);
WTF_EXPORT_PRIVATE float charactersToFloat(std::span<const char16_t>, size_t& parsedLength);

// Replacements for String::number(), available wherever strings are. For these argument types they take
// precedence over the variadic makeString() in MakeString.h, which produces the same strings.
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(int);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(unsigned);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(long);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(unsigned long);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(long long);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(unsigned long long);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(float);
WTF_EXPORT_PRIVATE RefPtr<StringImpl> makeString(double);

// Shared global null and empty strings, for functions that need to return a reference.
// These are immortal: the storage is a plain pointer, constant-initialized, never destroyed.
struct StaticString {
    constexpr StaticString(StringImpl::StaticStringImpl* pointer)
        : m_pointer(pointer)
    {
    }

    StringImpl::StaticStringImpl* m_pointer;
};
static_assert(sizeof(RefPtr<StringImpl>) == sizeof(StaticString), "RefPtr<StringImpl> and StaticString must be the same size!");
extern WTF_EXPORT_PRIVATE const StaticString nullStringData;
extern WTF_EXPORT_PRIVATE const StaticString emptyStringData;

inline const RefPtr<StringImpl>& nullString() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const RefPtr<StringImpl>*>(&nullStringData); }
inline const RefPtr<StringImpl>& emptyString() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const RefPtr<StringImpl>*>(&emptyStringData); }

#ifdef __OBJC__
WTF_EXPORT_PRIVATE RetainPtr<id> makeNSArrayElement(StringImpl*);
WTF_EXPORT_PRIVATE std::optional<RefPtr<StringImpl>> makeVectorElement(const RefPtr<StringImpl>*, id);
#endif

#if USE(CF)
WTF_EXPORT_PRIVATE RetainPtr<CFStringRef> makeCFArrayElement(StringImpl*);
WTF_EXPORT_PRIVATE std::optional<RefPtr<StringImpl>> makeVectorElement(const RefPtr<StringImpl>*, CFStringRef);
#endif

// Replaces each unpaired surrogate with the replacement character. Returns the argument if there are none.
WTF_EXPORT_PRIVATE RefPtr<StringImpl> replaceUnpairedSurrogatesWithReplacementCharacter(RefPtr<StringImpl>&&);
WTF_EXPORT_PRIVATE RefPtr<AtomStringImpl> replaceUnpairedSurrogatesWithReplacementCharacter(RefPtr<AtomStringImpl>&&);

inline namespace StringLiterals {

// Prefer "x"_si, which does not allocate.
inline RefPtr<StringImpl> operator""_str(const char* characters, size_t)
{
    return StringImpl::create(ASCIILiteral::fromLiteralUnsafe(characters));
}

inline RefPtr<StringImpl> operator""_str(const char16_t* characters, size_t length)
{
    return StringImpl::create(unsafeMakeSpan(characters, length));
}

} // inline StringLiterals

} // namespace WTF

using WTF::TrailingZerosPolicy;
using WTF::charactersToDouble;
using WTF::charactersToFixedDouble;
using WTF::charactersToFloat;
using WTF::emptyString;
using WTF::makeString;
using WTF::nullString;
using WTF::replaceUnpairedSurrogatesWithReplacementCharacter;
using WTF::equal;
using WTF::find;
using WTF::containsOnly;
using WTF::reverseFind;

// This also includes StringConcatenate.h and StringView.h, which in turn includes StringFunctions.h (the null-safe
// free functions on StringImpl pointers) at its end.
#include <wtf/text/AtomString.h>
