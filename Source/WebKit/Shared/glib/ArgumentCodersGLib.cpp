/*
 * Copyright (C) 2019 Igalia S.L.
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
#include "ArgumentCodersGLib.h"

#include "DataReference.h"
#include <glib.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace IPC {

void encode(Encoder& encoder, GVariant* variant)
{
    if (!variant) {
        encoder << CString();
        return;
    }

    encoder << CString(g_variant_get_type_string(variant));
    encoder << DataReference(static_cast<const uint8_t*>(g_variant_get_data(variant)), g_variant_get_size(variant));
}

Optional<GRefPtr<GVariant>> decode(Decoder& decoder)
{
    CString variantTypeString;
    if (!decoder.decode(variantTypeString))
        return WTF::nullopt;

    if (variantTypeString.isNull())
        return GRefPtr<GVariant>();

    if (!g_variant_type_string_is_valid(variantTypeString.data()))
        return WTF::nullopt;

    DataReference data;
    if (!decoder.decode(data))
        return WTF::nullopt;

    GUniquePtr<GVariantType> variantType(g_variant_type_new(variantTypeString.data()));
    GRefPtr<GBytes> bytes = adoptGRef(g_bytes_new(data.data(), data.size()));
    return Optional<GRefPtr<GVariant> >(g_variant_new_from_bytes(variantType.get(), bytes.get(), FALSE));
}

} // namespace IPC
