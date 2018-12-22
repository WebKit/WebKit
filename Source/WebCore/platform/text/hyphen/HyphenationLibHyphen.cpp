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

#include "FileSystem.h"
#include <hyphen.h>
#include <limits>
#include <stdlib.h>
#include <wtf/HashMap.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/glib/GLibUtilities.h>
#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/AtomicStringHash.h>
#include <wtf/text/CString.h>
#include <wtf/text/StringView.h>

namespace WebCore {

static const char* const gDictionaryDirectories[] = {
    "/usr/share/hyphen",
    "/usr/local/share/hyphen",
};

static String extractLocaleFromDictionaryFilePath(const String& filePath)
{
    // Dictionary files always have the form "hyph_<locale name>.dic"
    // so we strip everything except the locale.
    String fileName = FileSystem::pathGetFileName(filePath);
    static const int prefixLength = 5;
    static const int suffixLength = 4;
    return fileName.substring(prefixLength, fileName.length() - prefixLength - suffixLength);
}

static void scanDirectoryForDictionaries(const char* directoryPath, HashMap<AtomicString, Vector<String>>& availableLocales)
{
    for (auto& filePath : FileSystem::listDirectory(directoryPath, "hyph_*.dic")) {
        String locale = extractLocaleFromDictionaryFilePath(filePath).convertToASCIILowercase();

        char normalizedPath[PATH_MAX];
        if (!realpath(FileSystem::fileSystemRepresentation(filePath).data(), normalizedPath))
            continue;

        filePath = FileSystem::stringFromFileSystemRepresentation(normalizedPath);
        availableLocales.add(locale, Vector<String>()).iterator->value.append(filePath);

        String localeReplacingUnderscores = String(locale);
        localeReplacingUnderscores.replace('_', '-');
        if (locale != localeReplacingUnderscores)
            availableLocales.add(localeReplacingUnderscores, Vector<String>()).iterator->value.append(filePath);

        size_t dividerPosition = localeReplacingUnderscores.find('-');
        if (dividerPosition != notFound) {
            localeReplacingUnderscores.truncate(dividerPosition);
            availableLocales.add(localeReplacingUnderscores, Vector<String>()).iterator->value.append(filePath);
        }
    }
}

#if ENABLE(DEVELOPER_MODE)
static CString topLevelPath()
{
    if (const char* topLevelDirectory = g_getenv("WEBKIT_TOP_LEVEL"))
        return topLevelDirectory;

    // If the environment variable wasn't provided then assume we were built into
    // WebKitBuild/Debug or WebKitBuild/Release. Obviously this will fail if the build
    // directory is non-standard, but we can't do much more about this.
    GUniquePtr<char> parentPath(g_path_get_dirname(getCurrentExecutablePath().data()));
    GUniquePtr<char> layoutTestsPath(g_build_filename(parentPath.get(), "..", "..", "..", nullptr));
    GUniquePtr<char> absoluteTopLevelPath(realpath(layoutTestsPath.get(), 0));
    return absoluteTopLevelPath.get();
}

static CString webkitBuildDirectory()
{
    const char* webkitOutputDir = g_getenv("WEBKIT_OUTPUTDIR");
    if (webkitOutputDir)
        return webkitOutputDir;

    GUniquePtr<char> outputDir(g_build_filename(topLevelPath().data(), "WebKitBuild", nullptr));
    return outputDir.get();
}

static void scanTestDictionariesDirectoryIfNecessary(HashMap<AtomicString, Vector<String>>& availableLocales)
{
    // It's unfortunate that we need to look for the dictionaries this way, but
    // libhyphen doesn't have the concept of installed dictionaries. Instead,
    // we have this special case for WebKit tests.
#if PLATFORM(GTK)
    CString buildDirectory = webkitBuildDirectory();
    GUniquePtr<char> dictionariesPath(g_build_filename(buildDirectory.data(), "DependenciesGTK", "Root", "webkitgtk-test-dicts", nullptr));
    if (g_file_test(dictionariesPath.get(), static_cast<GFileTest>(G_FILE_TEST_IS_DIR))) {
        scanDirectoryForDictionaries(dictionariesPath.get(), availableLocales);
        return;
    }

    // Try alternative dictionaries path for people not using JHBuild.
    dictionariesPath.reset(g_build_filename(buildDirectory.data(), "webkitgtk-test-dicts", nullptr));
    scanDirectoryForDictionaries(dictionariesPath.get(), availableLocales);
#elif defined(TEST_HYPHENATAION_PATH)
    scanDirectoryForDictionaries(TEST_HYPHENATAION_PATH, availableLocales);
#else
    UNUSED_PARAM(availableLocales);
#endif
}
#endif

static HashMap<AtomicString, Vector<String>>& availableLocales()
{
    static bool scannedLocales = false;
    static HashMap<AtomicString, Vector<String>> availableLocales;

    if (!scannedLocales) {
        for (size_t i = 0; i < WTF_ARRAY_LENGTH(gDictionaryDirectories); i++)
            scanDirectoryForDictionaries(gDictionaryDirectories[i], availableLocales);

#if ENABLE(DEVELOPER_MODE)
        scanTestDictionariesDirectoryIfNecessary(availableLocales);
#endif

        scannedLocales = true;
    }

    return availableLocales;
}

bool canHyphenate(const AtomicString& localeIdentifier)
{
    if (localeIdentifier.isNull())
        return false;
    if (availableLocales().contains(localeIdentifier))
        return true;
    return availableLocales().contains(AtomicString(localeIdentifier.string().convertToASCIILowercase()));
}

class HyphenationDictionary : public RefCounted<HyphenationDictionary> {
    WTF_MAKE_NONCOPYABLE(HyphenationDictionary);
    WTF_MAKE_FAST_ALLOCATED;
public:
    typedef std::unique_ptr<HyphenDict, void(*)(HyphenDict*)> HyphenDictUniquePtr;

    virtual ~HyphenationDictionary() = default;

    static Ref<HyphenationDictionary> createNull()
    {
        return adoptRef(*new HyphenationDictionary());
    }

    static Ref<HyphenationDictionary> create(const CString& dictPath)
    {
        return adoptRef(*new HyphenationDictionary(dictPath));
    }

    HyphenDict* libhyphenDictionary() const
    {
        return m_libhyphenDictionary.get();
    }

private:
    HyphenationDictionary(const CString& dictPath)
        : m_libhyphenDictionary(HyphenDictUniquePtr(hnj_hyphen_load(dictPath.data()), hnj_hyphen_free))
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
class TinyLRUCachePolicy<AtomicString, RefPtr<WebCore::HyphenationDictionary>>
{
public:
    static TinyLRUCache<AtomicString, RefPtr<WebCore::HyphenationDictionary>, 32>& cache()
    {
        static NeverDestroyed<TinyLRUCache<AtomicString, RefPtr<WebCore::HyphenationDictionary>, 32>> cache;
        return cache;
    }

    static bool isKeyNull(const AtomicString& localeIdentifier)
    {
        return localeIdentifier.isNull();
    }

    static RefPtr<WebCore::HyphenationDictionary> createValueForNullKey()
    {
        return WebCore::HyphenationDictionary::createNull();
    }

    static RefPtr<WebCore::HyphenationDictionary> createValueForKey(const AtomicString& dictionaryPath)
    {
        return WebCore::HyphenationDictionary::create(WebCore::FileSystem::fileSystemRepresentation(dictionaryPath.string()));
    }
};

} // namespace WTF

namespace WebCore {

static void countLeadingSpaces(const CString& utf8String, int32_t& pointerOffset, int32_t& characterOffset)
{
    pointerOffset = 0;
    characterOffset = 0;
    const char* stringData = utf8String.data();
    UChar32 character = 0;
    while (static_cast<unsigned>(pointerOffset) < utf8String.length()) {
        int32_t nextPointerOffset = pointerOffset;
        U8_NEXT(stringData, nextPointerOffset, static_cast<int32_t>(utf8String.length()), character);

        if (character < 0 || !u_isUWhiteSpace(character))
            return;

        pointerOffset = nextPointerOffset;
        characterOffset++;
    }
}

size_t lastHyphenLocation(StringView string, size_t beforeIndex, const AtomicString& localeIdentifier)
{
    // libhyphen accepts strings in UTF-8 format, but WebCore can only provide StringView
    // which stores either UTF-16 or Latin1 data. This is unfortunate for performance
    // reasons and we should consider switching to a more flexible hyphenation library
    // if it is available.
    CString utf8StringCopy = string.toStringWithoutCopying().utf8();

    // WebCore often passes strings like " wordtohyphenate" to the platform layer. Since
    // libhyphen isn't advanced enough to deal with leading spaces (presumably CoreFoundation
    // can), we should find the appropriate indexes into the string to skip them.
    int32_t leadingSpaceBytes;
    int32_t leadingSpaceCharacters;
    countLeadingSpaces(utf8StringCopy, leadingSpaceBytes, leadingSpaceCharacters);

    // The libhyphen documentation specifies that this array should be 5 bytes longer than
    // the byte length of the input string.
    Vector<char> hyphenArray(utf8StringCopy.length() - leadingSpaceBytes + 5);
    char* hyphenArrayData = hyphenArray.data();

    String lowercaseLocaleIdentifier = AtomicString(localeIdentifier.string().convertToASCIILowercase());

    // Web content may specify strings for locales which do not exist or that we do not have.
    if (!availableLocales().contains(lowercaseLocaleIdentifier))
        return 0;

    for (const auto& dictionaryPath : availableLocales().get(lowercaseLocaleIdentifier)) {
        RefPtr<HyphenationDictionary> dictionary = WTF::TinyLRUCachePolicy<AtomicString, RefPtr<HyphenationDictionary>>::cache().get(AtomicString(dictionaryPath));

        char** replacements = nullptr;
        int* positions = nullptr;
        int* removedCharacterCounts = nullptr;
        hnj_hyphen_hyphenate2(dictionary->libhyphenDictionary(),
            utf8StringCopy.data() + leadingSpaceBytes,
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

#endif // USE(LIBHYPHEN)
