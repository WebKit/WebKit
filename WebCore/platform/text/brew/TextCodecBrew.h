/*
 * Copyright (C) 2010 Company 100, Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef TextCodecBrew_h
#define TextCodecBrew_h

#include "TextCodec.h"
#include "TextEncoding.h"

typedef struct ICharsetConv ICharsetConv;

namespace WebCore {

class TextCodecBrew : public TextCodec {
public:
    static void registerBaseEncodingNames(EncodingNameRegistrar);
    static void registerBaseCodecs(TextCodecRegistrar);

    static void registerExtendedEncodingNames(EncodingNameRegistrar);
    static void registerExtendedCodecs(TextCodecRegistrar);

    TextCodecBrew(const TextEncoding&);
    virtual ~TextCodecBrew();

    virtual String decode(const char*, size_t length, bool flush, bool stopOnError, bool& sawError);
    virtual CString encode(const UChar*, size_t length, UnencodableHandling);

private:
    TextEncoding m_encoding;
    size_t m_numBufferedBytes;
    unsigned char m_bufferedBytes[16]; // bigger than any single multi-byte character
    ICharsetConv* m_charsetConverter;

    static const char* m_internalEncodingName;
};

} // namespace WebCore

#endif // TextCodecBrew_h
