/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2015 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Hyphenation.h"

#if USE(LIBHYPHEN)

#include <hyphen.h>
#include <limits>
#include <stdlib.h>
#include <wtf/FileSystem.h>
#include <wtf/HashMap.h>
#include <wtf/Language.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/text/AtomStringHash.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringView.h>

#if PLATFORM(GTK)
#include <wtf/glib/GUniquePtr.h>
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // GLib port

namespace WebCore {

static const char* const gDictionaryDirectories[] = {
    "/usr/share/hyphen",
    "/usr/local/share/hyphen",
};

static RefPtr<AtomStringImpl> extractLocaleFromDictionaryFileName(StringImpl* fileName)
{
    if (!startsWith(fileName, "hyph_"_s) || !endsWith(fileName, ".dic"_s))
        return nullAtom();

    // Dictionary files always have the form "hyph_<locale name>.dic"
    // so we strip everything except the locale.
    constexpr int prefixLength = 5;
    constexpr int suffixLength = 4;
    return StringView(fileName).substring(prefixLength, length(fileName) - prefixLength - suffixLength).convertToASCIILowercaseAtom();
}

static void scanDirectoryForDictionaries(const char* directoryPath, HashMap<RefPtr<AtomStringImpl>, Vector<RefPtr<StringImpl>>>& availableLocales)
{
    auto directoryPathString = StringImpl::fromUTF8(directoryPath);
    for (auto& fileName : FileSystem::listDirectory(directoryPathString)) {
        auto locale = extractLocaleFromDictionaryFileName(fileName);
        if (isEmpty(locale))
            continue;

        auto fullPath = FileSystem::pathByAppendingComponent(directoryPathString, fileName);
        auto filePath = FileSystem::realPath(fullPath);

        availableLocales.add(locale, Vector<RefPtr<StringImpl>>()).iterator->value.append(filePath);

        RefPtr<StringImpl> localeReplacingUnderscores = makeStringByReplacingAll(locale, '_', '-');
        if (!equal(locale, localeReplacingUnderscores))
            availableLocales.add(AtomStringImpl::add(localeReplacingUnderscores), Vector<RefPtr<StringImpl>>()).iterator->value.append(filePath);

        size_t dividerPosition = find(localeReplacingUnderscores, '-');
        if (dividerPosition != notFound)
            availableLocales.add(StringView(localeReplacingUnderscores).left(dividerPosition).toAtomString(), Vector<RefPtr<StringImpl>>()).iterator->value.append(filePath);
    }
}


static HashMap<RefPtr<AtomStringImpl>, Vector<RefPtr<StringImpl>>>& availableLocales()
{
    static bool scannedLocales = false;
    static HashMap<RefPtr<AtomStringImpl>, Vector<RefPtr<StringImpl>>> availableLocales;

    if (!scannedLocales) {
        for (size_t i = 0; i < std::size(gDictionaryDirectories); i++)
            scanDirectoryForDictionaries(gDictionaryDirectories[i], availableLocales);

        scannedLocales = true;
    }

    return availableLocales;
}

bool canHyphenate(AtomStringImpl* localeIdentifier)
{
    if (!localeIdentifier)
        return false;
    return availableLocales().contains(convertToASCIILowercase(localeIdentifier));
}

class HyphenationDictionary : public RefCounted<HyphenationDictionary> {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(HyphenationDictionary);
    WTF_MAKE_NONCOPYABLE(HyphenationDictionary);
public:
    typedef std::unique_ptr<HyphenDict, void(*)(HyphenDict*)> HyphenDictUniquePtr;

    virtual ~HyphenationDictionary() = default;

    static Ref<HyphenationDictionary> createNull()
    {
        return adoptRef(*new HyphenationDictionary());
    }

    static Ref<HyphenationDictionary> create(const UTF8CString& dictPath)
    {
        return adoptRef(*new HyphenationDictionary(dictPath));
    }

    HyphenDict* libhyphenDictionary() const
    {
        return m_libhyphenDictionary.get();
    }

private:
    HyphenationDictionary(const UTF8CString& dictPath)
        : m_libhyphenDictionary(HyphenDictUniquePtr(hnj_hyphen_load(dictPath.legacyCStringPointer()), hnj_hyphen_free))
    {
    }

    HyphenationDictionary()
        : m_libhyphenDictionary(HyphenDictUniquePtr(nullptr, hnj_hyphen_free))
    {
    }

    HyphenDictUniquePtr m_libhyphenDictionary;
};

} // namespace WebCore

namespace WTF {

template<>
class TinyLRUCachePolicy<RefPtr<AtomStringImpl>, RefPtr<WebCore::HyphenationDictionary>>
{
public:
    static TinyLRUCache<RefPtr<AtomStringImpl>, RefPtr<WebCore::HyphenationDictionary>, 32>& cache()
    {
        static NeverDestroyed<TinyLRUCache<RefPtr<AtomStringImpl>, RefPtr<WebCore::HyphenationDictionary>, 32>> cache;
        return cache;
    }

    static bool isKeyNull(AtomStringImpl* localeIdentifier)
    {
        return !localeIdentifier;
    }

    static RefPtr<WebCore::HyphenationDictionary> createValueForNullKey()
    {
        return WebCore::HyphenationDictionary::createNull();
    }

    static RefPtr<WebCore::HyphenationDictionary> createValueForKey(AtomStringImpl* dictionaryPath)
    {
        return WebCore::HyphenationDictionary::create(FileSystem::fileSystemRepresentation(dictionaryPath));
    }

    static RefPtr<AtomStringImpl> createKeyForStorage(AtomStringImpl* key) { return key; }
};

} // namespace WTF

namespace WebCore {

static void countLeadingSpaces(const UTF8CString& utf8String, int32_t& pointerOffset, int32_t& characterOffset)
{
    pointerOffset = 0;
    characterOffset = 0;
    const char* stringData = utf8String.legacyCStringPointer();
    char32_t character = 0;
    while (static_cast<unsigned>(pointerOffset) < utf8String.length()) {
        int32_t nextPointerOffset = pointerOffset;
        U8_NEXT(stringData, nextPointerOffset, static_cast<int32_t>(utf8String.length()), character);
        if (character == static_cast<char32_t>(U_SENTINEL) || !u_isUWhiteSpace(character))
            return;

        pointerOffset = nextPointerOffset;
        characterOffset++;
    }
}

size_t lastHyphenLocation(StringView string, size_t beforeIndex, AtomStringImpl* localeIdentifier)
{
    // libhyphen accepts strings in UTF-8 format, but WebCore can only provide StringView
    // which stores either UTF-16 or Latin1 data. This is unfortunate for performance
    // reasons and we should consider switching to a more flexible hyphenation library
    // if it is available.
    auto utf8StringCopy = string.utf8();

    // WebCore often passes strings like " wordtohyphenate" to the platform layer. Since
    // libhyphen isn't advanced enough to deal with leading spaces (presumably CoreFoundation
    // can), we should find the appropriate indexes into the string to skip them.
    int32_t leadingSpaceBytes;
    int32_t leadingSpaceCharacters;
    countLeadingSpaces(utf8StringCopy, leadingSpaceBytes, leadingSpaceCharacters);

    // The libhyphen documentation specifies that this array should be 5 bytes longer than
    // the byte length of the input string.
    Vector<char> hyphenArray(utf8StringCopy.length() - leadingSpaceBytes + 5);
    char* hyphenArrayData = hyphenArray.mutableSpan().data();

    RefPtr<AtomStringImpl> lowercaseLocaleIdentifier = convertToASCIILowercase(localeIdentifier);

    // Web content may specify strings for locales which do not exist or that we do not have.
    if (!availableLocales().contains(lowercaseLocaleIdentifier))
        return 0;

    for (const auto& dictionaryPath : availableLocales().get(lowercaseLocaleIdentifier)) {
        RefPtr<HyphenationDictionary> dictionary = TinyLRUCachePolicy<RefPtr<AtomStringImpl>, RefPtr<HyphenationDictionary>>::cache().get(AtomStringImpl::add(dictionaryPath));

        if (!dictionary->libhyphenDictionary())
            continue;

        char** replacements = nullptr;
        int* positions = nullptr;
        int* removedCharacterCounts = nullptr;
        hnj_hyphen_hyphenate2(dictionary->libhyphenDictionary(),
            utf8StringCopy.legacyCStringPointer() + leadingSpaceBytes,
            utf8StringCopy.length() - leadingSpaceBytes,
            hyphenArrayData,
            nullptr, /* output parameter for hyphenated word */
            &replacements,
            &positions,
            &removedCharacterCounts);

        if (replacements) {
            for (unsigned i = 0; i < utf8StringCopy.length() - leadingSpaceBytes - 1; i++)
                free(replacements[i]);
            free(replacements);
        }

        free(positions);
        free(removedCharacterCounts);

        for (int i = beforeIndex - leadingSpaceCharacters - 2; i >= 0; i--) {
            // libhyphen will put an odd number in hyphenArrayData at all
            // hyphenation points. A number & 1 will be true for odd numbers.
            if (hyphenArrayData[i] & 1)
                return i + 1 + leadingSpaceCharacters;
        }
    }

    return 0;
}

} // namespace WebCore

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#endif // USE(LIBHYPHEN)
