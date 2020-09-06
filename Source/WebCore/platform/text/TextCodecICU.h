/*
 * Copyright (C) 2004-2017 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
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
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#pragma once

#include "TextCodec.h"
#include <unicode/ucnv.h>

namespace WebCore {

using ICUConverterPtr = std::unique_ptr<UConverter, void (*)(UConverter*)>;

class TextCodecICU final : public TextCodec {
public:
    static void registerEncodingNames(EncodingNameRegistrar);
    static void registerCodecs(TextCodecRegistrar);

    explicit TextCodecICU(const char* encoding, const char* canonicalConverterName);
    virtual ~TextCodecICU();

private:
    String decode(const char*, size_t length, bool flush, bool stopOnError, bool& sawError) final;
    Vector<uint8_t> encode(StringView, UnencodableHandling) const final;

    void createICUConverter() const;
    void releaseICUConverter() const;
    bool needsGBKFallbacks() const { return m_needsGBKFallbacks; }
    void setNeedsGBKFallbacks(bool needsFallbacks) { m_needsGBKFallbacks = needsFallbacks; }

    int decodeToBuffer(UChar* buffer, UChar* bufferLimit, const char*& source, const char* sourceLimit, int32_t* offsets, bool flush, UErrorCode&);

    const char* const m_encodingName;
    const char* const m_canonicalConverterName;
    mutable ICUConverterPtr m_converter { nullptr, ucnv_close };
    mutable bool m_needsGBKFallbacks { false };
};

struct ICUConverterWrapper {
    WTF_MAKE_STRUCT_FAST_ALLOCATED;

    ICUConverterPtr converter { nullptr, ucnv_close };
};

} // namespace WebCore
