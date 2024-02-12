/*
 * Copyright (C) 2024 Igalia S.L.
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

#include "config.h"
#include "FontCustomPlatformData.h"

#include "FontCache.h"
#include "FontCreationContext.h"
#include "FontDescription.h"
#include "FontVariationsSkia.h"
#include "NotImplemented.h"
#include <skia/core/SkData.h>

namespace WebCore {

FontCustomPlatformData::FontCustomPlatformData(sk_sp<SkTypeface>&& typeface, FontPlatformData::CreationData&& data)
    : m_typeface(WTFMove(typeface))
    , creationData(WTFMove(data))
    , m_renderingResourceIdentifier(RenderingResourceIdentifier::generate())
{
}

FontCustomPlatformData::~FontCustomPlatformData() = default;

FontPlatformData FontCustomPlatformData::fontPlatformData(const FontDescription& description, bool bold, bool italic, const FontCreationContext& fontCreationContext)
{
    sk_sp<SkTypeface> typeface = m_typeface;

    auto defaultValues = defaultFontVariationValues(*typeface);
    if (!defaultValues.isEmpty()) {
        Vector<SkFontArguments::VariationPosition::Coordinate> variationsToBeApplied;
        auto applyVariation = [&](const FontTag& tag, float value) {
            auto iterator = defaultValues.find(tag);
            if (iterator == defaultValues.end())
                return;

            variationsToBeApplied.append({ SkSetFourByteTag(tag[0], tag[1], tag[2], tag[3]), iterator->value.clamp(value) });
        };

        float weight = description.weight();
        if (auto weightValue = fontCreationContext.fontFaceCapabilities().weight)
            weight = std::max(std::min(weight, static_cast<float>(weightValue->maximum)), static_cast<float>(weightValue->minimum));
        applyVariation({ { 'w', 'g', 'h', 't' } }, weight);

        float width = description.stretch();
        if (auto widthValue = fontCreationContext.fontFaceCapabilities().width)
            width = std::max(std::min(width, static_cast<float>(widthValue->maximum)), static_cast<float>(widthValue->minimum));
        applyVariation({ { 'w', 'd', 't', 'h' } }, width);

        if (description.fontStyleAxis() == FontStyleAxis::ital)
            applyVariation({ { 'i', 't', 'a', 'l' } }, 1);
        else {
            float slope = description.italic().value_or(normalItalicValue());
            if (auto slopeValue = fontCreationContext.fontFaceCapabilities().weight)
                slope = std::max(std::min(slope, static_cast<float>(slopeValue->maximum)), static_cast<float>(slopeValue->minimum));
            applyVariation({ { 's', 'l', 'n', 't' } }, slope);
        }

        // FIXME: optical sizing.

        const auto& variations = description.variationSettings();
        for (auto& variation : variations)
            applyVariation(variation.tag(), variation.value());

        if (!variationsToBeApplied.isEmpty()) {
            SkFontArguments fontArgs;
            fontArgs.setVariationDesignPosition({ variationsToBeApplied.data(), static_cast<int>(variationsToBeApplied.size()) });
            if (auto variationTypeface = typeface->makeClone(fontArgs))
                typeface = WTFMove(variationTypeface);
        }
    }

    auto size = description.adjustedSizeForFontFace(fontCreationContext.sizeAdjust());
    auto features = FontCache::computeFeatures(description, fontCreationContext);
    FontPlatformData platformData(WTFMove(typeface), size, bold, italic, description.orientation(), description.widthVariant(), description.textRenderingMode(), WTFMove(features));
    platformData.updateSizeWithFontSizeAdjust(description.fontSizeAdjust(), description.computedSize());
    return platformData;
}

RefPtr<FontCustomPlatformData> createFontCustomPlatformData(SharedBuffer& buffer, const String& itemInCollection)
{
    sk_sp<SkTypeface> typeface = FontCache::forCurrentThread().fontManager()->makeFromData(buffer.createSkData());
    FontPlatformData::CreationData creationData = { buffer, itemInCollection };
    return adoptRef(new FontCustomPlatformData(WTFMove(typeface), WTFMove(creationData)));
}

bool FontCustomPlatformData::supportsFormat(const String& format)
{
    return equalLettersIgnoringASCIICase(format, "truetype"_s)
        || equalLettersIgnoringASCIICase(format, "opentype"_s)
#if HAVE(WOFF_SUPPORT) || USE(WOFF2)
        || equalLettersIgnoringASCIICase(format, "woff2"_s)
#if ENABLE(VARIATION_FONTS)
        || equalLettersIgnoringASCIICase(format, "woff2-variations"_s)
#endif
#endif
#if ENABLE(VARIATION_FONTS)
        || equalLettersIgnoringASCIICase(format, "woff-variations"_s)
        || equalLettersIgnoringASCIICase(format, "truetype-variations"_s)
        || equalLettersIgnoringASCIICase(format, "opentype-variations"_s)
#endif
        || equalLettersIgnoringASCIICase(format, "woff"_s)
        || equalLettersIgnoringASCIICase(format, "svg"_s);
}

bool FontCustomPlatformData::supportsTechnology(const FontTechnology&)
{
    // FIXME: define supported technologies for this platform (webkit.org/b/256310).
    notImplemented();
    return true;
}

}
