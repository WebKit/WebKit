/*
 * Copyright (C) 2004-2019 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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

#include "config.h"
#include "TextEncoding.h"

#include "DecodeEscapeSequences.h"
#include "TextCodec.h"
#include "TextEncodingRegistry.h"
#include <wtf/NeverDestroyed.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/StringView.h>

namespace PAL {

static const TextEncoding& UTF7Encoding()
{
    static NeverDestroyed<TextEncoding> globalUTF7Encoding("UTF-7");
    return globalUTF7Encoding;
}

TextEncoding::TextEncoding(const char* name)
    : m_name(atomCanonicalTextEncodingName(name))
    , m_backslashAsCurrencySymbol(backslashAsCurrencySymbol())
{
}

TextEncoding::TextEncoding(StringView name)
    : m_name(atomCanonicalTextEncodingName(name))
    , m_backslashAsCurrencySymbol(backslashAsCurrencySymbol())
{
}

TextEncoding::TextEncoding(const String& name)
    : TextEncoding(StringView { name })
{
}

String TextEncoding::decode(const char* data, size_t length, bool stopOnError, bool& sawError) const
{
    if (!m_name)
        return String();

    return newTextCodec(*this)->decode(data, length, true, stopOnError, sawError);
}

Vector<uint8_t> TextEncoding::encode(StringView string, PAL::UnencodableHandling handling, NFCNormalize normalize) const
{
    if (!m_name || string.isEmpty())
        return { };

    // FIXME: What's the right place to do normalization?
    // It's a little strange to do it inside the encode function.
    // Perhaps normalization should be an explicit step done before calling encode.
    if (normalize == NFCNormalize::Yes)
        return newTextCodec(*this)->encode(normalizedNFC(string).view, handling);
    return newTextCodec(*this)->encode(string, handling);
}

const char* TextEncoding::domName() const
{
    if (noExtendedTextEncodingNameUsed())
        return m_name;

    // We treat EUC-KR as windows-949 (its superset), but need to expose 
    // the name 'EUC-KR' because the name 'windows-949' is not recognized by
    // most Korean web servers even though they do use the encoding
    // 'windows-949' with the name 'EUC-KR'. 
    // FIXME: This is not thread-safe. At the moment, this function is
    // only accessed in a single thread, but eventually has to be made
    // thread-safe along with usesVisualOrdering().
    static const char* const a = atomCanonicalTextEncodingName("windows-949");
    if (m_name == a)
        return "EUC-KR";
    return m_name;
}

bool TextEncoding::usesVisualOrdering() const
{
    if (noExtendedTextEncodingNameUsed())
        return false;

    static const char* const a = atomCanonicalTextEncodingName("ISO-8859-8");
    return m_name == a;
}

bool TextEncoding::isJapanese() const
{
    return isJapaneseEncoding(m_name);
}

UChar TextEncoding::backslashAsCurrencySymbol() const
{
    return shouldShowBackslashAsCurrencySymbolIn(m_name) ? 0x00A5 : '\\';
}

bool TextEncoding::isNonByteBasedEncoding() const
{
    return *this == UTF16LittleEndianEncoding() || *this == UTF16BigEndianEncoding();
}

bool TextEncoding::isUTF7Encoding() const
{
    if (noExtendedTextEncodingNameUsed())
        return false;

    return *this == UTF7Encoding();
}

const TextEncoding& TextEncoding::closestByteBasedEquivalent() const
{
    if (isNonByteBasedEncoding())
        return UTF8Encoding();
    return *this; 
}

// HTML5 specifies that UTF-8 be used in form submission when a form is 
// is a part of a document in UTF-16 probably because UTF-16 is not a 
// byte-based encoding and can contain 0x00. By extension, the same
// should be done for UTF-32. In case of UTF-7, it is a byte-based encoding,
// but it's fraught with problems and we'd rather steer clear of it.
const TextEncoding& TextEncoding::encodingForFormSubmissionOrURLParsing() const
{
    if (isNonByteBasedEncoding() || isUTF7Encoding())
        return UTF8Encoding();
    return *this;
}

const TextEncoding& ASCIIEncoding()
{
    static NeverDestroyed<TextEncoding> globalASCIIEncoding("ASCII");
    return globalASCIIEncoding;
}

const TextEncoding& Latin1Encoding()
{
    static NeverDestroyed<TextEncoding> globalLatin1Encoding("latin1");
    return globalLatin1Encoding;
}

const TextEncoding& UTF16BigEndianEncoding()
{
    static NeverDestroyed<TextEncoding> globalUTF16BigEndianEncoding("UTF-16BE");
    return globalUTF16BigEndianEncoding;
}

const TextEncoding& UTF16LittleEndianEncoding()
{
    static NeverDestroyed<TextEncoding> globalUTF16LittleEndianEncoding("UTF-16LE");
    return globalUTF16LittleEndianEncoding;
}

const TextEncoding& UTF8Encoding()
{
    static NeverDestroyed<TextEncoding> globalUTF8Encoding("UTF-8");
    ASSERT(globalUTF8Encoding.get().isValid());
    return globalUTF8Encoding;
}

const TextEncoding& WindowsLatin1Encoding()
{
    static NeverDestroyed<TextEncoding> globalWindowsLatin1Encoding("WinLatin-1");
    return globalWindowsLatin1Encoding;
}

String decodeURLEscapeSequences(StringView string, const TextEncoding& encoding)
{
    if (string.isEmpty())
        return string.toString();
    return decodeEscapeSequences<URLEscapeSequence>(string, encoding);
}

} // namespace PAL
