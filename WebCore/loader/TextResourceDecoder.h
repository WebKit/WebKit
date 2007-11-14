/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)
    Copyright (C) 2006 Alexey Proskuryakov (ap@nypop.com)
    Copyright (C) 2006 Apple Computer, Inc.

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

#ifndef TextResourceDecoder_h
#define TextResourceDecoder_h

#include "PlatformString.h"
#include <wtf/RefCounted.h>
#include "TextDecoder.h"
#include <wtf/Vector.h>

namespace WebCore {

class TextResourceDecoder : public RefCounted<TextResourceDecoder> {
public:
    enum EncodingSource {
        DefaultEncoding,
        AutoDetectedEncoding,
        EncodingFromXMLHeader,
        EncodingFromMetaTag,
        EncodingFromCSSCharset,
        EncodingFromHTTPHeader,
        UserChosenEncoding
    };

    TextResourceDecoder(const String& mimeType, const TextEncoding& defaultEncoding = TextEncoding());
    ~TextResourceDecoder();

    void setEncoding(const TextEncoding&, EncodingSource);
    const TextEncoding& encoding() const { return m_decoder.encoding(); }

    String decode(const char* data, size_t length);
    String flush();

private:
    enum ContentType { PlainText, HTML, XML, CSS }; // PlainText is equivalent to directly using TextDecoder.
    static ContentType determineContentType(const String& mimeType);
    static const TextEncoding& defaultEncoding(ContentType, const TextEncoding& defaultEncoding);

    void checkForBOM(const char*, size_t);
    bool checkForCSSCharset(const char*, size_t, bool& movedDataToBuffer);
    bool checkForHeadCharset(const char*, size_t, bool& movedDataToBuffer);
    void detectJapaneseEncoding(const char*, size_t);

    ContentType m_contentType;
    TextDecoder m_decoder;
    EncodingSource m_source;
    Vector<char> m_buffer;
    bool m_checkedForBOM;
    bool m_checkedForCSSCharset;
    bool m_checkedForHeadCharset;
};

}

#endif
