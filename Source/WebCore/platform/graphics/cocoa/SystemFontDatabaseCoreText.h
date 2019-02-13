/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "FontCascadeDescription.h"
#include <pal/spi/cocoa/CoreTextSPI.h>
#include <wtf/HashMap.h>
#include <wtf/HashTraits.h>
#include <wtf/text/AtomicString.h>
#include <wtf/text/AtomicStringHash.h>

namespace WebCore {

class SystemFontDatabaseCoreText {
public:
    struct CascadeListParameters {
        CascadeListParameters()
        {
        }

        CascadeListParameters(WTF::HashTableDeletedValueType)
            : fontName(WTF::HashTableDeletedValue)
        {
        }

        bool isHashTableDeletedValue() const
        {
            return fontName.isHashTableDeletedValue();
        }

        bool operator==(const CascadeListParameters& other) const
        {
            return fontName == other.fontName
                && locale == other.locale
                && weight == other.weight
                && size == other.size
                && allowUserInstalledFonts == other.allowUserInstalledFonts
                && italic == other.italic;
        }

        unsigned hash() const
        {
            IntegerHasher hasher;
            ASSERT(!fontName.isNull());
            hasher.add(locale.existingHash());
            hasher.add(locale.isNull() ? 0 : locale.existingHash());
            hasher.add(weight);
            hasher.add(size);
            hasher.add(static_cast<unsigned>(allowUserInstalledFonts));
            hasher.add(italic);
            return hasher.hash();
        }

        struct CascadeListParametersHash : WTF::PairHash<AtomicString, float> {
            static unsigned hash(const CascadeListParameters& parameters)
            {
                return parameters.hash();
            }
            static bool equal(const CascadeListParameters& a, const CascadeListParameters& b)
            {
                return a == b;
            }
            static const bool safeToCompareToEmptyOrDeleted = true;
        };

        AtomicString fontName;
        AtomicString locale;
        CGFloat weight { 0 };
        float size { 0 };
        AllowUserInstalledFonts allowUserInstalledFonts { AllowUserInstalledFonts::No };
        bool italic { false };
    };

    static SystemFontDatabaseCoreText& singleton();

    enum class ClientUse { ForSystemUI, ForTextStyle };

    Vector<RetainPtr<CTFontDescriptorRef>> cascadeList(const FontCascadeDescription&, const AtomicString& cssFamily, ClientUse, AllowUserInstalledFonts);

    String serifFamily(const String& locale);
    String sansSerifFamily(const String& locale);
    String cursiveFamily(const String& locale);
    String fantasyFamily(const String& locale);
    String monospaceFamily(const String& locale);

    void clear();

private:
    SystemFontDatabaseCoreText();

    Vector<RetainPtr<CTFontDescriptorRef>> cascadeList(const CascadeListParameters&, ClientUse);

    static RetainPtr<CTFontRef> applyWeightItalicsAndFallbackBehavior(CTFontRef, CGFloat weight, bool italic, float size, AllowUserInstalledFonts);
    static RetainPtr<CTFontDescriptorRef> removeCascadeList(CTFontDescriptorRef);
    static Vector<RetainPtr<CTFontDescriptorRef>> computeCascadeList(CTFontRef, CFStringRef locale);
    static CascadeListParameters systemFontParameters(const FontCascadeDescription&, const AtomicString& familyName, ClientUse, AllowUserInstalledFonts);

    HashMap<CascadeListParameters, Vector<RetainPtr<CTFontDescriptorRef>>, CascadeListParameters::CascadeListParametersHash, SimpleClassHashTraits<CascadeListParameters>> m_systemFontCache;

    HashMap<String, String> m_serifFamilies;
    HashMap<String, String> m_sansSeriferifFamilies;
    HashMap<String, String> m_cursiveFamilies;
    HashMap<String, String> m_fantasyFamilies;
    HashMap<String, String> m_monospaceFamilies;
};

}
