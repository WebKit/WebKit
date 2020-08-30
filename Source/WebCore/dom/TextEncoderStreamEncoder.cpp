/*
 * Copyright (C) 2020 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextEncoderStreamEncoder.h"

#include <JavaScriptCore/GenericTypedArrayViewInlines.h>
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSGenericTypedArrayViewInlines.h>

namespace WebCore {

RefPtr<Uint8Array> TextEncoderStreamEncoder::encode(const String& input)
{
    StringView view(input);

    if (!view.length())
        return nullptr;

    Vector<uint8_t> bytes(WTF::checkedProduct<size_t>(view.length() + 1, 3).unsafeGet());
    size_t bytesWritten = 0;

    for (size_t cptr = 0; cptr < view.length(); cptr++) {
        // https://encoding.spec.whatwg.org/#convert-code-unit-to-scalar-value
        auto token = view[cptr];
        if (m_pendingHighSurrogate) {
            auto highSurrogate = *m_pendingHighSurrogate;
            m_pendingHighSurrogate = WTF::nullopt;
            if (token >= 0xDC00 && token <= 0xDFFF) {
                auto codePoint = 0x10000 + ((highSurrogate - 0xD800) << 10) + (token - 0xDC00);
                U8_APPEND_UNSAFE(bytes.data(), bytesWritten, codePoint);
                continue;
            }
            U8_APPEND_UNSAFE(bytes.data(), bytesWritten, 0XFFFD);
        }
        if (token >= 0xD800 && token <= 0xDBFF) {
            m_pendingHighSurrogate = token;
            continue;
        }
        if (token >= 0xDC00 && token <= 0xDFFF) {
            U8_APPEND_UNSAFE(bytes.data(), bytesWritten, 0XFFFD);
            continue;
        }
        U8_APPEND_UNSAFE(bytes.data(), bytesWritten, token);
    }

    if (!bytesWritten)
        return nullptr;

    bytes.shrink(bytesWritten);
    return Uint8Array::tryCreate(reinterpret_cast<const uint8_t*>(bytes.data()), bytesWritten);
}

RefPtr<Uint8Array> TextEncoderStreamEncoder::flush()
{
    if (!m_pendingHighSurrogate)
        return nullptr;

    auto byteSequence = "\xEF\xBF\xBD";
    return Uint8Array::tryCreate(reinterpret_cast<const uint8_t*>(byteSequence), 3);
}

}
