/*
 * Copyright (C) 2024 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "FontPlatformData.h"

#include "FontCache.h"
#include "FontCustomPlatformData.h"
#include "FontRenderOptions.h"
#include "FontVariationsSkia.h"
#include "NotImplemented.h"
#include "OpenTypeTypes.h"
#include "SkiaHarfBuzzFont.h"
#include <skia/core/SkStream.h>
#include <skia/core/SkTypeface.h>
#include <wtf/Hasher.h>
#include <wtf/VectorHash.h>

namespace WebCore {

FontPlatformData::FontPlatformData(sk_sp<SkTypeface>&& typeface, float size, bool syntheticBold, bool syntheticOblique, FontOrientation orientation, FontWidthVariant widthVariant, TextRenderingMode textRenderingMode, Vector<hb_feature_t>&& features, const FontCustomPlatformData* customPlatformData)
    : FontPlatformData(size, syntheticBold, syntheticOblique, orientation, widthVariant, textRenderingMode, customPlatformData)
{
    m_font = SkFont(typeface, m_size);
    m_font.setEmbolden(m_syntheticBold);
    m_font.setSkewX(m_syntheticOblique ? -SK_Scalar1 / 4 : 0);
    m_font.setEdging(FontRenderOptions::singleton().antialias());
    m_font.setHinting(FontRenderOptions::singleton().hinting());

    m_hbFont = SkiaHarfBuzzFont::getOrCreate(*m_font.getTypeface());

    m_features = WTFMove(features);
}

bool FontPlatformData::isFixedPitch() const
{
    return m_font.getTypeface()->isFixedPitch();
}

unsigned FontPlatformData::hash() const
{
    // FIXME: do we need to consider m_features for the hash?
    return computeHash(m_font.getTypeface()->uniqueID(), m_widthVariant, m_isHashTableDeletedValue, m_textRenderingMode, m_orientation, m_syntheticBold, m_syntheticOblique);
}

bool FontPlatformData::platformIsEqual(const FontPlatformData& other) const
{

    return SkTypeface::Equal(m_font.getTypeface(), other.skFont().getTypeface()) && m_features == other.m_features;
}

#if !LOG_DISABLED
String FontPlatformData::description() const
{
    return String();
}
#endif

String FontPlatformData::familyName() const
{
    if (auto* typeface = m_font.getTypeface()) {
        SkString familyName;
        typeface->getFamilyName(&familyName);
        return String::fromUTF8(familyName.data());
    }
    return { };
}

static_assert(std::is_same<SkFontTableTag, OpenType::Tag>::value);

RefPtr<SharedBuffer> FontPlatformData::openTypeTable(uint32_t table) const
{
    auto* typeface = m_font.getTypeface();
    if (!typeface)
        return nullptr;

    OpenType::Tag tag = OT_MAKE_TAG(table >> 24, (table & 0xff0000) >> 16, (table & 0xff00) >> 8, (table & 0xff));
    size_t tableSize = typeface->getTableSize(tag);
    if (!tableSize)
        return nullptr;

    Vector<uint8_t> data(tableSize);
    if (typeface->getTableData(tag, 0, tableSize, data.data()) != tableSize)
        return nullptr;

    return SharedBuffer::create(WTFMove(data));
}

FontPlatformData FontPlatformData::create(const Attributes& data, const FontCustomPlatformData* custom)
{
    ASSERT(custom);
    sk_sp<SkTypeface> typeface = custom->m_typeface;
    Vector<hb_feature_t> features = data.m_features;
    return FontPlatformData(WTFMove(typeface), data.m_size, data.m_syntheticBold, data.m_syntheticOblique, data.m_orientation, data.m_widthVariant, data.m_textRenderingMode, WTFMove(features), custom);
}

FontPlatformData::Attributes FontPlatformData::attributes() const
{
    Attributes result(m_size, m_orientation, m_widthVariant, m_textRenderingMode, m_syntheticBold, m_syntheticOblique);
    result.m_features = m_features;
    return result;
}

hb_font_t* FontPlatformData::hbFont() const
{
    return m_hbFont->scaledFont(*this);
}

#if ENABLE(MATHML)
HbUniquePtr<hb_font_t> FontPlatformData::createOpenTypeMathHarfBuzzFont() const
{
    auto* face = hb_font_get_face(hbFont());
    if (!hb_ot_math_has_data(face))
        return nullptr;

    return HbUniquePtr<hb_font_t>(hb_font_create(face));
}
#endif

void FontPlatformData::updateSize(float size)
{
    m_size = size;
    m_font.setSize(m_size);
}

Vector<FontPlatformData::FontVariationAxis> FontPlatformData::variationAxes(ShouldLocalizeAxisNames) const
{
    auto* typeface = m_font.getTypeface();
    if (!typeface)
        return { };

    return WTF::map(defaultFontVariationValues(*typeface), [](auto&& entry) {
        auto& [tag, values] = entry;
        return FontPlatformData::FontVariationAxis { values.axisName, String(tag), values.defaultValue, values.minimumValue, values.maximumValue };
    });
}

} // namespace WebCore
