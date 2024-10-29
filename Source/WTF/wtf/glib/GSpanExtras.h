/*
 *  Copyright (C) 2024 Igalia S.L.
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

#include <wtf/StdLibExtras.h>
#include <wtf/glib/GRefPtr.h>

namespace WTF {

inline std::span<const uint8_t> span(GBytes* bytes)
{
    size_t size = 0;
    const auto* ptr = static_cast<const uint8_t*>(g_bytes_get_data(bytes, &size));
    return unsafeForgeSpan<const uint8_t>(ptr, size);
}

inline std::span<const uint8_t> span(const GRefPtr<GBytes>& bytes)
{
    return span(bytes.get());
}

inline std::span<const uint8_t> span(GByteArray* array)
{
    return unsafeForgeSpan<const uint8_t>(array->data, array->len);
}

inline std::span<const uint8_t> span(const GRefPtr<GByteArray>& array)
{
    return span(array.get());
}

inline std::span<const uint8_t> span(GVariant* variant)
{
    const auto* ptr = static_cast<const uint8_t*>(g_variant_get_data(variant));
    size_t size = g_variant_get_size(variant);
    return unsafeForgeSpan<const uint8_t>(ptr, size);
}

inline std::span<const uint8_t> span(const GRefPtr<GVariant>& variant)
{
    return span(variant.get());
}

} // namespace WTF

using WTF::span;
