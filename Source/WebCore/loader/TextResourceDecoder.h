/*
    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
    Copyright (C) 2006-2017 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.

*/

#pragma once

#include "TextEncoding.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class HTMLMetaCharsetParser;
class TextCodec;

class TextResourceDecoder : public RefCounted<TextResourceDecoder> {
public:
    enum EncodingSource {
        DefaultEncoding,
        AutoDetectedEncoding,
        EncodingFromXMLHeader,
        EncodingFromMetaTag,
        EncodingFromCSSCharset,
        EncodingFromHTTPHeader,
        UserChosenEncoding,
        EncodingFromParentFrame
    };

    WEBCORE_EXPORT static Ref<TextResourceDecoder> create(const String& mimeType, const TextEncoding& defaultEncoding = { }, bool usesEncodingDetector = false);
    WEBCORE_EXPORT ~TextResourceDecoder();

    static String textFromUTF8(const unsigned char* data, unsigned length);

    void setEncoding(const TextEncoding&, EncodingSource);
    const TextEncoding& encoding() const { return m_encoding; }
    const TextEncoding* encodingForURLParsing();

    bool hasEqualEncodingForCharset(const String& charset) const;

    WEBCORE_EXPORT String decode(const char* data, size_t length);
    String decode(const uint8_t* data, size_t length) { return decode(reinterpret_cast<const char*>(data), length); }
    WEBCORE_EXPORT String flush();

    WEBCORE_EXPORT String decodeAndFlush(const char* data, size_t length);
    String decodeAndFlush(const uint8_t* data, size_t length) { return decodeAndFlush(reinterpret_cast<const char*>(data), length); }

    void setHintEncoding(const TextResourceDecoder* parentFrameDecoder);
   
    void useLenientXMLDecoding() { m_useLenientXMLDecoding = true; }
    bool sawError() const { return m_sawError; }

private:
    TextResourceDecoder(const String& mimeType, const TextEncoding& defaultEncoding, bool usesEncodingDetector);

    enum ContentType { PlainText, HTML, XML, CSS }; // PlainText only checks for BOM.
    static ContentType determineContentType(const String& mimeType);
    static const TextEncoding& defaultEncoding(ContentType, const TextEncoding& defaultEncoding);

    size_t checkForBOM(const char*, size_t);
    bool checkForCSSCharset(const char*, size_t, bool& movedDataToBuffer);
    bool checkForHeadCharset(const char*, size_t, bool& movedDataToBuffer);
    bool checkForMetaCharset(const char*, size_t);
    void detectJapaneseEncoding(const char*, size_t);
    bool shouldAutoDetect() const;

    ContentType m_contentType;
    TextEncoding m_encoding;
    std::unique_ptr<TextCodec> m_codec;
    std::unique_ptr<HTMLMetaCharsetParser> m_charsetParser;
    EncodingSource m_source { DefaultEncoding };
    const char* m_parentFrameAutoDetectedEncoding { nullptr };
    Vector<char> m_buffer;
    bool m_checkedForBOM { false };
    bool m_checkedForCSSCharset { false };
    bool m_checkedForHeadCharset { false };
    bool m_useLenientXMLDecoding { false }; // Don't stop on XML decoding errors.
    bool m_sawError { false };
    bool m_usesEncodingDetector { false };
};

inline void TextResourceDecoder::setHintEncoding(const TextResourceDecoder* parentFrameDecoder)
{
    if (parentFrameDecoder && parentFrameDecoder->m_source == AutoDetectedEncoding)
        m_parentFrameAutoDetectedEncoding = parentFrameDecoder->encoding().name();
}

} // namespace WebCore
