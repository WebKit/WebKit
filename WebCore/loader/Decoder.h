/*
    This file is part of the KDE libraries

    Copyright (C) 1999 Lars Knoll (knoll@mpi-hd.mpg.de)

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
    the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
    Boston, MA 02111-1307, USA.

*/
#ifndef Decoder_h
#define Decoder_h

#include <wtf/OwnPtr.h>
#include "TextEncoding.h"
#include "PlatformString.h"

namespace WebCore {

    class StreamingTextDecoder;    
    
/**
 * @internal
 */
class Decoder : public Shared<Decoder>
{
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
    
    Decoder(const String& mimeType, const String& defaultEncodingName = String());
    ~Decoder();

    void setEncodingName(const char* encoding, EncodingSource type);
    const char* encodingName() const;

    bool visuallyOrdered() const { return m_encoding.usesVisualOrdering(); }
    const TextEncoding& encoding() const { return m_encoding; }

    DeprecatedString decode(const char* data, int len);
    DeprecatedString flush() const;

private:
    enum ContentType {
        HTML,
        XML,
        CSS,
        PlainText // Do not look inside the document (equivalent to directly using StreamingTextDecoder)
    };

    // encoding used for decoding. default is Latin1.
    TextEncoding m_encoding;
    ContentType m_contentType;
    OwnPtr<StreamingTextDecoder> m_decoder;
    DeprecatedCString m_encodingName;
    EncodingSource m_type;

    // Our version of DeprecatedString works well for all-8-bit characters, and allows null characters.
    // This works better than DeprecatedCString when there are null characters involved.
    DeprecatedString m_buffer;

    bool m_reachedBody;
    bool m_checkedForCSSCharset;
    bool m_checkedForBOM;
};

}

#endif
