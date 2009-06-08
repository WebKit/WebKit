/*
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 * Copyright (C) 2006, 2007, 2008, 2009 Apple Inc. All rights reserved.
 * Copyright (C) 2009 Torch Mobile Inc. http://www.torchmobile.com/
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "HTTPParsers.h"

#include "CString.h"
#include "PlatformString.h"
#include <wtf/DateMath.h>

using namespace WTF;

namespace WebCore {

// true if there is more to parse
static inline bool skipWhiteSpace(const String& str, int& pos, bool fromHttpEquivMeta)
{
    int len = str.length();

    if (fromHttpEquivMeta) {
        while (pos != len && str[pos] <= ' ')
            ++pos;
    } else {
        while (pos != len && (str[pos] == '\t' || str[pos] == ' '))
            ++pos;
    }

    return pos != len;
}

bool parseHTTPRefresh(const String& refresh, bool fromHttpEquivMeta, double& delay, String& url)
{
    int len = refresh.length();
    int pos = 0;
    
    if (!skipWhiteSpace(refresh, pos, fromHttpEquivMeta))
        return false;
    
    while (pos != len && refresh[pos] != ',' && refresh[pos] != ';')
        ++pos;
    
    if (pos == len) { // no URL
        url = String();
        bool ok;
        delay = refresh.stripWhiteSpace().toDouble(&ok);
        return ok;
    } else {
        bool ok;
        delay = refresh.left(pos).stripWhiteSpace().toDouble(&ok);
        if (!ok)
            return false;
        
        ++pos;
        skipWhiteSpace(refresh, pos, fromHttpEquivMeta);
        int urlStartPos = pos;
        if (refresh.find("url", urlStartPos, false) == urlStartPos) {
            urlStartPos += 3;
            skipWhiteSpace(refresh, urlStartPos, fromHttpEquivMeta);
            if (refresh[urlStartPos] == '=') {
                ++urlStartPos;
                skipWhiteSpace(refresh, urlStartPos, fromHttpEquivMeta);
            } else
                urlStartPos = pos;  // e.g. "Refresh: 0; url.html"
        }

        int urlEndPos = len;

        if (refresh[urlStartPos] == '"' || refresh[urlStartPos] == '\'') {
            UChar quotationMark = refresh[urlStartPos];
            urlStartPos++;
            while (urlEndPos > urlStartPos) {
                urlEndPos--;
                if (refresh[urlEndPos] == quotationMark)
                    break;
            }
        }

        url = refresh.substring(urlStartPos, urlEndPos - urlStartPos).stripWhiteSpace();
        return true;
    }
}

double parseDate(const String& value)
{
    return parseDateFromNullTerminatedCharacters(value.utf8().data());
}

String filenameFromHTTPContentDisposition(const String& value)
{
    Vector<String> keyValuePairs;
    value.split(';', keyValuePairs);

    unsigned length = keyValuePairs.size();
    for (unsigned i = 0; i < length; i++) {
        int valueStartPos = keyValuePairs[i].find('=');
        if (valueStartPos < 0)
            continue;

        String key = keyValuePairs[i].left(valueStartPos).stripWhiteSpace();

        if (key.isEmpty() || key != "filename")
            continue;
        
        String value = keyValuePairs[i].substring(valueStartPos + 1).stripWhiteSpace();

        // Remove quotes if there are any
        if (value[0] == '\"')
            value = value.substring(1, value.length() - 2);

        return value;
    }

    return String();
}

String extractMIMETypeFromMediaType(const String& mediaType)
{
    Vector<UChar, 64> mimeType;
    unsigned length = mediaType.length();
    mimeType.reserveCapacity(length);
    for (unsigned i = 0; i < length; i++) {
        UChar c = mediaType[i];

        if (c == ';')
            break;

        // While RFC 2616 does not allow it, other browsers allow multiple values in the HTTP media
        // type header field, Content-Type. In such cases, the media type string passed here may contain
        // the multiple values separated by commas. For now, this code ignores text after the first comma,
        // which prevents it from simply failing to parse such types altogether. Later for better
        // compatibility we could consider using the first or last valid MIME type instead.
        // See https://bugs.webkit.org/show_bug.cgi?id=25352 for more discussion.
        if (c == ',')
            break;

        // FIXME: The following is not correct. RFC 2616 allows linear white space before and
        // after the MIME type, but not within the MIME type itself. And linear white space
        // includes only a few specific ASCII characters; a small subset of isSpaceOrNewline.
        // See https://bugs.webkit.org/show_bug.cgi?id=8644 for a bug tracking part of this.
        if (isSpaceOrNewline(c))
            continue;

        mimeType.append(c);
    }

    if (mimeType.size() == length)
        return mediaType;
    return String(mimeType.data(), mimeType.size());
}

String extractCharsetFromMediaType(const String& mediaType)
{
    int pos = 0;
    int length = (int)mediaType.length();
    
    while (pos < length) {
        pos = mediaType.find("charset", pos, false);
        if (pos <= 0)
            return String();
        
        // is what we found a beginning of a word?
        if (mediaType[pos-1] > ' ' && mediaType[pos-1] != ';') {
            pos += 7;
            continue;
        }
        
        pos += 7;

        // skip whitespace
        while (pos != length && mediaType[pos] <= ' ')
            ++pos;
    
        if (mediaType[pos++] != '=') // this "charset" substring wasn't a parameter name, but there may be others
            continue;

        while (pos != length && (mediaType[pos] <= ' ' || mediaType[pos] == '"' || mediaType[pos] == '\''))
            ++pos;

        // we don't handle spaces within quoted parameter values, because charset names cannot have any
        int endpos = pos;
        while (pos != length && mediaType[endpos] > ' ' && mediaType[endpos] != '"' && mediaType[endpos] != '\'' && mediaType[endpos] != ';')
            ++endpos;
    
        return mediaType.substring(pos, endpos-pos);
    }
    
    return String();
}
}
