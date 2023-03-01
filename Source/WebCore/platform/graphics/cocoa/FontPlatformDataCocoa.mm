/*
 * This file is part of the internal font implementation.
 *
 * Copyright (C) 2006, 2007, 2008, 2009, 2010 Apple Inc. All rights reserved.
 * Copyright (c) 2010 Google Inc. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 *
 */

#import "config.h"
#import "FontPlatformData.h"

#import "FontCacheCoreText.h"
#import "SharedBuffer.h"
#import <pal/spi/cf/CoreTextSPI.h>
#import <wtf/Hasher.h>
#import <wtf/text/StringConcatenateNumbers.h>

#if PLATFORM(IOS_FAMILY)
#import <CoreText/CoreText.h>
#import <pal/spi/cg/CoreGraphicsSPI.h>
#endif

namespace WebCore {

unsigned FontPlatformData::hash() const
{
    // FIXME: Hashing a CFHash is unfortunate here.
    return computeHash(CFHash(m_font.get()), m_widthVariant, m_isHashTableDeletedValue, m_textRenderingMode, m_orientation, m_orientation, m_syntheticBold, m_syntheticOblique);
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
    if (!m_font || !other.m_font)
        return m_font == other.m_font;
    return CFEqual(m_font.get(), other.m_font.get());
}

RefPtr<SharedBuffer> FontPlatformData::platformOpenTypeTable(uint32_t) const
{
    return nullptr;
}

Vector<FontPlatformData::FontVariationAxis> FontPlatformData::variationAxes(ShouldLocalizeAxisNames shouldLocalizeAxisNames) const
{
    auto platformFont = ctFont();
    if (!platformFont)
        return { };
    
    return WTF::map(defaultVariationValues(platformFont, shouldLocalizeAxisNames), [](auto&& entry) {
        auto& [tag, values] = entry;
        return FontPlatformData::FontVariationAxis { values.axisName, String(tag.data(), tag.size()), values.defaultValue, values.minimumValue, values.maximumValue };
    });
}

#if HAVE(CTFONT_COPY_LOCALIZED_NAME_BY_ID)
Vector<FontPlatformData::FontVariationInstance> FontPlatformData::variationInstances() const
{
    auto platformFont = ctFont();
    if (!platformFont)
        return { };

    RetainPtr<CFDataRef> variationTable = adoptCF(CTFontCopyTable(platformFont, kCTFontTableFvar, kCTFontTableOptionNoOptions));
    if (!variationTable)
        return { };

    auto variationTableLength = CFDataGetLength(variationTable.get());

    static constexpr auto openTypeFontVariationTableHeaderLength = 16;
    if (variationTableLength < openTypeFontVariationTableHeaderLength)
        return { };

    const uint8_t* variationTableBytes = CFDataGetBytePtr(variationTable.get());

    uint16_t axesArrayOffset = variationTableBytes[5] | (variationTableBytes[4] << 8);
    uint16_t axisCount = variationTableBytes[9] | (variationTableBytes[8] << 8);
    uint16_t axisSize = variationTableBytes[11] | (variationTableBytes[10] << 8);
    uint16_t instanceCount = variationTableBytes[13] | (variationTableBytes[12] << 8);
    uint16_t instanceSize = variationTableBytes[15] | (variationTableBytes[14] << 8);

    if (variationTableLength != axesArrayOffset + (axisCount * axisSize) + (instanceCount * instanceSize))
        return { };

    // An instance entry must be at least enough bytes for all known axes.
    static constexpr auto openTypeFontVariationTableInstanceNameIdAndFlagsLength = 4;
    static constexpr auto openTypeFontVariationTableInstanceAxisValueLength = 4;
    if (instanceSize < openTypeFontVariationTableInstanceNameIdAndFlagsLength + (openTypeFontVariationTableInstanceAxisValueLength * axisCount))
        return { };

    auto axes = WebCore::variationAxes(platformFont, ShouldLocalizeAxisNames::No);
    if (!axes)
        return { };

    if (CFArrayGetCount(axes.get()) != axisCount)
        return { };

    // Axis values in a variation instance are in the same order the variation axes are defined.
    Vector<String> variationTags;
    for (CFIndex i = 0; i < axisCount; ++i) {
        CFDictionaryRef axis = static_cast<CFDictionaryRef>(CFArrayGetValueAtIndex(axes.get(), i));
        CFNumberRef axisIdentifier = static_cast<CFNumberRef>(CFDictionaryGetValue(axis, kCTFontVariationAxisIdentifierKey));
        auto tag = fontTagForVariationAxisIdentifier(axisIdentifier);

        variationTags.append(String(tag.data(), tag.size()));
    }

    Vector<FontPlatformData::FontVariationInstance> result;
    for (auto instanceOffset = axesArrayOffset + (axisCount * axisSize); instanceOffset < variationTableLength; instanceOffset += instanceSize) {
        ASSERT_WITH_SECURITY_IMPLICATION(instanceOffset + instanceSize <= variationTableLength);

        uint16_t subfamilyNameId = variationTableBytes[instanceOffset + 1] | (variationTableBytes[instanceOffset] << 8);
        String subfamilyName = static_cast<CFStringRef>(CTFontCopyLocalizedNameByIDWithLanguages(platformFont, subfamilyNameId, CFArrayCreate(NULL, NULL, 0, NULL), NULL));
        if (subfamilyName.isEmpty())
            continue;

        Vector<FontPlatformData::FontVariationAxisValue> axisValues;
        for (auto axisIndex = 0; axisIndex < axisCount; ++axisIndex) {
            auto axisOffset = instanceOffset + openTypeFontVariationTableInstanceNameIdAndFlagsLength + (axisIndex * openTypeFontVariationTableInstanceAxisValueLength);

            // TODO: [PCA] Before submitting for review, see if there is a better way to take these four bytes that represent a "fixed" number, and turn them into a float. I'm also not confident in the bit order for the decimal part of the value yet, not that it is obviously wrong, but I didn't bother to check the order for it yet.
            // axisValue is a 4-byte "fixed" number.
            uint16_t axisValueWhole = variationTableBytes[axisOffset + 1] | (variationTableBytes[axisOffset] << 8);
            uint16_t axisValueDecimal = variationTableBytes[axisOffset + 3] | (variationTableBytes[axisOffset + 2] << 8);
            float axisValue = axisValueWhole + ((float)axisValueDecimal / 0xFFFF);

            axisValues.append({ variationTags[axisIndex], axisValue });
        }

        result.append({ WTFMove(subfamilyName), WTFMove(axisValues) });
    }

    return result;
}
#endif // HAVE(CTFONT_COPY_LOCALIZED_NAME_BY_ID)

} // namespace WebCore
