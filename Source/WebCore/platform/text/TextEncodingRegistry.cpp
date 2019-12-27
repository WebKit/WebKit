/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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
#include "TextEncodingRegistry.h"

#include "TextCodecICU.h"
#include "TextCodecLatin1.h"
#include "TextCodecReplacement.h"
#include "TextCodecUTF16.h"
#include "TextCodecUTF8.h"
#include "TextCodecUserDefined.h"
#include "TextEncoding.h"
#include <mutex>
#include <wtf/ASCIICType.h>
#include <wtf/CheckedArithmetic.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>
#include <wtf/Lock.h>
#include <wtf/MainThread.h>
#include <wtf/StdLibExtras.h>
#include <wtf/text/CString.h>

namespace WebCore {

const size_t maxEncodingNameLength = 63;

// Hash for all-ASCII strings that does case folding.
struct TextEncodingNameHash {
    static bool equal(const char* s1, const char* s2)
    {
        char c1;
        char c2;
        do {
            c1 = *s1++;
            c2 = *s2++;
            if (toASCIILower(c1) != toASCIILower(c2))
                return false;
        } while (c1 && c2);
        return !c1 && !c2;
    }

    // This algorithm is the one-at-a-time hash from:
    // http://burtleburtle.net/bob/hash/hashfaq.html
    // http://burtleburtle.net/bob/hash/doobs.html
    static unsigned hash(const char* s)
    {
        unsigned h = WTF::stringHashingStartValue;
        for (;;) {
            char c = *s++;
            if (!c) {
                h += (h << 3);
                h ^= (h >> 11);
                h += (h << 15);
                return h;
            }
            h += toASCIILower(c);
            h += (h << 10); 
            h ^= (h >> 6); 
        }
    }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

using TextEncodingNameMap = HashMap<const char*, const char*, TextEncodingNameHash>;
using TextCodecMap = HashMap<const char*, NewTextCodecFunction>;

static Lock encodingRegistryMutex;

static TextEncodingNameMap* textEncodingNameMap;
static TextCodecMap* textCodecMap;
static bool didExtendTextCodecMaps;
static HashSet<const char*>* japaneseEncodings;
static HashSet<const char*>* nonBackslashEncodings;

static const char* const textEncodingNameBlacklist[] = { "UTF-7", "BOCU-1", "SCSU" };

static bool isUndesiredAlias(const char* alias)
{
    // Reject aliases with version numbers that are supported by some back-ends (such as "ISO_2022,locale=ja,version=0" in ICU).
    for (const char* p = alias; *p; ++p) {
        if (*p == ',')
            return true;
    }
    // 8859_1 is known to (at least) ICU, but other browsers don't support this name - and having it caused a compatibility
    // problem, see bug 43554.
    if (0 == strcmp(alias, "8859_1"))
        return true;
    return false;
}

static void addToTextEncodingNameMap(const char* alias, const char* name)
{
    ASSERT(strlen(alias) <= maxEncodingNameLength);
    if (isUndesiredAlias(alias))
        return;
    const char* atomName = textEncodingNameMap->get(name);
    ASSERT(!strcmp(alias, name) || atomName);
    if (!atomName)
        atomName = name;

    ASSERT_WITH_MESSAGE(!textEncodingNameMap->get(alias), "Duplicate text encoding name %s for %s (previously registered as %s)", alias, atomName, textEncodingNameMap->get(alias));

    textEncodingNameMap->add(alias, atomName);
}

static void addToTextCodecMap(const char* name, NewTextCodecFunction&& function)
{
    const char* atomName = textEncodingNameMap->get(name);
    ASSERT(atomName);
    textCodecMap->add(atomName, WTFMove(function));
}

static void pruneBlacklistedCodecs()
{
    for (auto& nameFromBlacklist : textEncodingNameBlacklist) {
        auto* atomName = textEncodingNameMap->get(nameFromBlacklist);
        if (!atomName)
            continue;

        Vector<const char*> names;
        for (auto& entry : *textEncodingNameMap) {
            if (entry.value == atomName)
                names.append(entry.key);
        }

        for (auto* name : names)
            textEncodingNameMap->remove(name);

        textCodecMap->remove(atomName);
    }
}

static void buildBaseTextCodecMaps(const std::lock_guard<Lock>&)
{
    ASSERT(!textCodecMap);
    ASSERT(!textEncodingNameMap);

    textCodecMap = new TextCodecMap;
    textEncodingNameMap = new TextEncodingNameMap;

    TextCodecLatin1::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecLatin1::registerCodecs(addToTextCodecMap);

    TextCodecUTF8::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecUTF8::registerCodecs(addToTextCodecMap);

    TextCodecUTF16::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecUTF16::registerCodecs(addToTextCodecMap);

    TextCodecUserDefined::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecUserDefined::registerCodecs(addToTextCodecMap);
}

static void addEncodingName(HashSet<const char*>* set, const char* name)
{
    // We must not use atomCanonicalTextEncodingName() because this function is called in it.
    const char* atomName = textEncodingNameMap->get(name);
    if (atomName)
        set->add(atomName);
}

static void buildQuirksSets()
{
    // FIXME: Having isJapaneseEncoding() and shouldShowBackslashAsCurrencySymbolIn()
    // and initializing the sets for them in TextEncodingRegistry.cpp look strange.

    ASSERT(!japaneseEncodings);
    ASSERT(!nonBackslashEncodings);

    japaneseEncodings = new HashSet<const char*>;
    addEncodingName(japaneseEncodings, "EUC-JP");
    addEncodingName(japaneseEncodings, "ISO-2022-JP");
    addEncodingName(japaneseEncodings, "ISO-2022-JP-1");
    addEncodingName(japaneseEncodings, "ISO-2022-JP-2");
    addEncodingName(japaneseEncodings, "ISO-2022-JP-3");
    addEncodingName(japaneseEncodings, "JIS_C6226-1978");
    addEncodingName(japaneseEncodings, "JIS_X0201");
    addEncodingName(japaneseEncodings, "JIS_X0208-1983");
    addEncodingName(japaneseEncodings, "JIS_X0208-1990");
    addEncodingName(japaneseEncodings, "JIS_X0212-1990");
    addEncodingName(japaneseEncodings, "Shift_JIS");
    addEncodingName(japaneseEncodings, "Shift_JIS_X0213-2000");
    addEncodingName(japaneseEncodings, "cp932");
    addEncodingName(japaneseEncodings, "x-mac-japanese");

    nonBackslashEncodings = new HashSet<const char*>;
    // The text encodings below treat backslash as a currency symbol for IE compatibility.
    // See http://blogs.msdn.com/michkap/archive/2005/09/17/469941.aspx for more information.
    addEncodingName(nonBackslashEncodings, "x-mac-japanese");
    addEncodingName(nonBackslashEncodings, "ISO-2022-JP");
    addEncodingName(nonBackslashEncodings, "EUC-JP");
    // Shift_JIS_X0213-2000 is not the same encoding as Shift_JIS on Mac. We need to register both of them.
    addEncodingName(nonBackslashEncodings, "Shift_JIS");
    addEncodingName(nonBackslashEncodings, "Shift_JIS_X0213-2000");
}

bool isJapaneseEncoding(const char* canonicalEncodingName)
{
    return canonicalEncodingName && japaneseEncodings && japaneseEncodings->contains(canonicalEncodingName);
}

bool shouldShowBackslashAsCurrencySymbolIn(const char* canonicalEncodingName)
{
    return canonicalEncodingName && nonBackslashEncodings && nonBackslashEncodings->contains(canonicalEncodingName);
}

static void extendTextCodecMaps()
{
    TextCodecReplacement::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecReplacement::registerCodecs(addToTextCodecMap);

    TextCodecICU::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecICU::registerCodecs(addToTextCodecMap);

    pruneBlacklistedCodecs();
    buildQuirksSets();
}

std::unique_ptr<TextCodec> newTextCodec(const TextEncoding& encoding)
{
    std::lock_guard<Lock> lock(encodingRegistryMutex);

    ASSERT(textCodecMap);
    auto result = textCodecMap->find(encoding.name());
    ASSERT(result != textCodecMap->end());
    return result->value();
}

const char* atomCanonicalTextEncodingName(const char* name)
{
    if (!name || !name[0])
        return nullptr;

    std::lock_guard<Lock> lock(encodingRegistryMutex);

    if (!textEncodingNameMap)
        buildBaseTextCodecMaps(lock);

    if (const char* atomName = textEncodingNameMap->get(name))
        return atomName;
    if (didExtendTextCodecMaps)
        return nullptr;

    extendTextCodecMaps();
    didExtendTextCodecMaps = true;
    return textEncodingNameMap->get(name);
}

template<typename CharacterType> static const char* atomCanonicalTextEncodingName(const CharacterType* characters, size_t length)
{
    char buffer[maxEncodingNameLength + 1];
    size_t j = 0;
    for (size_t i = 0; i < length; ++i) {
        if (j == maxEncodingNameLength)
            return nullptr;
        buffer[j++] = characters[i];
    }
    buffer[j] = 0;
    return atomCanonicalTextEncodingName(buffer);
}

const char* atomCanonicalTextEncodingName(const String& alias)
{
    if (alias.isEmpty() || !alias.isAllASCII())
        return nullptr;

    if (alias.is8Bit())
        return atomCanonicalTextEncodingName(alias.characters8(), alias.length());

    return atomCanonicalTextEncodingName(alias.characters16(), alias.length());
}

bool noExtendedTextEncodingNameUsed()
{
    // If the calling thread did not use extended encoding names, it is fine for it to use a stale false value.
    return !didExtendTextCodecMaps;
}

String defaultTextEncodingNameForSystemLanguage()
{
#if PLATFORM(COCOA)
    String systemEncodingName = CFStringConvertEncodingToIANACharSetName(webDefaultCFStringEncoding());

    // CFStringConvertEncodingToIANACharSetName() returns cp949 for kTextEncodingDOSKorean AKA "extended EUC-KR" AKA windows-949.
    // ICU uses this name for a different encoding, so we need to change the name to a value that actually gives us windows-949.
    // In addition, this value must match what is used in Safari, see <rdar://problem/5579292>.
    // On some OS versions, the result is CP949 (uppercase).
    if (equalLettersIgnoringASCIICase(systemEncodingName, "cp949"))
        systemEncodingName = "ks_c_5601-1987"_s;

    // CFStringConvertEncodingToIANACharSetName() returns cp874 for kTextEncodingDOSThai, AKA windows-874.
    // Since "cp874" alias is not standard (https://encoding.spec.whatwg.org/#names-and-labels), map to
    // "dos-874" instead.
    if (equalLettersIgnoringASCIICase(systemEncodingName, "cp874"))
        systemEncodingName = "dos-874"_s;

    return systemEncodingName;
#else
    return "ISO-8859-1"_s;
#endif
}

} // namespace WebCore
