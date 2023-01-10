/*
 * Copyright (C) 2015-2021 Apple Inc. All rights reserved.
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
#include "FontDatabase.h"

#include "FontCacheCoreText.h"
#include "FontCascadeDescription.h"
#include <pal/spi/cf/CoreTextSPI.h>

namespace WebCore {

FontDatabase::InstalledFont::InstalledFont(CTFontDescriptorRef fontDescriptor)
    : fontDescriptor(fontDescriptor)
    , capabilities(capabilitiesForFontDescriptor(fontDescriptor))
{
}

FontDatabase::InstalledFontFamily::InstalledFontFamily(Vector<InstalledFont>&& installedFonts)
    : installedFonts(WTFMove(installedFonts))
{
    for (auto& font : this->installedFonts)
        expand(font);
}

void FontDatabase::InstalledFontFamily::expand(const InstalledFont& installedFont)
{
    capabilities.expand(installedFont.capabilities);
}

const FontDatabase::InstalledFontFamily& FontDatabase::collectionForFamily(const String& familyName)
{
    auto folded = FontCascadeDescription::foldedFamilyName(familyName);
    {
        Locker locker { m_familyNameToFontDescriptorsLock };
        auto it = m_familyNameToFontDescriptors.find(folded);
        if (it != m_familyNameToFontDescriptors.end())
            return *it->value;
    }

    auto installedFontFamily = [&] {
        auto familyNameString = folded.createCFString();
        auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(attributes.get(), kCTFontFamilyNameAttribute, familyNameString.get());
        addAttributesForInstalledFonts(attributes.get(), m_allowUserInstalledFonts);
        auto fontDescriptorToMatch = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
        auto mandatoryAttributes = installedFontMandatoryAttributes(m_allowUserInstalledFonts);
        if (auto matches = adoptCF(CTFontDescriptorCreateMatchingFontDescriptors(fontDescriptorToMatch.get(), mandatoryAttributes.get()))) {
            auto count = CFArrayGetCount(matches.get());
            Vector<InstalledFont> result;
            result.reserveInitialCapacity(count);
            for (CFIndex i = 0; i < count; ++i) {
                InstalledFont installedFont(static_cast<CTFontDescriptorRef>(CFArrayGetValueAtIndex(matches.get(), i)));
                result.uncheckedAppend(WTFMove(installedFont));
            }
            return makeUnique<InstalledFontFamily>(WTFMove(result));
        }
        return makeUnique<InstalledFontFamily>();
    }();

    Locker locker { m_familyNameToFontDescriptorsLock };
    return *m_familyNameToFontDescriptors.add(folded.isolatedCopy(), WTFMove(installedFontFamily)).iterator->value;
}

const FontDatabase::InstalledFont& FontDatabase::fontForPostScriptName(const AtomString& postScriptName)
{
    const auto& folded = FontCascadeDescription::foldedFamilyName(postScriptName);
    return m_postScriptNameToFontDescriptors.ensure(folded, [&] {
        auto postScriptNameString = folded.createCFString();
        CFStringRef nameAttribute = kCTFontPostScriptNameAttribute;
        auto attributes = adoptCF(CFDictionaryCreateMutable(kCFAllocatorDefault, 0, &kCFTypeDictionaryKeyCallBacks, &kCFTypeDictionaryValueCallBacks));
        CFDictionaryAddValue(attributes.get(), kCTFontEnabledAttribute, kCFBooleanTrue);
        CFDictionaryAddValue(attributes.get(), nameAttribute, postScriptNameString.get());
        addAttributesForInstalledFonts(attributes.get(), m_allowUserInstalledFonts);
        auto fontDescriptorToMatch = adoptCF(CTFontDescriptorCreateWithAttributes(attributes.get()));
        auto mandatoryAttributes = installedFontMandatoryAttributes(m_allowUserInstalledFonts);
        auto match = adoptCF(CTFontDescriptorCreateMatchingFontDescriptor(fontDescriptorToMatch.get(), mandatoryAttributes.get()));
        return InstalledFont(match.get());
    }).iterator->value;
}

void FontDatabase::clear()
{
    {
        Locker locker { m_familyNameToFontDescriptorsLock };
        m_familyNameToFontDescriptors.clear();
    }
    m_postScriptNameToFontDescriptors.clear();
}

FontDatabase::FontDatabase(AllowUserInstalledFonts allowUserInstalledFonts)
    : m_allowUserInstalledFonts(allowUserInstalledFonts)
{
}

} // namespace WebCore
