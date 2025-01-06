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

#include <wtf/MallocSpan.h>
#include <wtf/StdLibExtras.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>

extern "C" {
void* g_malloc(size_t);
void* g_malloc0(size_t);
void* g_realloc(void*, size_t);
void* g_try_malloc(size_t);
void* g_try_malloc0(size_t);
void* g_try_realloc(void*, size_t);
void g_strfreev(char**);
}

namespace WTF {

struct GMalloc {
    static void* malloc(size_t size) { return g_malloc(size); }
    static void* tryMalloc(size_t size) { return g_try_malloc(size); }
    static void* zeroedMalloc(size_t size) { return g_malloc0(size); }
    static void* tryZeroedMalloc(size_t size) { return g_try_malloc0(size); }
    static void* realloc(void* ptr, size_t size) { return g_realloc(ptr, size); }
    static void* tryRealloc(void* ptr, size_t size) { return g_try_realloc(ptr, size); }
    static void free(void* ptr) { g_free(ptr); }

    static constexpr ALWAYS_INLINE size_t nextCapacity(size_t capacity)
    {
        return capacity + capacity / 4 + 1;
    }
};

struct GMallocStrv {
    static void free(char** ptr) { g_strfreev(ptr); }
};

template <typename T, typename Malloc = GMalloc>
using GMallocSpan = MallocSpan<T, Malloc>;

template<typename T, typename Malloc = GMalloc>
GMallocSpan<T, Malloc> adoptGMallocSpan(std::span<T> span)
{
    return adoptMallocSpan<T, Malloc>(span);
}

WTF_EXPORT_PRIVATE GMallocSpan<char> gFileGetContents(const char* path, GUniqueOutPtr<GError>&);
WTF_EXPORT_PRIVATE GMallocSpan<char*, GMallocStrv> gKeyFileGetKeys(GKeyFile*, const char* groupName, GUniqueOutPtr<GError>&);
WTF_EXPORT_PRIVATE GMallocSpan<GParamSpec*> gObjectClassGetProperties(GObjectClass*);
WTF_EXPORT_PRIVATE GMallocSpan<const char*> gVariantGetStrv(const GRefPtr<GVariant>&);

inline std::span<const uint8_t> span(GBytes* bytes)
{
    size_t size = 0;
    const auto* ptr = static_cast<const uint8_t*>(g_bytes_get_data(bytes, &size));
    return unsafeMakeSpan<const uint8_t>(ptr, size);
}

inline std::span<const uint8_t> span(const GRefPtr<GBytes>& bytes)
{
    return span(bytes.get());
}

inline std::span<const uint8_t> span(GByteArray* array)
{
    return unsafeMakeSpan<const uint8_t>(array->data, array->len);
}

inline std::span<const uint8_t> span(const GRefPtr<GByteArray>& array)
{
    return span(array.get());
}

inline std::span<const uint8_t> span(GVariant* variant)
{
    const auto* ptr = static_cast<const uint8_t*>(g_variant_get_data(variant));
    size_t size = g_variant_get_size(variant);
    return unsafeMakeSpan<const uint8_t>(ptr, size);
}

inline std::span<const uint8_t> span(const GRefPtr<GVariant>& variant)
{
    return span(variant.get());
}

static inline std::span<char*> span(char** strv)
{
    auto size = g_strv_length(strv);
    return unsafeMakeSpan(strv, size);
}

template <typename T = void*, typename = std::enable_if_t<std::is_pointer_v<T>>>
inline std::span<T> span(GPtrArray* array)
{
    if (!array)
        return unsafeMakeSpan<T>(nullptr, 0);

    return unsafeMakeSpan(static_cast<T*>(static_cast<void*>(array->pdata)), array->len);
}

template <typename T = void*, typename = std::enable_if_t<std::is_pointer_v<T>>>
inline std::span<T> span(GRefPtr<GPtrArray>& array)
{
    return span<T>(array.get());
}

} // namespace WTF

using WTF::GMallocSpan;
using WTF::gFileGetContents;
using WTF::gKeyFileGetKeys;
using WTF::gObjectClassGetProperties;
using WTF::gVariantGetStrv;
using WTF::span;
