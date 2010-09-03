/*
 * Copyright (C) 2007-2009 Torch Mobile, Inc. All rights reserved.
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
 *  This library is distributed in the hope that i will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "TextCodecWinCE.h"

#include "FontCache.h"
#include "PlatformString.h"
#include <mlang.h>
#include <winbase.h>
#include <winnls.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringHash.h>
#include <wtf/unicode/UTF8.h>

namespace WebCore {

struct CharsetInfo {
    CString m_name;
    String m_friendlyName;
    UINT m_codePage;
    Vector<CString> m_aliases;
};

class LanguageManager {
private:
    LanguageManager();

    friend LanguageManager& languageManager();
};

// Usage: a lookup table used to get CharsetInfo with code page ID.
// Key: code page ID. Value: charset information.
static HashMap<UINT, CString>& codePageCharsets()
{
    static HashMap<UINT, CString> cc;
    return cc;
}

static HashMap<String, CharsetInfo>& knownCharsets()
{
    static HashMap<String, CharsetInfo> kc;
    return kc;
}

// Usage: a map that stores charsets that are supported by system. Sorted by name.
// Key: charset. Value: code page ID.
typedef HashSet<String> CharsetSet;
static CharsetSet& supportedCharsets()
{
    static CharsetSet sl;
    return sl;
}

static LanguageManager& languageManager()
{
    static LanguageManager lm;
    return lm;
}

LanguageManager::LanguageManager()
{
    IEnumCodePage* enumInterface;
    IMultiLanguage* mli = FontCache::getMultiLanguageInterface();
    if (mli && S_OK == mli->EnumCodePages(MIMECONTF_BROWSER, &enumInterface)) {
        MIMECPINFO cpInfo;
        ULONG ccpInfo;
        while (S_OK == enumInterface->Next(1, &cpInfo, &ccpInfo) && ccpInfo) {
            if (!IsValidCodePage(cpInfo.uiCodePage))
                continue;

            HashMap<UINT, CString>::iterator i = codePageCharsets().find(cpInfo.uiCodePage);

            CString name(String(cpInfo.wszWebCharset).latin1());
            if (i == codePageCharsets().end()) {
                CharsetInfo info;
                info.m_codePage = cpInfo.uiCodePage;
                knownCharsets().set(name.data(), info);
                i = codePageCharsets().set(cpInfo.uiCodePage, name).first;
            }
            if (i != codePageCharsets().end()) {
                HashMap<String, CharsetInfo>::iterator j = knownCharsets().find(String(i->second.data(), i->second.length()));
                ASSERT(j != knownCharsets().end());
                CharsetInfo& info = j->second;
                info.m_name = i->second.data();
                info.m_friendlyName = cpInfo.wszDescription;
                info.m_aliases.append(name);
                info.m_aliases.append(String(cpInfo.wszHeaderCharset).latin1());
                info.m_aliases.append(String(cpInfo.wszBodyCharset).latin1());
                String cpName = String::format("cp%d", cpInfo.uiCodePage);
                info.m_aliases.append(cpName.latin1());
                supportedCharsets().add(i->second.data());
            }
        }
        enumInterface->Release();
    }
}

static UINT getCodePage(const char* name)
{
    if (!strcmp(name, "UTF-8"))
        return CP_UTF8;

    // Explicitly use a "const" reference to fix the silly VS build error
    // saying "==" is not found for const_iterator and iterator
    const HashMap<String, CharsetInfo>& charsets = knownCharsets();
    HashMap<String, CharsetInfo>::const_iterator i = charsets.find(name);
    return i == charsets.end() ? CP_ACP : i->second.m_codePage;
}

static PassOwnPtr<TextCodec> newTextCodecWinCE(const TextEncoding& encoding, const void*)
{
    return new TextCodecWinCE(encoding);
}

TextCodecWinCE::TextCodecWinCE(const TextEncoding& encoding)
    : m_encoding(encoding)
{
}

TextCodecWinCE::~TextCodecWinCE()
{
}

void TextCodecWinCE::registerBaseEncodingNames(EncodingNameRegistrar registrar)
{
    registrar("UTF-8", "UTF-8");
}

void TextCodecWinCE::registerBaseCodecs(TextCodecRegistrar registrar)
{
    registrar("UTF-8", newTextCodecWinCE, 0);
}

void TextCodecWinCE::registerExtendedEncodingNames(EncodingNameRegistrar registrar)
{
    languageManager();
    for (CharsetSet::iterator i = supportedCharsets().begin(); i != supportedCharsets().end(); ++i) {
        HashMap<String, CharsetInfo>::iterator j = knownCharsets().find(*i);
        if (j != knownCharsets().end()) {
            registrar(j->second.m_name.data(), j->second.m_name.data());
            for (Vector<CString>::const_iterator alias = j->second.m_aliases.begin(); alias != j->second.m_aliases.end(); ++alias)
                registrar(alias->data(), j->second.m_name.data());
        }
    }
}

void TextCodecWinCE::registerExtendedCodecs(TextCodecRegistrar registrar)
{
    languageManager();
    for (CharsetSet::iterator i = supportedCharsets().begin(); i != supportedCharsets().end(); ++i) {
        HashMap<String, CharsetInfo>::iterator j = knownCharsets().find(*i);
        if (j != knownCharsets().end())
            registrar(j->second.m_name.data(), newTextCodecWinCE, 0);
    }
}

static DWORD getCodePageFlags(UINT codePage)
{
    if (codePage == CP_UTF8)
        return MB_ERR_INVALID_CHARS;

    if (codePage == 42) // Symbol
        return 0;

    // Microsoft says the flag must be 0 for the following code pages
    if (codePage > 50000) {
        if ((codePage >= 50220 && codePage <= 50222)
            || codePage == 50225
            || codePage == 50227
            || codePage == 50229
            || codePage == 52936
            || codePage == 54936
            || (codePage >= 57002 && codePage <= 57001)
            || codePage == 65000 // UTF-7
            )
            return 0;
    }

    return MB_PRECOMPOSED | MB_ERR_INVALID_CHARS;
}

static inline const char* findFirstNonAsciiCharacter(const char* bytes, size_t length)
{
    for (const char* bytesEnd = bytes + length; bytes < bytesEnd; ++bytes) {
        if (*bytes & 0x80)
            break;
    }
    return bytes;
}

static void decode(Vector<UChar, 8192>& result, const char* encodingName, const char* bytes, size_t length, size_t* left, bool canBeFirstTime, bool& sawInvalidChar)
{
    *left = length;
    if (!bytes || !length)
        return;

    UINT codePage;

    HashMap<String, CharsetInfo>::iterator i = knownCharsets().find(encodingName);
    if (i == knownCharsets().end()) {
        if (!strcmp(encodingName, "UTF-8"))
            codePage = CP_UTF8;
        else
            codePage = CP_ACP;
    }

    DWORD flags = getCodePageFlags(codePage);

    if (codePage == CP_UTF8) {
        if (canBeFirstTime) {
            // Handle BOM.
            if (length > 3) {
                if (bytes[0] == (char)0xEF && bytes[1] == (char)0xBB && bytes[2] == (char)0xBF) {
                    // BOM found!
                    length -= 3;
                    bytes += 3;
                    *left = length;
                }
            } else if (bytes[0] == 0xEF && (length < 2 || bytes[1] == (char)0xBB) && (length < 3 || bytes[2] == (char)0xBF)) {
                if (length == 3)
                    *left = 0;
                return;
            }
        }

        // Process ASCII characters at beginning.
        const char* firstNonAsciiChar = findFirstNonAsciiCharacter(bytes, length);
        int numAsciiCharacters = firstNonAsciiChar - bytes;
        if (numAsciiCharacters) {
            result.append(bytes, numAsciiCharacters);
            length -= numAsciiCharacters;
            if (!length) {
                *left = 0;
                return;
            }
            bytes = firstNonAsciiChar;
        }

        int oldSize = result.size();
        result.resize(oldSize + length);
        UChar* resultStart = result.data() + oldSize;
        const char* sourceStart = bytes;
        const char* const sourceEnd = bytes + length;
        for (;;) {
            using namespace WTF::Unicode;
            ConversionResult convRes = convertUTF8ToUTF16(&sourceStart
                , sourceEnd
                , &resultStart
                , result.data() + result.size()
                , true);

            // FIXME: is it possible?
            if (convRes == targetExhausted && sourceStart < sourceEnd) {
                oldSize = result.size();
                result.resize(oldSize + 256);
                resultStart = result.data() + oldSize;
                continue;
            }

            if (convRes != conversionOK)
                sawInvalidChar = true;

            break;
        }

        *left = sourceEnd - sourceStart;
        result.resize(resultStart - result.data());
    } else {
        int testLength = length;
        int untestedLength = length;
        for (;;) {
            int resultLength = MultiByteToWideChar(codePage, flags, bytes, testLength, 0, 0);

            if (resultLength > 0) {
                int oldSize = result.size();
                result.resize(oldSize + resultLength);

                MultiByteToWideChar(codePage, flags, bytes, testLength, result.data() + oldSize, resultLength);

                if (testLength == untestedLength) {
                    *left = length - testLength;
                    break;
                }
                untestedLength -= testLength;
                length -= testLength;
                bytes += testLength;
            } else {
                untestedLength = testLength - 1;
                if (!untestedLength) {
                    *left = length;
                    break;
                }
            }
            testLength = (untestedLength + 1) / 2;
        }
    }
}

String TextCodecWinCE::decode(const char* bytes, size_t length, bool flush, bool stopOnError, bool& sawError)
{
    if (!m_decodeBuffer.isEmpty()) {
        m_decodeBuffer.append(bytes, length);
        bytes = m_decodeBuffer.data();
        length = m_decodeBuffer.size();
    }

    size_t left;
    Vector<UChar, 8192> result;
    for (;;) {
        bool sawInvalidChar = false;
        WebCore::decode(result, m_encoding.name(), bytes, length, &left, m_decodeBuffer.isEmpty(), sawInvalidChar);
        if (!left)
            break;

        if (!sawInvalidChar && !flush && left < 16)
            break;

        result.append(L'?');
        sawError = true;
        if (stopOnError)
            return String(result.data(), result.size());


        if (left == 1)
            break;

        bytes += length - left + 1;
        length = left - 1;
    }
    if (left && !flush) {
        if (m_decodeBuffer.isEmpty())
            m_decodeBuffer.append(bytes + length - left, left);
        else {
            memmove(m_decodeBuffer.data(), bytes + length - left, left);
            m_decodeBuffer.resize(left);
        }
    } else
        m_decodeBuffer.clear();
    return String(result.data(), result.size());
}

CString TextCodecWinCE::encode(const UChar* characters, size_t length, UnencodableHandling)
{
    if (!characters || !length)
        return CString();

    UINT codePage = getCodePage(m_encoding.name());
    DWORD flags = codePage == CP_UTF8 ? 0 : WC_COMPOSITECHECK;

    int resultLength = WideCharToMultiByte(codePage, flags, characters, length, 0, 0, 0, 0);

    // FIXME: We need to implement UnencodableHandling: QuestionMarksForUnencodables, EntitiesForUnencodables, and URLEncodedEntitiesForUnencodables.

    if (resultLength <= 0)
        return "?";

    Vector<char> result(resultLength);

    WideCharToMultiByte(codePage, flags, characters, length, result.data(), resultLength, 0, 0);

    return CString(result.data(), result.size());
}

void TextCodecWinCE::enumerateSupportedEncodings(EncodingReceiver& receiver)
{
    languageManager();
    for (CharsetSet::iterator i = supportedCharsets().begin(); i != supportedCharsets().end(); ++i) {
        HashMap<String, CharsetInfo>::iterator j = knownCharsets().find(*i);
        if (j != knownCharsets().end() && !receiver.receive(j->second.m_name.data(), j->second.m_friendlyName.charactersWithNullTermination(), j->second.m_codePage))
            break;
    }
}

} // namespace WebCore
