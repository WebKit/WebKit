/*
 * Copyright (C) 2009, 2010, 2011 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include "config.h"
#include "WebKitTextCodec.h"

#include "Base64.h"
#include "KURL.h"
#include "MIMETypeRegistry.h"
#include "TextCodecICU.h"
#include <wtf/Vector.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

using WebCore::TextEncoding;
using WebCore::TextCodecICU;

namespace BlackBerry {
namespace WebKit {

bool isSameEncoding(const char* encoding1, const char* encoding2)
{
    return TextEncoding(encoding1) == TextEncoding(encoding2);
}

bool isASCIICompatibleEncoding(const char* encoding)
{
    TextEncoding textEncoding(encoding);
    if (!textEncoding.isValid())
        return false;

    // Check the most common encodings first
    if (textEncoding == WebCore::UTF8Encoding() || textEncoding == WebCore::Latin1Encoding() || textEncoding == WebCore::ASCIIEncoding())
        return true;

    String lowercasedEncoding = String(encoding).lower();
    // This is slow and could easily be optimized by directly inspecting encoding[i].
    if (lowercasedEncoding.startsWith("iso-8859")
        || lowercasedEncoding.startsWith("windows")
        || lowercasedEncoding.startsWith("euc-jp")
        || lowercasedEncoding.startsWith("euc-kr"))
        return true;

    return false;
}

TranscodeResult transcode(const char* sourceEncoding, const char* targetEncoding, const char*& sourceStart, int sourceLength, char*& targetStart, unsigned int targetLength)
{
    TextEncoding textEncodingSource(sourceEncoding);
    if (!textEncodingSource.isValid())
        return SourceEncodingUnsupported;

    TextEncoding textEncodingTarget(targetEncoding);
    if (!textEncodingTarget.isValid())
        return TargetEncodingUnsupported;

    bool sawError = false;
    String ucs2 = TextCodecICU(textEncodingSource).decode(sourceStart, sourceLength, true, true, sawError);

    if (sawError)
        return SourceBroken;

    CString encoded = TextCodecICU(textEncodingTarget).encode(ucs2.characters(), ucs2.length(), WebCore::EntitiesForUnencodables);
    if (encoded.length() > targetLength)
        return TargetBufferInsufficient;

    strncpy(targetStart, encoded.data(), encoded.length());
    targetStart += encoded.length();

    return Success;
}

WebCore::Base64DecodePolicy base64DecodePolicyForWebCore(Base64DecodePolicy policy)
{
    // Must make sure Base64DecodePolicy is the same in WebKit and WebCore!
    return static_cast<WebCore::Base64DecodePolicy>(policy);
}

bool base64Decode(const std::string& base64, std::vector<char>& binary, Base64DecodePolicy policy)
{
    Vector<char> result;
    if (!WebCore::base64Decode(base64.c_str(), base64.length(), result, base64DecodePolicyForWebCore(policy)))
        return false;

    binary.insert(binary.begin(), result.begin(), result.end());
    return true;
}

bool base64Encode(const std::vector<char>& binary, std::string& base64, Base64EncodePolicy policy)
{
    Vector<char> result;
    result.append(&binary[0], binary.size());

    WebCore::base64Encode(&binary[0], binary.size(), result, Base64InsertCRLF == policy ? true : false);

    base64.clear();
    base64.append(&result[0], result.size());

    return true;
}

void unescapeURL(const std::string& escaped, std::string& url)
{
    String escapedString(escaped.data(), escaped.length());
    String urlString = WebCore::decodeURLEscapeSequences(escapedString);
    CString utf8 = urlString.utf8();

    url.clear();
    url.append(utf8.data(), utf8.length());
}

void escapeURL(const std::string& url, std::string& escaped)
{
    String urlString(url.data(), url.length());
    String escapedString = WebCore::encodeWithURLEscapeSequences(urlString);
    CString utf8 = escapedString.utf8();

    escaped.clear();
    escaped.append(utf8.data(), utf8.length());
}

bool getExtensionForMimeType(const std::string& mime, std::string& extension)
{
    String mimeType(mime.data(), mime.length());
    String preferredExtension = WebCore::MIMETypeRegistry::getPreferredExtensionForMIMEType(mimeType);
    if (preferredExtension.isEmpty())
        return false;

    CString utf8 = preferredExtension.utf8();
    extension.clear();
    extension.append(utf8.data(), utf8.length());
    return true;
}

} // namespace WebKit
} // namespace BlackBerry
