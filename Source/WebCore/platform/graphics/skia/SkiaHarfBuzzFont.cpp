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
#include "SkiaHarfBuzzFont.h"

#if USE(SKIA)
#include "FontCache.h"
#include "FontCascade.h"
#include "FontPlatformData.h"
#include "NotImplemented.h"
#include "SkiaHarfBuzzFontCache.h"
#include <skia/core/SkStream.h>
#include <wtf/unicode/CharacterNames.h>

namespace WebCore {

Ref<SkiaHarfBuzzFont> SkiaHarfBuzzFont::getOrCreate(SkTypeface& typeface)
{
    return FontCache::forCurrentThread().harfBuzzFontCache().font(typeface);
}

static hb_font_funcs_t* harfBuzzFontFunctions()
{
    static hb_font_funcs_t* fontFunctions = nullptr;

    if (!fontFunctions) {
        fontFunctions = hb_font_funcs_create();

        hb_font_funcs_set_nominal_glyph_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t unicode, hb_codepoint_t* glyph, void*) -> hb_bool_t {
            auto& skiaHarfBuzzFont = *static_cast<SkiaHarfBuzzFont*>(context);
            auto hbGlyph = skiaHarfBuzzFont.glyph(unicode);
            if (!hbGlyph)
                return false;
            *glyph = hbGlyph.value();
            return true;
        }, nullptr, nullptr);

        hb_font_funcs_set_variation_glyph_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t unicode, hb_codepoint_t variation, hb_codepoint_t* glyph, void*) -> hb_bool_t {
            auto& skiaHarfBuzzFont = *static_cast<SkiaHarfBuzzFont*>(context);
            auto hbGlyph = skiaHarfBuzzFont.glyph(unicode, variation);
            if (!hbGlyph)
                return false;
            *glyph = hbGlyph.value();
            return true;
        }, nullptr, nullptr);

        hb_font_funcs_set_glyph_h_advance_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t glyph, void*) -> hb_position_t {
            auto& skiaHarfBuzzFont = *static_cast<SkiaHarfBuzzFont*>(context);
            return skiaHarfBuzzFont.glyphWidth(glyph);
        }, nullptr, nullptr);

#if HB_VERSION_ATLEAST(1, 8, 6)
        hb_font_funcs_set_glyph_h_advances_func(fontFunctions, [](hb_font_t*, void* context, unsigned count, const hb_codepoint_t* glyphs, unsigned glyphStride, hb_position_t* advances, unsigned advanceStride, void*) {
            auto& skiaHarfBuzzFont = *static_cast<SkiaHarfBuzzFont*>(context);
            skiaHarfBuzzFont.glyphWidths(count, glyphs, glyphStride, advances, advanceStride);
        }, nullptr, nullptr);

#endif

        hb_font_funcs_set_glyph_extents_func(fontFunctions, [](hb_font_t*, void* context, hb_codepoint_t glyph, hb_glyph_extents_t* extents, void*) -> hb_bool_t {
            auto& skiaHarfBuzzFont = *static_cast<SkiaHarfBuzzFont*>(context);
            skiaHarfBuzzFont.glyphExtents(glyph, extents);
            return true;
        }, nullptr, nullptr);

        hb_font_funcs_make_immutable(fontFunctions);
    }
    return fontFunctions;
}

static HbUniquePtr<hb_face_t> createHarfBuzzFace(SkTypeface& typeface)
{
    int index;
    std::unique_ptr<SkStreamAsset> stream(typeface.openStream(&index));
    if (stream) {
        if (const auto* memory = stream->getMemoryBase()) {
            auto size = static_cast<unsigned>(stream->getLength());
            HbUniquePtr<hb_blob_t> blob(hb_blob_create(reinterpret_cast<const char*>(memory), size, HB_MEMORY_MODE_READONLY, stream.release(), [](void* data) {
                delete reinterpret_cast<SkStreamAsset*>(data);
            }));
            auto faceCount = hb_face_count(blob.get());
            if (faceCount && static_cast<unsigned>(index) < faceCount)
                return HbUniquePtr<hb_face_t>(hb_face_create(blob.get(), index));
        }
    }

    return HbUniquePtr<hb_face_t>(hb_face_create_for_tables([](hb_face_t*, hb_tag_t tag, void* userData) -> hb_blob_t* {
        SkTypeface& typeface = *reinterpret_cast<SkTypeface*>(userData);
        auto tableData = typeface.copyTableData(tag);
        if (!tableData)
            return nullptr;

        const auto* data = reinterpret_cast<const char*>(tableData->data());
        auto dataSize = tableData->size();
        return hb_blob_create(data, dataSize, HB_MEMORY_MODE_WRITABLE, tableData.release(), [](void* data) {
            sk_sp<SkData> tableData(reinterpret_cast<SkData*>(data));
        });
    }, &typeface, nullptr));
}

SkiaHarfBuzzFont::SkiaHarfBuzzFont(SkTypeface& typeface)
    : m_uniqueID(typeface.uniqueID())
{
    auto hbFace = createHarfBuzzFace(typeface);
    HbUniquePtr<hb_font_t> hbFont(hb_font_create(hbFace.get()));

    if (int axisCount = typeface.getVariationDesignPosition(nullptr, 0)) {
        Vector<SkFontArguments::VariationPosition::Coordinate> axisValues(axisCount);
        if (typeface.getVariationDesignPosition(axisValues.data(), axisValues.size()) != -1)
            hb_font_set_variations(hbFont.get(), reinterpret_cast<hb_variation_t*>(axisValues.data()), axisValues.size());
    }

    // Create a subfont with custom functions so that the missing ones are taken from the parent.
    m_font.reset(hb_font_create_sub_font(hbFont.get()));
    hb_font_set_funcs(m_font.get(), harfBuzzFontFunctions(), this, nullptr);
}

SkiaHarfBuzzFont::~SkiaHarfBuzzFont()
{
    FontCache::forCurrentThread().harfBuzzFontCache().remove(m_uniqueID);
}

static inline hb_position_t skScalarToHarfBuzzPosition(SkScalar value)
{
    static constexpr int hbPosition = 1 << 16;
    return clampTo<int>(value * hbPosition);
}

hb_font_t* SkiaHarfBuzzFont::scaledFont(const FontPlatformData& fontPlatformData)
{
    float size = fontPlatformData.size();
    auto scale = skScalarToHarfBuzzPosition(size);
    hb_font_set_scale(m_font.get(), scale, scale);
    hb_font_set_ptem(m_font.get(), size);
    m_scaledFont = fontPlatformData.skFont();
    return m_font.get();
}

std::optional<hb_codepoint_t> SkiaHarfBuzzFont::glyph(hb_codepoint_t unicode, std::optional<hb_codepoint_t> variation)
{
    if (FontCascade::treatAsSpace(unicode))
        unicode = space;
    else if (FontCascade::treatAsZeroWidthSpaceInComplexScript(unicode))
        unicode = zeroWidthSpace;

    hb_codepoint_t returnValue;
    if (hb_font_get_glyph(hb_font_get_parent(m_font.get()), unicode, variation.value_or(0), &returnValue))
        return returnValue;
    return std::nullopt;
}

hb_position_t SkiaHarfBuzzFont::glyphWidth(hb_codepoint_t glyph)
{
    SkGlyphID glyphID = glyph;
    SkScalar width;
    m_scaledFont.getWidths(&glyphID, 1, &width);
    if (!m_scaledFont.isSubpixel())
        width = SkScalarRoundToInt(width);
    return skScalarToHarfBuzzPosition(width);
}

void SkiaHarfBuzzFont::glyphWidths(unsigned count, const hb_codepoint_t* glyphs, unsigned glyphStride, hb_position_t* advances, unsigned advanceStride)
{
    WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN // Glib/Win port

    Vector<SkGlyphID, 256> skGlyphs(count);
    for (unsigned i = 0; i < count; ++i) {
        skGlyphs[i] = *glyphs;
        glyphs = reinterpret_cast<const hb_codepoint_t*>(reinterpret_cast<const uint8_t*>(glyphs) + glyphStride);
    }

    Vector<SkScalar, 256> widths(count);
    m_scaledFont.getWidths(skGlyphs.data(), count, widths.data());
    if (!m_scaledFont.isSubpixel()) {
        for (unsigned i = 0; i < count; ++i)
            widths[i] = SkScalarRoundToInt(widths[i]);
    }

    for (unsigned i = 0; i < count; ++i) {
        *advances = skScalarToHarfBuzzPosition(widths[i]);
        advances = reinterpret_cast<hb_position_t*>(reinterpret_cast<uint8_t*>(advances) + advanceStride);
    }

    WTF_ALLOW_UNSAFE_BUFFER_USAGE_END
}

void SkiaHarfBuzzFont::glyphExtents(hb_codepoint_t glyph, hb_glyph_extents_t* extents)
{
    SkGlyphID glyphID = glyph;
    SkRect bounds;
    m_scaledFont.getBounds(&glyphID, 1, &bounds, nullptr);
    if (!m_scaledFont.isSubpixel())
        bounds.set(bounds.roundOut());

    extents->x_bearing = skScalarToHarfBuzzPosition(bounds.fLeft);
    extents->y_bearing = skScalarToHarfBuzzPosition(-bounds.fTop);
    extents->width = skScalarToHarfBuzzPosition(bounds.width());
    extents->height = skScalarToHarfBuzzPosition(-bounds.height());
}

} // namespace WebCore

#endif // USE(SKIA)
