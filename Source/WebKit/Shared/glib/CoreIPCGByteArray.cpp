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
#include "CoreIPCGByteArray.h"

#if USE(GLIB)

#include <glib.h>
#include <wtf/glib/GSpanExtras.h>

namespace WebKit {

CoreIPCGByteArray::CoreIPCGByteArray(const GRefPtr<GByteArray>& array)
    : m_data(array)
{
}

CoreIPCGByteArray::CoreIPCGByteArray(std::span<const uint8_t> data)
    : m_data(adoptGRef(g_byte_array_sized_new(data.size())))
{
    g_byte_array_append(m_data.get(), data.data(), data.size());
}

std::span<const uint8_t> CoreIPCGByteArray::data() const
{
    return span(m_data);
}

CoreIPCGByteArray::operator GRefPtr<GByteArray>() const
{
    return m_data;
}

} // namespace WebKit

#endif // USE(GLIB)
