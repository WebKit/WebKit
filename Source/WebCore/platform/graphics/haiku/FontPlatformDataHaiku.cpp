/*
 * Copyright (C) 2009 Maxime Simon <simon.maxime@gmail.com>
 * Copyright (C) 2010 Stephan Aßmus <superstippi@gmx.de>
 *
 * All rights reserved.
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
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#include "config.h"
#include "FontDescription.h"
#include "FontPlatformData.h"
#include "SharedBuffer.h"
#include "NotImplemented.h"

#include <String.h>

#include <wtf/text/AtomString.h>
#include <wtf/text/CString.h>

namespace WebCore {
font_family FontPlatformData::m_FallbackSansSerifFontFamily= "Noto Sans";
font_family FontPlatformData::m_FallbackSerifFontFamily = "Noto Serif";
font_family FontPlatformData::m_FallbackFixedFontFamily = "Noto Mono";
font_family FontPlatformData::m_FallbackStandardFontFamily = "Noto Sans";

void
FontPlatformData::findMatchingFontFamily(const AtomString& familyName, font_family& fontFamily)
{
    CString familyNameUTF8 = familyName.string().utf8();
    if (BFont().SetFamilyAndStyle(familyNameUTF8.data(), 0) == B_OK)
        strlcpy(fontFamily, familyNameUTF8.data(), B_FONT_FAMILY_LENGTH);
    else {
        // If no font family is found for the given name, we use a generic font.
        if (familyName.contains("Sans"))
            strlcpy(fontFamily, m_FallbackSansSerifFontFamily, B_FONT_FAMILY_LENGTH);
        else if (familyName.contains("Serif"))
            strlcpy(fontFamily, m_FallbackSerifFontFamily, B_FONT_FAMILY_LENGTH);
        else if (familyName.contains("Mono"))
            strlcpy(fontFamily, m_FallbackFixedFontFamily, B_FONT_FAMILY_LENGTH);
        else {
            // This is the fallback font.
            strlcpy(fontFamily, m_FallbackStandardFontFamily, B_FONT_FAMILY_LENGTH);
        }
    }
}

static void findMatchingFontStyle(const font_family& fontFamily, bool bold, bool oblique, font_style* fontStyle)
{
    int32 numStyles = count_font_styles(const_cast<char*>(fontFamily));

    for (int i = 0; i < numStyles; i++) {
        if (get_font_style(const_cast<char*>(fontFamily), i, fontStyle) == B_OK) {
            String styleName(*fontStyle);
            if ((oblique && bold) && (styleName == "Bold Italic" || styleName == "Bold Oblique"))
                return;
            if ((oblique && !bold) && (styleName == "Italic" || styleName == "Oblique"))
                return;
            if ((!oblique && bold) && styleName == "Bold")
                return;
            if ((!oblique && !bold) && (styleName == "Roma" || styleName == "Book"
                || styleName == "Condensed" || styleName == "Regular" || styleName == "Medium")) {
                return;
            }
        }
    }

    // Didn't find matching style
    memset(*fontStyle, 0, sizeof(font_style));
}

// #pragma mark -

FontPlatformData::FontPlatformData(const FontDescription& fontDescription, const AtomString& familyName)
{
	m_font = std::make_unique<BFont>();
    m_font->SetSize(fontDescription.computedSize());

    font_family fontFamily;
    findMatchingFontFamily(familyName, fontFamily);

    font_style fontStyle;
    findMatchingFontStyle(fontFamily, fontDescription.weight() == boldWeightValue(), fontDescription.italic() != WTF::nullopt, &fontStyle);

    m_font->SetFamilyAndStyle(fontFamily, fontStyle);

	m_size = m_font->Size();
}

FontPlatformData::FontPlatformData(const FontPlatformData& other)
    : m_isHashTableDeletedValue(other.m_isHashTableDeletedValue)
    , m_size(other.m_size)
    , m_syntheticBold(other.m_syntheticBold)
    , m_syntheticOblique(other.m_syntheticOblique)
    , m_isColorBitmapFont(other.m_isColorBitmapFont)
    , m_orientation(other.m_orientation)
    , m_widthVariant(other.m_widthVariant)
    , m_textRenderingMode(other.m_textRenderingMode)
{
	if (other.m_font != nullptr) {
		m_font = std::make_unique<BFont>(other.m_font.get());
	}
}

WebCore::FontPlatformData&
FontPlatformData::operator=(const FontPlatformData& other)
{
    m_isHashTableDeletedValue = other.m_isHashTableDeletedValue;
    m_size = other.m_size;
    m_syntheticBold = other.m_syntheticBold;
    m_syntheticOblique = other.m_syntheticOblique;
    m_isColorBitmapFont = other.m_isColorBitmapFont;
    m_orientation = other.m_orientation;
    m_widthVariant = other.m_widthVariant;
    m_textRenderingMode = other.m_textRenderingMode;

	if (other.m_font != nullptr) {
		m_font = std::make_unique<BFont>(other.m_font.get());
	} else
		m_font = nullptr;

	return *this;
}

FontPlatformData FontPlatformData::cloneWithSize(const FontPlatformData& source, float size)
{
    FontPlatformData copy(source);
    copy.m_size = size;
	copy.m_font->SetSize(size);
    return copy;
}


unsigned FontPlatformData::hash() const
{
	return PtrHash<BFont*>::hash(m_font.get());
}


bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{
	if (m_font == nullptr && other.m_font == nullptr)
		return true;
	if (m_font == nullptr || other.m_font == nullptr)
		return false;
	return (*m_font == *other.m_font);
}

bool FontPlatformData::isFixedPitch() const
{
	if (m_font == nullptr)
		return false;
	return m_font->Spacing() == B_FIXED_SPACING;
}

void
FontPlatformData::SetFallBackSerifFont(const BString& font)
{
	strlcpy(m_FallbackSerifFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

void
FontPlatformData::SetFallBackSansSerifFont(const BString& font)
{
	strlcpy(m_FallbackSansSerifFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

void
FontPlatformData::SetFallBackFixedFont(const BString& font)
{
	strlcpy(m_FallbackFixedFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

void
FontPlatformData::SetFallBackStandardFont(const BString& font)
{
	strlcpy(m_FallbackStandardFontFamily, font.String(), B_FONT_FAMILY_LENGTH);
}

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
	notImplemented();
	return nullptr;
}

#if !LOG_DISABLED
String FontPlatformData::description() const
{
    return String();
}
#endif

} // namespace WebCore

