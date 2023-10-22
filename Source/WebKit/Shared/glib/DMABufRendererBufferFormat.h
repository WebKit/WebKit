/*
 * Copyright (C) 2023 Igalia S.L.
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

#if USE(GBM)

#include <wtf/EnumTraits.h>
#include <wtf/Vector.h>

namespace WebKit {

struct DMABufRendererBufferFormat {
    enum class Usage : uint8_t {
        Rendering,
        Mapping,
        Scanout
    };

    Usage usage { Usage::Rendering };
    uint32_t fourcc { 0 };
    Vector<uint64_t, 1> modifiers;
};

} // namespace WebKit

namespace WTF {

template<> struct EnumTraits<WebKit::DMABufRendererBufferFormat::Usage> {
    using values = EnumValues<
        WebKit::DMABufRendererBufferFormat::Usage,
        WebKit::DMABufRendererBufferFormat::Usage::Rendering,
        WebKit::DMABufRendererBufferFormat::Usage::Mapping,
        WebKit::DMABufRendererBufferFormat::Usage::Scanout
    >;
};

} // namespace WTF

#endif // USE(GBM)
