/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
 *           (C) 2006 Alexey Proskuryakov (ap@nypop.com)
 * Copyright (C) 2008 Torch Mobile Inc. All rights reserved. (http://www.torchmobile.com/)
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "FormDataBuilder.h"

#include "CString.h"
#include "Document.h"
#include "Frame.h"
#include "FrameLoader.h"
#include "TextEncoding.h"

#include <limits>
#include <wtf/Assertions.h>
#include <wtf/RandomNumber.h>

namespace WebCore {

FormDataBuilder::FormDataBuilder()
    : m_isPostMethod(false)
    , m_isMultiPartForm(false)
    , m_encodingType("application/x-www-form-urlencoded")
{
}

FormDataBuilder::~FormDataBuilder()
{
}

void FormDataBuilder::parseEncodingType(const String& type)
{
    if (type.contains("multipart", false) || type.contains("form-data", false)) {
        m_encodingType = "multipart/form-data";
        m_isMultiPartForm = true;
    } else if (type.contains("text", false) || type.contains("plain", false)) {
        m_encodingType = "text/plain";
        m_isMultiPartForm = false;
    } else {
        m_encodingType = "application/x-www-form-urlencoded";
        m_isMultiPartForm = false;
    }
}

void FormDataBuilder::parseMethodType(const String& type)
{
    if (equalIgnoringCase(type, "post"))
        m_isPostMethod = true;
    else if (equalIgnoringCase(type, "get"))
        m_isPostMethod = false;
}

TextEncoding FormDataBuilder::dataEncoding(Document* document) const
{
    String acceptCharset = m_acceptCharset;
    acceptCharset.replace(',', ' ');

    Vector<String> charsets;
    acceptCharset.split(' ', charsets);

    TextEncoding encoding;

    Vector<String>::const_iterator end = charsets.end();
    for (Vector<String>::const_iterator it = charsets.begin(); it != end; ++it) {
        if ((encoding = TextEncoding(*it)).isValid())
            return encoding;
    }

    if (Frame* frame = document->frame())
        return frame->loader()->encoding();

    return Latin1Encoding();
}

// Helper functions
static inline void append(Vector<char>& buffer, char string)
{
    buffer.append(string);
}

static inline void append(Vector<char>& buffer, const char* string)
{
    buffer.append(string, strlen(string));
}

static inline void append(Vector<char>& buffer, const CString& string)
{
    buffer.append(string.data(), string.length());
}

Vector<char> FormDataBuilder::generateUniqueBoundaryString()
{
    Vector<char> boundary;

    // The RFC 2046 spec says the alphanumeric characters plus the
    // following characters are legal for boundaries:  '()+_,-./:=?
    // However the following characters, though legal, cause some sites
    // to fail: (),./:= (http://bugs.webkit.org/show_bug.cgi?id=13352)
    static const char alphaNumericEncodingMap[64] = {
        0x41, 0x42, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48,
        0x49, 0x4A, 0x4B, 0x4C, 0x4D, 0x4E, 0x4F, 0x50,
        0x51, 0x52, 0x53, 0x54, 0x55, 0x56, 0x57, 0x58,
        0x59, 0x5A, 0x61, 0x62, 0x63, 0x64, 0x65, 0x66,
        0x67, 0x68, 0x69, 0x6A, 0x6B, 0x6C, 0x6D, 0x6E,
        0x6F, 0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x76,
        0x77, 0x78, 0x79, 0x7A, 0x30, 0x31, 0x32, 0x33,
        0x34, 0x35, 0x36, 0x37, 0x38, 0x39, 0x2B, 0x41
        // FIXME <rdar://problem/5252577> gmail does not accept legal characters in the form boundary
        // As stated above, some legal characters cause, sites to fail. Specifically
        // the / character which was the last character in the above array. I have
        // replaced the last character with another character already in the array
        // (notice the first and last values are both 0x41, A). Instead of picking
        // another unique legal character for boundary strings that, because it has
        // never been tested, may or may not break other sites, I simply
        // replaced / with A.  This means A is twice as likely to occur in our boundary
        // strings than any other character but I think this is fine for the time being.
        // The FIXME here is about restoring the / character once the aforementioned
        // radar has been resolved.
    };

    // Start with an informative prefix.
    append(boundary, "----WebKitFormBoundary");

    // Append 16 random 7bit ascii AlphaNumeric characters.
    Vector<char> randomBytes;

    for (unsigned i = 0; i < 4; ++i) {
        unsigned randomness = static_cast<unsigned>(WTF::randomNumber() * (std::numeric_limits<unsigned>::max() + 1.0));
        randomBytes.append(alphaNumericEncodingMap[(randomness >> 24) & 0x3F]);
        randomBytes.append(alphaNumericEncodingMap[(randomness >> 16) & 0x3F]);
        randomBytes.append(alphaNumericEncodingMap[(randomness >> 8) & 0x3F]);
        randomBytes.append(alphaNumericEncodingMap[randomness & 0x3F]);
    }

    boundary.append(randomBytes);
    boundary.append(0); // Add a 0 at the end so we can use this as a C-style string.
    return boundary;
}

void FormDataBuilder::beginMultiPartHeader(Vector<char>& buffer, const CString& boundary, const CString& name)
{
    addBoundaryToMultiPartHeader(buffer, boundary);

    append(buffer, "Content-Disposition: form-data; name=\"");
    append(buffer, name);
    append(buffer, '"');
}

void FormDataBuilder::addBoundaryToMultiPartHeader(Vector<char>& buffer, const CString& boundary, bool isLastBoundary)
{
    append(buffer, "--");
    append(buffer, boundary);

    if (isLastBoundary)
        append(buffer, "--");

    append(buffer, "\r\n");
}

void FormDataBuilder::addFilenameToMultiPartHeader(Vector<char>& buffer, const TextEncoding& encoding, const String& filename)
{
    // FIXME: This won't work if the filename includes a " mark,
    // or control characters like CR or LF. This also does strange
    // things if the filename includes characters you can't encode
    // in the website's character set.
    append(buffer, "; filename=\"");
    append(buffer, encoding.encode(filename.characters(), filename.length(), QuestionMarksForUnencodables));
    append(buffer, '"');
}

void FormDataBuilder::addContentTypeToMultiPartHeader(Vector<char>& buffer, const CString& mimeType)
{
    append(buffer, "\r\nContent-Type: ");
    append(buffer, mimeType);
}

void FormDataBuilder::finishMultiPartHeader(Vector<char>& buffer)
{
    append(buffer, "\r\n\r\n");
}

void FormDataBuilder::addKeyValuePairAsFormData(Vector<char>& buffer, const CString& key, const CString& value)
{
    if (!buffer.isEmpty())
        append(buffer, '&');

    encodeStringAsFormData(buffer, key);
    append(buffer, '=');
    encodeStringAsFormData(buffer, value);
}

void FormDataBuilder::encodeStringAsFormData(Vector<char>& buffer, const CString& string)
{
    static const char hexDigits[17] = "0123456789ABCDEF";

    // Same safe characters as Netscape for compatibility.
    static const char safeCharacters[] = "-._*";

    // http://www.w3.org/TR/html4/interact/forms.html#h-17.13.4.1
    unsigned length = string.length();
    for (unsigned i = 0; i < length; ++i) {
        unsigned char c = string.data()[i];

        if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || strchr(safeCharacters, c))
            append(buffer, c);
        else if (c == ' ')
            append(buffer, '+');
        else if (c == '\n' || (c == '\r' && (i + 1 >= length || string.data()[i + 1] != '\n')))
            append(buffer, "%0D%0A");
        else if (c != '\r') {
            append(buffer, '%');
            append(buffer, hexDigits[c >> 4]);
            append(buffer, hexDigits[c & 0xF]);
        }
    }
}

}
