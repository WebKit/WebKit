/*
 * Copyright (C) 2006, 2007 Apple Inc. All rights reserved.
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
#include <wtf/HashMap.h>

#if USE(ICU_UNICODE)
#include "TextCodecICU.h"
#endif
#if PLATFORM(MAC)
#include "TextCodecMac.h"
#endif
#if PLATFORM(QT)
#include "qt/TextCodecQt.h"
#endif

using namespace WTF;

namespace WebCore {

const size_t maxEncodingNameLength = 63;

// Hash for all-ASCII strings that does case folding and skips any characters
// that are not alphanumeric. If passed any non-ASCII characters, depends on
// the behavior of isalnum -- if that returns false as it does on OS X, then
// it will properly skip those characters too.
struct TextEncodingNameHash {

    // Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
    // or anything like that.
    static const unsigned PHI = 0x9e3779b9U;

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
        unsigned h = PHI;
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

static TextEncodingNameMap* textEncodingNameMap;
static TextCodecMap* textCodecMap;
static bool didExtendTextCodecMaps;

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
            && strcmp(atomicName, "ISO_8859-8:1988") == 0)
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
    TextEncoding encoding(name);
    ASSERT(encoding.isValid());
    textCodecMap->add(encoding.name(), TextCodecFactory(function, additionalData));
}

static void buildBaseTextCodecMaps()
{
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
}

std::auto_ptr<TextCodec> newTextCodec(const TextEncoding& encoding)
{
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
    return !didExtendTextCodecMaps;
}

} // namespace WebCore
