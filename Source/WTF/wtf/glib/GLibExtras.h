/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <gio/gio.h>
#include <glib-object.h>
#include <type_traits>
#include <utility>
#include <wtf/Assertions.h>
#include <wtf/glib/GMallocString.h>
#include <wtf/glib/GRefPtr.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/ASCIILiteral.h>
#include <wtf/text/CString.h>
#include <wtf/text/UTF8CStringView.h>

// Wrappers for GLib functions that take UTF-8 strings, so that callers can pass typed strings
// instead of unwrapping them with legacyCStringPointer().

namespace WTF {

// Converts the arguments of the variadic wrappers below. Typed UTF-8 strings become the const char*
// GLib expects, and scalars, including a const char* owned by another C interface, are passed through.
// Any other type, such as a String or a Latin1CString, is rejected.
template<typename T> requires (std::is_scalar_v<std::decay_t<T>>) inline T glibVariadicType(T argument) { return argument; }
inline const char* glibVariadicType(const UTF8CString& string LIFETIME_BOUND) { return string.legacyCStringPointer(); }
inline const char* glibVariadicType(const UTF8CStringView& string LIFETIME_BOUND) { return string.utf8(); }
inline const char* glibVariadicType(ASCIILiteral string) { return string.characters(); }
inline const char* glibVariadicType(const ASCIICString& string LIFETIME_BOUND) { return string.data(); }
inline const char* glibVariadicType(const GMallocString& string LIFETIME_BOUND) { return string.utf8(); }

[[nodiscard]] inline char* gStrdup(UTF8CStringView string)
{
    return g_strdup(string.utf8());
}

inline GQuark gQuarkFromString(UTF8CStringView string)
{
    return g_quark_from_string(string.utf8());
}

inline GRefPtr<GFile> gFileNewForPath(UTF8CStringView path)
{
    return adoptGRef(g_file_new_for_path(path.utf8()));
}

inline void gValueSetString(GValue* value, UTF8CStringView string)
{
    g_value_set_string(value, string.utf8());
}

inline GVariant* gVariantNewString(UTF8CStringView string)
{
    return g_variant_new_string(string.utf8());
}

template<typename... Arguments>
GVariant* gVariantNew(const char* format, Arguments&&... arguments)
{
    return g_variant_new(format, glibVariadicType(std::forward<Arguments>(arguments))...);
}

template<typename... Arguments>
void gVariantBuilderAdd(GVariantBuilder* builder, const char* format, Arguments&&... arguments)
{
    g_variant_builder_add(builder, format, glibVariadicType(std::forward<Arguments>(arguments))...);
}

template<typename... Arguments>
void gSignalEmit(gpointer instance, guint signalID, GQuark detail, Arguments&&... arguments)
{
    g_signal_emit(instance, signalID, detail, glibVariadicType(std::forward<Arguments>(arguments))...);
}

// Supplies the terminating nullptr itself.
template<typename... Elements>
GMallocString gBuildFilename(Elements&&... elements)
{
    static_assert(sizeof...(Elements) > 0);
    return GMallocString::unsafeAdoptFromUTF8(g_build_filename(glibVariadicType(std::forward<Elements>(elements))..., nullptr));
}

} // namespace WTF

using WTF::gBuildFilename;
using WTF::gFileNewForPath;
using WTF::gQuarkFromString;
using WTF::gSignalEmit;
using WTF::gStrdup;
using WTF::gValueSetString;
using WTF::gVariantBuilderAdd;
using WTF::gVariantNew;
using WTF::gVariantNewString;

// The printf-style GLib functions have to be called directly for their format strings to be checked,
// so these wrappers are macros that convert each argument with glibVariadicType().
#define SAFE_G_VARIADIC_TYPES(...) WTF_FOR_EACH(WTF::glibVariadicType, __VA_ARGS__)

#define SAFE_G_STRDUP_PRINTF(format, ...) \
    g_strdup_printf(format __VA_OPT__(, SAFE_G_VARIADIC_TYPES(__VA_ARGS__)))

#define SAFE_G_WARNING(format, ...) \
    g_warning(format __VA_OPT__(, SAFE_G_VARIADIC_TYPES(__VA_ARGS__)))

#define SAFE_G_ERROR_NEW(domain, code, format, ...) \
    g_error_new(domain, code, format __VA_OPT__(, SAFE_G_VARIADIC_TYPES(__VA_ARGS__)))

#define SAFE_G_SET_ERROR(error, domain, code, format, ...) \
    g_set_error(error, domain, code, format __VA_OPT__(, SAFE_G_VARIADIC_TYPES(__VA_ARGS__)))

#define SAFE_G_TASK_RETURN_NEW_ERROR(task, domain, code, format, ...) \
    g_task_return_new_error(task, domain, code, format __VA_OPT__(, SAFE_G_VARIADIC_TYPES(__VA_ARGS__)))
