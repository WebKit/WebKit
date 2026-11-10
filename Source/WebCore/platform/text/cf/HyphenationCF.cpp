/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include <wtf/Language.h>
#include <wtf/RetainPtr.h>
#include <wtf/TinyLRUCache.h>
#include <wtf/text/StringView.h>
#include <wtf/text/TextBreakIteratorInternalICU.h>

namespace WTF {

template<>
class TinyLRUCachePolicy<RefPtr<AtomStringImpl>, RetainPtr<CFLocaleRef>>
{
public:
    NODELETE static TinyLRUCache<RefPtr<AtomStringImpl>, RetainPtr<CFLocaleRef>>& cache()
    {
        static NeverDestroyed<TinyLRUCache<RefPtr<AtomStringImpl>, RetainPtr<CFLocaleRef>>> cache;
        return cache;
    }

    NODELETE static bool isKeyNull(AtomStringImpl* localeIdentifier)
    {
        return !localeIdentifier;
    }

    static RetainPtr<CFLocaleRef> createValueForNullKey()
    {
        return nullptr;
    }

    static RetainPtr<CFLocaleRef> createValueForKey(AtomStringImpl* localeIdentifier)
    {
        RetainPtr<CFLocaleRef> locale = adoptCF(CFLocaleCreate(kCFAllocatorDefault, createCFString(localeIdentifier).get()));

        return CFStringIsHyphenationAvailableForLocale(locale.get()) ? locale : nullptr;
    }

    NODELETE static RefPtr<AtomStringImpl> createKeyForStorage(AtomStringImpl* key) { return key; }
};
}

namespace WebCore {

bool canHyphenate(AtomStringImpl* localeIdentifier)
{
    if (!localeIdentifier)
        return false;
    return !!TinyLRUCachePolicy<RefPtr<AtomStringImpl>, RetainPtr<CFLocaleRef>>::cache().get(localeIdentifier);
}

size_t lastHyphenLocation(StringView text, size_t beforeIndex, AtomStringImpl* localeIdentifier)
{
    RetainPtr<CFLocaleRef> locale = TinyLRUCachePolicy<RefPtr<AtomStringImpl>, RetainPtr<CFLocaleRef>>::cache().get(localeIdentifier);

    CFOptionFlags searchAcrossWordBoundaries = 1;
    CFIndex result = CFStringGetHyphenationLocationBeforeIndex(text.createCFStringWithoutCopying().get(), beforeIndex, CFRangeMake(0, text.length()), searchAcrossWordBoundaries, locale.get(), nullptr);
    return result == kCFNotFound ? 0 : result;
}

} // namespace WebCore
