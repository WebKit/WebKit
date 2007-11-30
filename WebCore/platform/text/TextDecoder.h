/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef TextDecoder_h
#define TextDecoder_h

#include "PlatformString.h"
#include "TextCodec.h"
#include "TextEncoding.h"
#include <wtf/OwnPtr.h>

namespace WebCore {

    class TextCodec;

    class TextDecoder {
    public:
        TextDecoder(const TextEncoding&);
        void reset(const TextEncoding&);
        const TextEncoding& encoding() const { return m_encoding; };

        String decode(const char* data, size_t length, bool flush = false)
        {
            if (!m_checkedForBOM)
                return checkForBOM(data, length, flush);
            return m_codec->decode(data, length, flush);
        }

    private:
        String checkForBOM(const char*, size_t length, bool flush);

        TextEncoding m_encoding;
        OwnPtr<TextCodec> m_codec;

        bool m_checkedForBOM;
        unsigned char m_numBufferedBytes;
        unsigned char m_bufferedBytes[3];
    };

} // namespace WebCore

#endif // TextDecoder_h
