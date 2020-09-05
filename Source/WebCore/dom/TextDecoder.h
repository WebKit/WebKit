/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "BufferSource.h"
#include "ExceptionOr.h"
#include "TextEncoding.h"
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class TextCodec;

class TextDecoder : public RefCounted<TextDecoder> {
public:
    struct Options {
        bool fatal { false };
        bool ignoreBOM { false };
    };
    struct DecodeOptions {
        bool stream { false };
    };
    
    static ExceptionOr<Ref<TextDecoder>> create(const String& label, Options);
    ~TextDecoder();

    String encoding() const;
    bool fatal() const { return m_options.fatal; }
    bool ignoreBOM() const { return m_options.ignoreBOM; }
    ExceptionOr<String> decode(Optional<BufferSource::VariantType>, DecodeOptions);

private:
    TextDecoder(const char*, Options);

    enum class WaitForMoreBOMBytes : bool { No, Yes };
    WaitForMoreBOMBytes ignoreBOMIfNecessary(const uint8_t*& data, size_t& length, bool stream);
    size_t bytesNeededForFullBOMIgnoreCheck() const;
    bool isBeginningOfIncompleteBOM(const uint8_t*, size_t) const;

    const TextEncoding m_textEncoding;
    std::unique_ptr<TextCodec> m_codec;
    Options m_options;
    Vector<uint8_t> m_buffer;
    bool m_bomIgnoredIfNecessary { false };
};

}
