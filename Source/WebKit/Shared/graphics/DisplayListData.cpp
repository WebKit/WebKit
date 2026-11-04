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

namespace {

class DisplayListDataEncoder {
public:
    std::optional<DisplayListData> encode(const DisplayList::DisplayList& displayList)
    {
        if (!encodeDisplayList(displayList))
            return std::nullopt;
        return DisplayListData { WTF::move(m_customFonts), WTF::move(m_fonts), WTF::move(m_bottomUpDisplayLists) };
    }

private:
    std::optional<uint64_t> encodeDisplayList(const DisplayList::DisplayList& displayList)
    {
        if (auto iterator = m_indices.find(&displayList); iterator != m_indices.end())
            return iterator->value;

        // A placeholder can draw anything, including a display list that draws it.
        if (!m_displayListsBeingEncoded.add(Ref { displayList }).isNewEntry)
            return std::nullopt;

        Vector<DisplayListDataItem> items;
        bool success = appendItems(displayList.items(), items);
        m_displayListsBeingEncoded.remove(&displayList);
        if (!success)
            return std::nullopt;

        m_bottomUpDisplayLists.append(WTF::move(items));
        uint64_t index = m_bottomUpDisplayLists.size() - 1;
        m_indices.add(Ref { displayList }, index);
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
                result.append(DisplayList::DrawDisplayList { *index });
                return true;
            },
            [&](const DisplayList::DrawPlaceholder& item) {
                Ref recorder = DisplayList::RecorderImpl::create(FloatRect { });
                item.apply(recorder.get());
                Ref contents = recorder->takeDisplayList();
                return appendItems(contents->items(), result);
            },
            [&](const DisplayList::DrawGlyphs& item) {
                Ref font = item.font();
                auto glyphs = item;
                glyphs.setFontIndex(fontIndex(font));
                result.append(WTF::move(glyphs));
                return true;
            },
            [&](const DisplayList::DrawSystemImage& item) {
#if USE(SYSTEM_PREVIEW)
                if (item.systemImage().systemImageType() == SystemImageType::ARKitBadge)
                    return false;
#endif
                result.append(item);
                return true;
            },
            // FIXME: Serialize images, image buffers and patterns by value.
            [&](const DisplayList::SetState& item) {
                if (item.state().fillBrush().pattern() || item.state().strokeBrush().pattern())
                    return false;
                result.append(item);
                return true;
            },
            [&](const DisplayList::DrawFilteredImageBuffer& item) {
                if (item.sourceImage() || !item.filter().effectsOfType(FilterFunction::Type::FEImage).isEmpty())
                    return false;
                result.append(item);
                return true;
            },
            [](const DisplayList::ClipToImageBuffer&) { return false; },
            [](const DisplayList::DrawImageBuffer&) { return false; },
            [](const DisplayList::DrawMeshGradient&) { return false; },
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
        return m_fontIndices.ensure(Ref { font }, [&] {
            std::optional<uint64_t> customFontIndex;
            if (RefPtr customFont = font.platformData().customPlatformData()) {
                customFontIndex = m_customFontIndices.ensure(Ref { *customFont }, [&] {
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
    Vector<Vector<DisplayListDataItem>> m_bottomUpDisplayLists;
    HashMap<Ref<const FontCustomPlatformData>, uint64_t> m_customFontIndices;
    HashMap<Ref<const FontBase>, uint64_t> m_fontIndices;
    HashMap<Ref<const DisplayList::DisplayList>, uint64_t> m_indices;
    HashSet<Ref<const DisplayList::DisplayList>> m_displayListsBeingEncoded;
};

} // namespace

static std::optional<Vector<Ref<const FontBase>>> createFonts(const Vector<FontCustomPlatformSerializedData>& customFontData, const Vector<DisplayListDataFont>& fontData, bool shouldUseLockdownFontParser)
{
    Vector<Ref<FontCustomPlatformData>> customFonts(ReserveCapacityTag { }, customFontData.size());
    for (auto data : customFontData) {
        auto customFont = FontCustomPlatformData::tryMakeFromSerializationData(WTF::move(data), shouldUseLockdownFontParser);
        if (!customFont)
            return std::nullopt;
        customFonts.append(WTF::move(*customFont));
    }

    Vector<Ref<const FontBase>> fonts(ReserveCapacityTag { }, fontData.size());
    for (auto& data : fontData) {
        RefPtr<FontCustomPlatformData> customFont;
        if (data.customFontIndex) {
            if (*data.customFontIndex >= customFonts.size())
                return std::nullopt;
            customFont = customFonts[*data.customFontIndex].ptr();
        }
        auto platformData = FontPlatformData::create(data.platformData, customFont.get());
        fonts.append(SharedFont::create(platformData, data.attributes.origin, data.attributes.isInterstitial, data.attributes.visibility, data.attributes.isTextOrientationFallback, data.attributes.isSmallCapsFace, data.attributes.renderingResourceIdentifier));
    }
    return fonts;
}

std::optional<DisplayListData> DisplayListData::create(const DisplayList::DisplayList& displayList)
{
    return DisplayListDataEncoder { }.encode(displayList);
}

DisplayListData::DisplayListData(Vector<FontCustomPlatformSerializedData>&& customFonts, Vector<DisplayListDataFont>&& fonts, Vector<Vector<DisplayListDataItem>>&& bottomUpDisplayLists)
    : m_customFonts(WTF::move(customFonts))
    , m_fonts(WTF::move(fonts))
    , m_bottomUpDisplayLists(WTF::move(bottomUpDisplayLists))
{
}

RefPtr<const DisplayList::DisplayList> DisplayListData::createDisplayList(bool shouldUseLockdownFontParser) &&
{
    auto fonts = createFonts(m_customFonts, m_fonts, shouldUseLockdownFontParser);
    if (!fonts)
        return nullptr;

    Vector<Ref<const DisplayList::DisplayList>> displayLists(ReserveCapacityTag { }, m_bottomUpDisplayLists.size());
    for (auto& dataItems : m_bottomUpDisplayLists) {
        Vector<DisplayList::Item> items(ReserveCapacityTag { }, dataItems.size());
        for (auto& dataItem : dataItems) {
            if (auto* item = std::get_if<DisplayList::DrawFilteredImageBuffer>(&dataItem); item && !item->filter().effectsOfType(FilterFunction::Type::FEImage).isEmpty())
                return nullptr;
            WTF::switchOn(dataItem, [&](auto& item) {
                items.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, WTF::move(item));
            });
        }
        RefPtr displayList = DisplayList::DisplayList::createResolvingResources(WTF::move(items), fonts->span(), displayLists.span());
        if (!displayList)
            return nullptr;
        displayLists.append(displayList.releaseNonNull());
    }
    if (displayLists.isEmpty())
        return nullptr;
    return displayLists.takeLast();
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
