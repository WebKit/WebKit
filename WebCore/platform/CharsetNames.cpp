/*
 * Copyright (C) 2003, 2006 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2005 Alexey Proskuryakov <ap@nypop.com>.
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
#include "CharsetNames.h"

#include "CharsetData.h"
#include <wtf/HashMap.h>
#include <unicode/ucnv.h>
#include <unicode/utypes.h>

using namespace WTF;
using namespace WebCore;

namespace WebCore {

struct TextEncodingIDHashTraits : GenericHashTraits<TextEncodingID> {
    static TraitType deletedValue() { return InvalidEncoding; }
};

}

namespace WTF {

template<> struct HashKeyStorageTraits<IntHash<TextEncodingID>, TextEncodingIDHashTraits> {
    typedef IntTypes<sizeof(TextEncodingID)>::SignedType IntType;
    typedef IntHash<IntType> Hash;
    typedef HashTraits<IntType> Traits;
};

}

namespace WebCore {

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

struct EncodingHash {
    static bool equal(const char *s1, const char *s2)
    {
        char c1;
        char c2;
        
        do {
            do {
                c1 = *s1++;
            } while (c1 && !isalnum(c1));
            do {
                c2 = *s2++;
            } while (c2 && !isalnum(c2));
            
            if (tolower(c1) != tolower(c2))
                return false;
        } while (c1 && c2);

        return !c1 && !c2;
    }

    // This hash algorithm comes from:
    // http://burtleburtle.net/bob/hash/hashfaq.html
    // http://burtleburtle.net/bob/hash/doobs.html
    static unsigned hash(const char* s)
    {
        unsigned h = PHI;
        
        for (int i = 0; i != 16; ++i) {
            char c;
            do {
                c = *s++;
            } while (c && !isalnum(c));
            if (!c) {
                break;
            }
            h += tolower(c);
            h += (h << 10); 
            h ^= (h >> 6); 
        }
        
        h += (h << 3);
        h ^= (h >> 11);
        h += (h << 15);
        
        return h;
    }
};

typedef HashMap<const char*, const CharsetEntry*, EncodingHash> NameMap;
typedef HashMap<TextEncodingID, const CharsetEntry*, IntHash<TextEncodingID>, TextEncodingIDHashTraits> EncodingMap;

static NameMap* nameMap;
static EncodingMap* encodingMap;

static void buildDictionaries()
{
    nameMap = new NameMap;
    encodingMap = new EncodingMap;

    for (int i = 0; CharsetTable[i].name; ++i) {
        nameMap->add(CharsetTable[i].name, &CharsetTable[i]);
        encodingMap->add(CharsetTable[i].encoding, &CharsetTable[i]);
    }
}

TextEncodingID textEncodingIDFromCharsetName(const char* name, TextEncodingFlags* flags)
{
    if (!nameMap)
        buildDictionaries();

    const CharsetEntry* entry = nameMap->get(name);
    if (!entry) {
        UErrorCode err = U_ZERO_ERROR;
        name = ucnv_getStandardName(name, "IANA", &err);
        
        if (!name || !(entry = nameMap->get(name))) {
            if (flags)
                *flags = NoEncodingFlags;
            return InvalidEncoding;
        }
    }

    if (flags)
        *flags = (TextEncodingFlags)entry->flags;
    return entry->encoding;
}

const char* charsetNameFromTextEncodingID(TextEncodingID encoding)
{
    if (!encodingMap)
        buildDictionaries();
    
    const CharsetEntry* entry = encodingMap->get(encoding);
    if (!entry)
        return 0;
    return entry->name;
}

} // namespace WebCore
