/*
 * Copyright (C) 2004-2022 Apple Inc. All rights reserved.
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

// There is no AtomString class anymore: atoms are RefPtr<AtomStringImpl>, compared by identity, with the
// null-safe free functions declared in StringFunctions.h. This header provides the shared null and empty atoms.

#include <utility>
#include <wtf/text/AtomStringImpl.h>
#include <wtf/text/WTFString.h>

namespace WTF {

// Shared global null and empty atoms, for functions that need to return a reference.
// These are immortal: the storage is a plain pointer, constant-initialized, never destroyed.
struct StaticAtomString {
    constexpr StaticAtomString(StringImpl::StaticStringImpl* pointer)
        : m_pointer(pointer)
    {
    }

    StringImpl::StaticStringImpl* m_pointer;
};
static_assert(sizeof(RefPtr<AtomStringImpl>) == sizeof(StaticAtomString), "RefPtr<AtomStringImpl> and StaticAtomString must be the same size!");
extern WTF_EXPORT_PRIVATE const StaticAtomString nullAtomData;
extern WTF_EXPORT_PRIVATE const StaticAtomString emptyAtomData;

inline const RefPtr<AtomStringImpl>& nullAtom() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const RefPtr<AtomStringImpl>*>(&nullAtomData); }
inline const RefPtr<AtomStringImpl>& emptyAtom() { SUPPRESS_MEMORY_UNSAFE_CAST return *reinterpret_cast<const RefPtr<AtomStringImpl>*>(&emptyAtomData); }

} // namespace WTF

using WTF::nullAtom;
using WTF::emptyAtom;

#include <wtf/text/StringConcatenate.h>
