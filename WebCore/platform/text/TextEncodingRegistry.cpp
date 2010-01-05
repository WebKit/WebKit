/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
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

#include "PlatformString.h"
#include "TextCodecLatin1.h"
#include "TextCodecUserDefined.h"
#include "TextCodecUTF16.h"
#include <wtf/ASCIICType.h>
#include <wtf/Assertions.h>
#include <wtf/HashFunctions.h>
#include <wtf/HashMap.h>
#include <wtf/StdLibExtras.h>
#include <wtf/StringExtras.h>
#include <wtf/Threading.h>

#if USE(ICU_UNICODE)
#include "TextCodecICU.h"
#endif
#if PLATFORM(MAC)
#include "TextCodecMac.h"
#endif
#if PLATFORM(QT)
#include "qt/TextCodecQt.h"
#endif
#if USE(GLIB_UNICODE)
#include "gtk/TextCodecGtk.h"
#endif
#if OS(WINCE) && !PLATFORM(QT)
#include "TextCodecWince.h"
#endif

using namespace WTF;

namespace WebCore {

const size_t maxEncodingNameLength = 63;

// Hash for all-ASCII strings that does case folding and skips any characters
// that are not alphanumeric. If passed any non-ASCII characters, depends on
// the behavior of isalnum -- if that returns false as it does on OS X, then
// it will properly skip those characters too.
struct TextEncodingNameHash {

    static bool equal(const char* s1, const char* s2)
    {
        char c1;
        char c2;
        do {
            do
                c1 = *s1++;
            while (c1 && !isASCIIAlphanumeric(c1));
            do
                c2 = *s2++;
            while (c2 && !isASCIIAlphanumeric(c2));
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
            char c;
            do {
                c = *s++;
                if (!c) {
                    h += (h << 3);
                    h ^= (h >> 11);
                    h += (h << 15);
                    return h;
                }
            } while (!isASCIIAlphanumeric(c));
            h += toASCIILower(c);
            h += (h << 10); 
            h ^= (h >> 6); 
        }
    }

    static const bool safeToCompareToEmptyOrDeleted = false;
};

struct TextCodecFactory {
    NewTextCodecFunction function;
    const void* additionalData;
    TextCodecFactory(NewTextCodecFunction f = 0, const void* d = 0) : function(f), additionalData(d) { }
};

typedef HashMap<const char*, const char*, TextEncodingNameHash> TextEncodingNameMap;
typedef HashMap<const char*, TextCodecFactory> TextCodecMap;

static Mutex& encodingRegistryMutex()
{
    // We don't have to use AtomicallyInitializedStatic here because
    // this function is called on the main thread for any page before
    // it is used in worker threads.
    DEFINE_STATIC_LOCAL(Mutex, mutex, ());
    return mutex;
}

static TextEncodingNameMap* textEncodingNameMap;
static TextCodecMap* textCodecMap;
static bool didExtendTextCodecMaps;

static const char* const textEncodingNameBlacklist[] = {
    "UTF-7"
};

#if ERROR_DISABLED

static inline void checkExistingName(const char*, const char*) { }

#else

static void checkExistingName(const char* alias, const char* atomicName)
{
    const char* oldAtomicName = textEncodingNameMap->get(alias);
    if (!oldAtomicName)
        return;
    if (oldAtomicName == atomicName)
        return;
    // Keep the warning silent about one case where we know this will happen.
    if (strcmp(alias, "ISO-8859-8-I") == 0
            && strcmp(oldAtomicName, "ISO-8859-8-I") == 0
            && strcasecmp(atomicName, "iso-8859-8") == 0)
        return;
    LOG_ERROR("alias %s maps to %s already, but someone is trying to make it map to %s",
        alias, oldAtomicName, atomicName);
}

#endif

static void addToTextEncodingNameMap(const char* alias, const char* name)
{
    ASSERT(strlen(alias) <= maxEncodingNameLength);
    const char* atomicName = textEncodingNameMap->get(name);
    ASSERT(strcmp(alias, name) == 0 || atomicName);
    if (!atomicName)
        atomicName = name;
    checkExistingName(alias, atomicName);
    textEncodingNameMap->add(alias, atomicName);
}

static void addToTextCodecMap(const char* name, NewTextCodecFunction function, const void* additionalData)
{
    const char* atomicName = textEncodingNameMap->get(name);
    ASSERT(atomicName);
    textCodecMap->add(atomicName, TextCodecFactory(function, additionalData));
}

static void pruneBlacklistedCodecs()
{
    size_t blacklistedCodecListLength = sizeof(textEncodingNameBlacklist) / sizeof(textEncodingNameBlacklist[0]);
    for (size_t i = 0; i < blacklistedCodecListLength; ++i) {
        const char* atomicName = textEncodingNameMap->get(textEncodingNameBlacklist[i]);
        if (!atomicName)
            continue;

        Vector<const char*> names;
        TextEncodingNameMap::const_iterator it = textEncodingNameMap->begin();
        TextEncodingNameMap::const_iterator end = textEncodingNameMap->end();
        for (; it != end; ++it) {
            if (it->second == atomicName)
                names.append(it->first);
        }

        size_t length = names.size();
        for (size_t j = 0; j < length; ++j)
            textEncodingNameMap->remove(names[j]);

        textCodecMap->remove(atomicName);
    }
}

static void buildBaseTextCodecMaps()
{
    ASSERT(isMainThread());
    ASSERT(!textCodecMap);
    ASSERT(!textEncodingNameMap);

    textCodecMap = new TextCodecMap;
    textEncodingNameMap = new TextEncodingNameMap;

    TextCodecLatin1::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecLatin1::registerCodecs(addToTextCodecMap);

    TextCodecUTF16::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecUTF16::registerCodecs(addToTextCodecMap);

    TextCodecUserDefined::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecUserDefined::registerCodecs(addToTextCodecMap);

#if USE(ICU_UNICODE)
    TextCodecICU::registerBaseEncodingNames(addToTextEncodingNameMap);
    TextCodecICU::registerBaseCodecs(addToTextCodecMap);
#endif

#if USE(GLIB_UNICODE)
    TextCodecGtk::registerBaseEncodingNames(addToTextEncodingNameMap);
    TextCodecGtk::registerBaseCodecs(addToTextCodecMap);
#endif

#if OS(WINCE) && !PLATFORM(QT)
    TextCodecWince::registerBaseEncodingNames(addToTextEncodingNameMap);
    TextCodecWince::registerBaseCodecs(addToTextCodecMap);
#endif
}

static void extendTextCodecMaps()
{
#if USE(ICU_UNICODE)
    TextCodecICU::registerExtendedEncodingNames(addToTextEncodingNameMap);
    TextCodecICU::registerExtendedCodecs(addToTextCodecMap);
#endif

#if USE(QT4_UNICODE)
    TextCodecQt::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecQt::registerCodecs(addToTextCodecMap);
#endif

#if PLATFORM(MAC)
    TextCodecMac::registerEncodingNames(addToTextEncodingNameMap);
    TextCodecMac::registerCodecs(addToTextCodecMap);
#endif

#if USE(GLIB_UNICODE)
    TextCodecGtk::registerExtendedEncodingNames(addToTextEncodingNameMap);
    TextCodecGtk::registerExtendedCodecs(addToTextCodecMap);
#endif

#if OS(WINCE) && !PLATFORM(QT)
    TextCodecWince::registerExtendedEncodingNames(addToTextEncodingNameMap);
    TextCodecWince::registerExtendedCodecs(addToTextCodecMap);
#endif

    pruneBlacklistedCodecs();
}

PassOwnPtr<TextCodec> newTextCodec(const TextEncoding& encoding)
{
    MutexLocker lock(encodingRegistryMutex());

    ASSERT(textCodecMap);
    TextCodecFactory factory = textCodecMap->get(encoding.name());
    ASSERT(factory.function);
    return factory.function(encoding, factory.additionalData);
}

const char* atomicCanonicalTextEncodingName(const char* name)
{
    if (!name || !name[0])
        return 0;
    if (!textEncodingNameMap)
        buildBaseTextCodecMaps();

    MutexLocker lock(encodingRegistryMutex());

    if (const char* atomicName = textEncodingNameMap->get(name))
        return atomicName;
    if (didExtendTextCodecMaps)
        return 0;
    extendTextCodecMaps();
    didExtendTextCodecMaps = true;
    return textEncodingNameMap->get(name);
}

const char* atomicCanonicalTextEncodingName(const UChar* characters, size_t length)
{
    char buffer[maxEncodingNameLength + 1];
    size_t j = 0;
    for (size_t i = 0; i < length; ++i) {
        UChar c = characters[i];
        if (isASCIIAlphanumeric(c)) {
            if (j == maxEncodingNameLength)
                return 0;
            buffer[j++] = c;
        }
    }
    buffer[j] = 0;
    return atomicCanonicalTextEncodingName(buffer);
}

bool noExtendedTextEncodingNameUsed()
{
    // If the calling thread did not use extended encoding names, it is fine for it to use a stale false value.
    return !didExtendTextCodecMaps;
}

} // namespace WebCore
