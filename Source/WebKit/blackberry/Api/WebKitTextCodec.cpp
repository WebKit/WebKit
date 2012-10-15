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

#include "KURL.h"
#include "TextCodecICU.h"
#include <BlackBerryPlatformString.h>
#include <wtf/Vector.h>
#include <wtf/text/Base64.h>
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

WTF::Base64DecodePolicy base64DecodePolicyForWTF(Base64DecodePolicy policy)
{
    COMPILE_ASSERT(WTF::Base64FailOnInvalidCharacter == static_cast<WTF::Base64DecodePolicy>(Base64FailOnInvalidCharacter));
    COMPILE_ASSERT(WTF::Base64IgnoreWhitespace == static_cast<WTF::Base64DecodePolicy>(Base64IgnoreWhitespace));
    COMPILE_ASSERT(WTF::Base64IgnoreInvalidCharacters == static_cast<WTF::Base64DecodePolicy>(Base64IgnoreInvalidCharacters));
    return static_cast<WTF::Base64DecodePolicy>(policy);
}

bool base64Decode(const BlackBerry::Platform::String& base64, std::vector<char>& binary, Base64DecodePolicy policy)
{
    Vector<char> result;
    if (!WTF::base64Decode(base64.c_str(), base64.length(), result, base64DecodePolicyForWTF(policy)))
        return false;

    binary.insert(binary.begin(), result.begin(), result.end());
    return true;
}

WTF::Base64DecodePolicy base64EncodePolicyForWTF(Base64EncodePolicy policy)
{
    // FIXME: Base64InsertCRLF should be Base64InsertLFs. WTF::encodeBase64 doesn't insert CR.
    COMPILE_ASSERT(WTF::Base64DoNotInsertLFs == static_cast<WTF::Base64EncodePolicy>(Base64DoNotInsertCRLF));
    COMPILE_ASSERT(WTF::Base64InsertLFs == static_cast<WTF::Base64EncodePolicy>(Base64InsertCRLF));
    return static_cast<WTF::Base64EncodePolicy>(policy);
}

bool base64Encode(const std::vector<char>& binary, BlackBerry::Platform::String& base64, Base64EncodePolicy policy)
{
    Vector<char> result;
    result.append(&binary[0], binary.size());

    WTF::base64Encode(&binary[0], binary.size(), result, base64EncodePolicyForWTF(policy));

    base64 = Blackberry::Platform::String(&result[0], result.size());

    return true;
}

void unescapeURL(const BlackBerry::Platform::String& escaped, BlackBerry::Platform::String& url)
{
    url = WebCore::decodeURLEscapeSequences(escaped);
}

void escapeURL(const BlackBerry::Platform::String& url, BlackBerry::Platform::String& escaped)
{
    escaped = WebCore::encodeWithURLEscapeSequences(url);
}

} // namespace WebKit
} // namespace BlackBerry
