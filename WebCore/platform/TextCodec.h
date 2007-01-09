/*
 * Copyright (C) 2004, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef TextCodec_h
#define TextCodec_h

#include <memory>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/unicode/Unicode.h>

namespace WebCore {

    class CString;
    class String;
    class TextEncoding;

    class TextCodec : Noncopyable {
    public:
        virtual ~TextCodec();

        virtual String decode(const char*, size_t length, bool flush = false) = 0;
        virtual CString encode(const UChar*, size_t length, bool allowEntities = false) = 0;

    protected:
        static void appendOmittingBOM(Vector<UChar>&, const UChar*, size_t length);
    };

    typedef void (*EncodingNameRegistrar)(const char* alias, const char* name);

    typedef std::auto_ptr<TextCodec> (*NewTextCodecFunction)(const TextEncoding&, const void* additionalData);
    typedef void (*TextCodecRegistrar)(const char* name, NewTextCodecFunction, const void* additionalData);

} // namespace WebCore

#endif // TextCodec_h
