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
#include <WebCore/DisplayListResourceCache.h>
#include <WebCore/FEImage.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/Pattern.h>
#include <wtf/Borrow.h>
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
        return DisplayListData { WTF::move(m_customFonts), WTF::move(m_fonts), WTF::move(m_images), WTF::move(m_bottomUpDisplayLists) };
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
            [&](const DisplayList::SetState& item) {
                if (!addPatternImage(item.state().fillBrush()) || !addPatternImage(item.state().strokeBrush()))
                    return false;
                result.append(item);
                return true;
            },
            [&](const DisplayList::DrawFilteredImageBuffer& item) {
                if (RefPtr sourceImage = item.sourceImage(); sourceImage && !addImage(*sourceImage))
                    return false;
                Ref filter = item.filter();
                for (auto& effect : filter->effectsOfType(FilterFunction::Type::FEImage)) {
                    Ref feImage = downcast<FEImage>(effect);
                    if (!addImage(feImage->sourceImage()))
                        return false;
                }
                result.append(item);
                return true;
            },
            [&](const DisplayList::ClipToNativeImage& item) {
                Ref image = item.nativeImage();
                return appendItemWithImage(item, image.get(), result);
            },
            [&](const DisplayList::DrawImageBuffer& item) {
                Ref imageBuffer = item.imageBuffer();
                return appendItemWithImage(item, imageBuffer.get(), result);
            },
            [&](const DisplayList::DrawNativeImage& item) {
                Ref image = item.nativeImage();
                return appendItemWithImage(item, image.get(), result);
            },
            [&](const DisplayList::DrawPatternNativeImage& item) {
                Ref image = item.nativeImage();
                return appendItemWithImage(item, image.get(), result);
            },
            [&](const DisplayList::DrawPatternImageBuffer& item) {
                Ref imageBuffer = item.imageBuffer();
                return appendItemWithImage(item, imageBuffer.get(), result);
            },
#if USE(SKIA)
            [](const DisplayList::DrawTextBlob&) { return false; },
#endif
            [&](const auto& item) {
                result.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, item);
                return true;
            });
    }

    bool appendItemWithImage(const auto& item, auto& image, Vector<DisplayListDataItem>& result)
    {
        if (!addImage(image))
            return false;
        result.append(item);
        return true;
    }

    bool addPatternImage(const SourceBrush& brush)
    {
        RefPtr pattern = brush.pattern();
        return !pattern || addImage(pattern->tileImage());
    }

    bool addImage(const SourceImage& image)
    {
        if (RefPtr nativeImage = image.nativeImageIfExists())
            return addImage(*nativeImage);
        if (RefPtr imageBuffer = image.imageBufferIfExists())
            return addImage(*imageBuffer);
        return false;
    }

    bool addImage(const NativeImage& image)
    {
        auto identifier = image.renderingResourceIdentifier();
        if (m_imageIdentifiers.contains(identifier))
            return true;
        return appendImage(identifier, image, std::nullopt);
    }

    bool addImage(ImageBuffer& imageBuffer)
    {
        auto identifier = imageBuffer.renderingResourceIdentifier();
        if (m_imageIdentifiers.contains(identifier))
            return true;
        RefPtr image = imageBuffer.copyNativeImage();
        if (!image)
            return false;
        return appendImage(identifier, *image, imageBuffer.parameters());
    }

    bool appendImage(RenderingResourceIdentifier identifier, const NativeImage& image, std::optional<ImageBufferParameters>&& imageBufferParameters)
    {
        RefPtr<ShareableBitmap> bitmap;
#if USE(CG)
        bitmap = ShareableBitmap::createFromImagePixels(image);
#endif
        if (!bitmap)
            bitmap = image.toShareableBitmap();
        if (!bitmap)
            return false;
        auto handle = bitmap->createReadOnlyHandle();
        if (!handle)
            return false;
        m_images.append(DisplayListDataImage { identifier, WTF::move(*handle), WTF::move(imageBufferParameters) });
        m_imageIdentifiers.add(identifier);
        return true;
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
    Vector<DisplayListDataImage> m_images;
    Vector<Vector<DisplayListDataItem>> m_bottomUpDisplayLists;
    HashMap<Ref<const FontCustomPlatformData>, uint64_t> m_customFontIndices;
    HashMap<Ref<const FontBase>, uint64_t> m_fontIndices;
    HashSet<RenderingResourceIdentifier> m_imageIdentifiers;
    HashMap<Ref<const DisplayList::DisplayList>, uint64_t> m_indices;
    HashSet<Ref<const DisplayList::DisplayList>> m_displayListsBeingEncoded;
};

} // namespace

static bool cacheFonts(DisplayList::ResourceCache& resourceCache, const Vector<FontCustomPlatformSerializedData>& customFontData, const Vector<DisplayListDataFont>& fontData, bool shouldUseLockdownFontParser)
{
    Vector<Ref<FontCustomPlatformData>> customFonts(ReserveCapacityTag { }, customFontData.size());
    for (auto data : customFontData) {
        auto customFont = FontCustomPlatformData::tryMakeFromSerializationData(WTF::move(data), shouldUseLockdownFontParser);
        if (!customFont)
            return false;
        customFonts.append(WTF::move(*customFont));
    }

    for (auto& data : borrow(fontData)) {
        RefPtr<FontCustomPlatformData> customFont;
        if (data.customFontIndex) {
            if (*data.customFontIndex >= customFonts.size())
                return false;
            customFont = customFonts[*data.customFontIndex].ptr();
        }
        auto platformData = FontPlatformData::create(data.platformData, customFont.get());
        resourceCache.appendFont(SharedFont::create(platformData, data.attributes.origin, data.attributes.isInterstitial, data.attributes.visibility, data.attributes.isTextOrientationFallback, data.attributes.isSmallCapsFace, data.attributes.renderingResourceIdentifier));
    }
    return true;
}

static bool cacheImages(DisplayList::ResourceCache& resourceCache, Vector<DisplayListDataImage>&& imageData)
{
    for (auto& data : borrow(imageData)) {
        RefPtr bitmap = ShareableBitmap::create(WTF::move(data.bitmap), SharedMemory::Protection::ReadOnly);
        if (!bitmap)
            return false;
        RefPtr nativeImage = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes));
        if (!nativeImage)
            return false;
        if (!data.imageBufferParameters) {
            if (!resourceCache.cacheNativeImage(data.identifier, nativeImage.releaseNonNull()))
                return false;
            continue;
        }

        auto& parameters = *data.imageBufferParameters;
        if (nativeImage->size() != parameters.backendSize())
            return false;
        RefPtr imageBuffer = ImageBuffer::create(parameters.logicalSize, RenderingMode::Unaccelerated, parameters.purpose, parameters.resolutionScale, parameters.colorSpace, parameters.bufferFormat);
        if (!imageBuffer)
            return false;
        imageBuffer->context().drawNativeImage(*nativeImage, FloatRect { { }, parameters.logicalSize }, FloatRect { { }, nativeImage->size() }, { CompositeOperator::Copy });
        if (!resourceCache.cacheImageBuffer(data.identifier, imageBuffer.releaseNonNull()))
            return false;
    }
    return true;
}

std::optional<DisplayListData> DisplayListData::create(const DisplayList::DisplayList& displayList)
{
    return DisplayListDataEncoder { }.encode(displayList);
}

DisplayListData::DisplayListData(Vector<FontCustomPlatformSerializedData>&& customFonts, Vector<DisplayListDataFont>&& fonts, Vector<DisplayListDataImage>&& images, Vector<Vector<DisplayListDataItem>>&& bottomUpDisplayLists)
    : m_customFonts(WTF::move(customFonts))
    , m_fonts(WTF::move(fonts))
    , m_images(WTF::move(images))
    , m_bottomUpDisplayLists(WTF::move(bottomUpDisplayLists))
{
}

void DisplayListData::setOwnershipOfMemory(const ProcessIdentity& processIdentity) const
{
    for (auto& image : borrow(m_images))
        image.bitmap.setOwnershipOfMemory(processIdentity, MemoryLedger::Graphics);
}

RefPtr<const DisplayList::DisplayList> DisplayListData::createDisplayList(bool shouldUseLockdownFontParser) &&
{
    DisplayList::ResourceCache resourceCache;
    if (!cacheFonts(resourceCache, m_customFonts, m_fonts, shouldUseLockdownFontParser))
        return nullptr;

    if (!cacheImages(resourceCache, WTF::move(m_images)))
        return nullptr;

    RefPtr<const DisplayList::DisplayList> displayList;
    for (auto& dataItems : borrow(m_bottomUpDisplayLists)) {
        Vector<DisplayList::Item> items(ReserveCapacityTag { }, dataItems.size());
        for (auto& dataItem : borrow(dataItems)) {
            WTF::switchOn(dataItem, [&](auto& item) {
                items.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, WTF::move(item));
            });
        }
        displayList = DisplayList::DisplayList::createResolved(WTF::move(items), resourceCache);
        if (!displayList)
            return nullptr;
        resourceCache.appendDisplayList(*displayList);
    }
    return displayList;
}

} // namespace WebKit

#endif // ENABLE(GPU_PROCESS)
