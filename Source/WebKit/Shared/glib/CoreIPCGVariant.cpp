/*
 * Copyright (C) 2026 Igalia S.L.
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

#include "config.h"
#include "CoreIPCGVariant.h"

#if USE(GLIB)

#include <glib.h>
#include <wtf/glib/GUniquePtr.h>

namespace WebKit {

CoreIPCGVariant::CoreIPCGVariant(const GRefPtr<GVariant>& variant)
    : m_typeString(g_variant_get_type_string(variant.get()))
    , m_data(adoptGRef(g_variant_get_data_as_bytes(variant.get())))
{
}

CoreIPCGVariant::CoreIPCGVariant(CString&& typeString, std::span<const uint8_t> data)
    : m_typeString(WTF::move(typeString))
    , m_data(adoptGRef(g_bytes_new(data.data(), data.size())))
{
}

std::span<const uint8_t> CoreIPCGVariant::data() const
{
    gsize size = 0;
    const auto* data = static_cast<const uint8_t*>(g_bytes_get_data(m_data.get(), &size));
    return unsafeMakeSpan(data, size);
}

CoreIPCGVariant::operator GRefPtr<GVariant>() const
{
    GUniquePtr<GVariantType> type(g_variant_type_new(m_typeString.data()));
    return g_variant_new_from_bytes(type.get(), m_data.get(), FALSE);
}

} // namespace WebKit

#endif // USE(GLIB)
