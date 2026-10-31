/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "DisplayListData.h"

#if ENABLE(GPU_PROCESS)

#include "SharedFont.h"
#include <WebCore/DisplayListRecorderImpl.h>
#include <wtf/HashMap.h>
#include <wtf/HashSet.h>

namespace WebKit {
using namespace WebCore;

static bool hasFEImage(const Filter& filter)
{
    return !filter.effectsOfType(FilterFunction::Type::FEImage).isEmpty();
}

static std::optional<SourceBrushData> sourceBrushData(const SourceBrush& brush)
{
    if (brush.pattern())
        return std::nullopt;
    SourceBrushData result { brush.color(), std::monostate { } };
    if (RefPtr gradient = brush.gradient())
        result.patternGradient = SourceBrushLogicalGradient { gradient.releaseNonNull(), brush.gradientSpaceTransform() };
    return result;
}

static std::optional<SetStateData> setStateData(const GraphicsContextState& state)
{
    auto fillBrush = sourceBrushData(state.fillBrush());
    auto strokeBrush = sourceBrushData(state.strokeBrush());
    if (!fillBrush || !strokeBrush)
        return std::nullopt;
    return SetStateData {
        state.changes(),
        WTF::move(*fillBrush),
        state.fillRule(),
        WTF::move(*strokeBrush),
        state.strokeThickness(),
        state.strokeStyle(),
        state.compositeMode(),
        state.dropShadow(),
        state.style(),
        state.alpha(),
        state.imageInterpolationQuality(),
        state.textDrawingMode(),
        state.shouldAntialias(),
        state.shouldSmoothFonts(),
        state.shouldSubpixelQuantizeFonts(),
        state.shadowsIgnoreTransforms()
    };
}

namespace {

class DisplayListDataEncoder {
public:
    std::optional<DisplayListData> encode(const DisplayList::DisplayList& displayList)
    {
        if (!encodeDisplayList(displayList))
            return std::nullopt;
        return DisplayListData { WTF::move(m_customFonts), WTF::move(m_fonts), WTF::move(m_displayLists) };
    }

private:
    std::optional<uint64_t> encodeDisplayList(const DisplayList::DisplayList& displayList)
    {
        if (auto iterator = m_indices.find(&displayList); iterator != m_indices.end())
            return iterator->value;

        // A placeholder can draw anything, including a display list that draws it.
        if (!m_displayListsBeingEncoded.add(&displayList).isNewEntry)
            return std::nullopt;

        Vector<DisplayListDataItem> items;
        bool success = appendItems(displayList.items(), items);
        m_displayListsBeingEncoded.remove(&displayList);
        if (!success)
            return std::nullopt;

        m_displayLists.append(WTF::move(items));
        uint64_t index = m_displayLists.size() - 1;
        m_indices.add(&displayList, index);
        return index;
    }

    bool appendItems(std::span<const DisplayList::Item> items, Vector<DisplayListDataItem>& result)
    {
        for (auto& item : items) {
            if (!appendItem(item, result))
                return false;
        }
        return true;
    }

    bool appendItem(const DisplayList::Item& item, Vector<DisplayListDataItem>& result)
    {
        return WTF::switchOn(item,
            [&](const DisplayList::DrawDisplayList& item) {
                auto index = encodeDisplayList(item.displayList());
                if (!index)
                    return false;
                result.constructAndAppend(WTF::InPlaceType<DrawDisplayListAtIndex>, *index);
                return true;
            },
            [&](const DisplayList::DrawPlaceholder& item) {
                Ref recorder = DisplayList::RecorderImpl::create(FloatRect { });
                item.apply(recorder.get());
                Ref contents = recorder->takeDisplayList();
                return appendItems(contents->items(), result);
            },
            [&](const DisplayList::DrawGlyphs& item) {
                auto advances = item.advances().map([](auto& advance) {
                    return WebCore::size(advance);
                });
                Ref font = item.font();
                result.constructAndAppend(WTF::InPlaceType<DrawGlyphsWithFontAtIndex>, fontIndex(font), item.glyphs(), WTF::move(advances), item.localAnchor(), item.fontSmoothingMode());
                return true;
            },
            [&](const DisplayList::DrawFilteredImageBuffer& item) {
                if (item.sourceImage() || hasFEImage(item.filter()))
                    return false;
                result.constructAndAppend(WTF::InPlaceType<DrawFilteredImageBuffer>, item.sourceImageRect(), item.filter());
                return true;
            },
            [&](const DisplayList::DrawSystemImage& item) {
#if USE(SYSTEM_PREVIEW)
                if (item.systemImage().systemImageType() == SystemImageType::ARKitBadge)
                    return false;
#endif
                result.constructAndAppend(WTF::InPlaceType<DisplayList::DrawSystemImage>, item);
                return true;
            },
            [&](const DisplayList::SetState& item) {
                auto state = setStateData(item.state());
                if (!state)
                    return false;
                result.constructAndAppend(WTF::InPlaceType<SetStateData>, WTF::move(*state));
                return true;
            },
            [](const DisplayList::ClipToImageBuffer&) { return false; },
            [](const DisplayList::DrawImageBuffer&) { return false; },
            [](const DisplayList::DrawNativeImage&) { return false; },
            [](const DisplayList::DrawPatternNativeImage&) { return false; },
            [](const DisplayList::DrawPatternImageBuffer&) { return false; },
#if USE(SKIA)
            [](const DisplayList::DrawTextBlob&) { return false; },
#endif
            [&](const auto& item) {
                result.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, item);
                return true;
            });
    }

    uint64_t fontIndex(const FontBase& font)
    {
        return m_fontIndices.ensure(&font, [&] {
            std::optional<uint64_t> customFontIndex;
            if (RefPtr customFont = font.platformData().customPlatformData()) {
                customFontIndex = m_customFontIndices.ensure(customFont, [&] {
                    m_customFonts.append(customFont->serializedData());
                    return m_customFonts.size() - 1;
                }).iterator->value;
            }
            m_fonts.append(DisplayListDataFont { font.attributes(), font.platformData().attributes(), customFontIndex });
            return m_fonts.size() - 1;
        }).iterator->value;
    }

    Vector<FontCustomPlatformSerializedData> m_customFonts;
    Vector<DisplayListDataFont> m_fonts;
    Vector<Vector<DisplayListDataItem>> m_displayLists;
    HashMap<RefPtr<const FontCustomPlatformData>, uint64_t> m_customFontIndices;
    HashMap<RefPtr<const FontBase>, uint64_t> m_fontIndices;
    HashMap<RefPtr<const DisplayList::DisplayList>, uint64_t> m_indices;
    HashSet<RefPtr<const DisplayList::DisplayList>> m_displayListsBeingEncoded;
};

} // namespace

static SourceBrush sourceBrush(const SourceBrushData& data)
{
    SourceBrush result { data.color };
    if (auto* gradient = std::get_if<SourceBrushLogicalGradient>(&data.patternGradient))
        result.setGradient(gradient->gradient.copyRef(), gradient->spaceTransform);
    return result;
}

static GraphicsContextState graphicsContextState(const SetStateData& data)
{
    GraphicsContextState state;
    state.setFillBrush(sourceBrush(data.fillBrush));
    state.setFillRule(data.fillRule);
    state.setStrokeBrush(sourceBrush(data.strokeBrush));
    state.setStrokeThickness(data.strokeThickness);
    state.setStrokeStyle(data.strokeStyle);
    state.setCompositeMode(data.compositeMode);
    state.setDropShadow(data.dropShadow);
    state.setStyle(data.style);
    state.setAlpha(data.alpha);
    state.setImageInterpolationQuality(data.imageInterpolationQuality);
    state.setTextDrawingMode(data.textDrawingMode);
    state.setShouldAntialias(data.shouldAntialias);
    state.setShouldSmoothFonts(data.shouldSmoothFonts);
    state.setShouldSubpixelQuantizeFonts(data.shouldSubpixelQuantizeFonts);
    state.setShadowsIgnoreTransforms(data.shadowsIgnoreTransforms);
    state.didApplyChanges();
    state.markChanged(data.changes);
    return state;
}

static std::optional<Vector<Ref<const FontBase>>> createFonts(const Vector<FontCustomPlatformSerializedData>& customFontData, const Vector<DisplayListDataFont>& fontData, bool shouldUseLockdownFontParser)
{
    Vector<Ref<FontCustomPlatformData>> customFonts;
    customFonts.reserveInitialCapacity(customFontData.size());
    for (auto data : customFontData) {
        auto customFont = FontCustomPlatformData::tryMakeFromSerializationData(WTF::move(data), shouldUseLockdownFontParser);
        if (!customFont)
            return std::nullopt;
        customFonts.append(WTF::move(*customFont));
    }

    Vector<Ref<const FontBase>> fonts;
    fonts.reserveInitialCapacity(fontData.size());
    for (auto& data : fontData) {
        RefPtr<FontCustomPlatformData> customFont;
        if (data.customFontIndex) {
            if (*data.customFontIndex >= customFonts.size())
                return std::nullopt;
            customFont = customFonts[*data.customFontIndex].ptr();
        }
        auto platformData = FontPlatformData::create(data.platformData, customFont.get());
        fonts.append(SharedFont::create(platformData, data.attributes.origin, data.attributes.isInterstitial, data.attributes.visibility, data.attributes.isTextOrientationFallback, data.attributes.renderingResourceIdentifier));
    }
    return fonts;
}

std::optional<DisplayListData> DisplayListData::create(const DisplayList::DisplayList& displayList)
{
    return DisplayListDataEncoder { }.encode(displayList);
}

DisplayListData::DisplayListData(Vector<FontCustomPlatformSerializedData>&& customFonts, Vector<DisplayListDataFont>&& fonts, Vector<Vector<DisplayListDataItem>>&& displayLists)
    : m_customFonts(WTF::move(customFonts))
    , m_fonts(WTF::move(fonts))
    , m_displayLists(WTF::move(displayLists))
{
}

RefPtr<const DisplayList::DisplayList> DisplayListData::createDisplayList(bool shouldUseLockdownFontParser) const
{
    auto fonts = createFonts(m_customFonts, m_fonts, shouldUseLockdownFontParser);
    if (!fonts)
        return nullptr;

    Vector<Ref<const DisplayList::DisplayList>> displayLists;
    displayLists.reserveInitialCapacity(m_displayLists.size());
    for (auto& items : m_displayLists) {
        Vector<DisplayList::Item> result;
        result.reserveInitialCapacity(items.size());
        for (auto& item : items) {
            bool isValid = WTF::switchOn(item,
                [&](const DrawDisplayListAtIndex& item) {
                    if (item.index >= displayLists.size())
                        return false;
                    result.constructAndAppend(WTF::InPlaceType<DisplayList::DrawDisplayList>, displayLists[item.index].copyRef());
                    return true;
                },
                [&](const DrawGlyphsWithFontAtIndex& item) {
                    if (item.fontIndex >= fonts->size() || item.glyphs.size() != item.advances.size())
                        return false;
                    auto advances = item.advances.map([](auto& advance) {
                        return makeGlyphBufferAdvance(advance);
                    });
                    result.constructAndAppend(WTF::InPlaceType<DisplayList::DrawGlyphs>, (*fonts)[item.fontIndex].copyRef(), Vector<GlyphBufferGlyph>(item.glyphs), WTF::move(advances), item.localAnchor, item.fontSmoothingMode);
                    return true;
                },
                [&](const DrawFilteredImageBuffer& item) {
                    if (hasFEImage(item.filter))
                        return false;
                    result.constructAndAppend(WTF::InPlaceType<DisplayList::DrawFilteredImageBuffer>, nullptr, item.sourceImageRect, item.filter.get());
                    return true;
                },
                [&](const SetStateData& item) {
                    result.constructAndAppend(WTF::InPlaceType<DisplayList::SetState>, graphicsContextState(item));
                    return true;
                },
                [&](const auto& item) {
                    result.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, item);
                    return true;
                });
            if (!isValid)
                return nullptr;
        }
        displayLists.append(DisplayList::DisplayList::create(WTF::move(result)));
    }
    if (displayLists.isEmpty())
        return nullptr;
    return displayLists.takeLast();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
