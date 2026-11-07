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
#include <WebCore/FEImage.h>
#include <WebCore/ImageBuffer.h>
#include <WebCore/NativeImage.h>
#include <WebCore/Pattern.h>
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
                auto state = item;
                if (!replacePatternImage(state.state().fillBrush()) || !replacePatternImage(state.state().strokeBrush()))
                    return false;
                result.append(WTF::move(state));
                return true;
            },
            [&](const DisplayList::DrawFilteredImageBuffer& item) {
                std::optional<uint64_t> sourceImageIndex;
                if (RefPtr sourceImage = item.sourceImage()) {
                    sourceImageIndex = imageIndex(*sourceImage);
                    if (!sourceImageIndex)
                        return false;
                }
                Vector<uint64_t> effectImageIndices;
                for (auto& effect : item.filter().effectsOfType(FilterFunction::Type::FEImage)) {
                    auto index = imageIndex(downcast<FEImage>(effect).sourceImage());
                    if (!index)
                        return false;
                    effectImageIndices.append(*index);
                }
                result.append(DisplayList::DrawFilteredImageBuffer { sourceImageIndex, item.sourceImageRect(), Ref { item.filter() }, WTF::move(effectImageIndices) });
                return true;
            },
            [&](const DisplayList::ClipToImageBuffer& item) {
                auto index = imageIndex(item.imageBuffer());
                if (!index)
                    return false;
                result.append(DisplayList::ClipToImageBuffer { *index, item.destinationRect() });
                return true;
            },
            [&](const DisplayList::DrawImageBuffer& item) {
                auto index = imageIndex(item.imageBuffer());
                if (!index)
                    return false;
                result.append(DisplayList::DrawImageBuffer { *index, item.destinationRect(), item.source(), item.options() });
                return true;
            },
            [&](const DisplayList::DrawNativeImage& item) {
                auto index = imageIndex(item.nativeImage());
                if (!index)
                    return false;
                result.append(DisplayList::DrawNativeImage { *index, item.destinationRect(), item.source(), item.options() });
                return true;
            },
            [&](const DisplayList::DrawPatternNativeImage& item) {
                auto index = imageIndex(item.nativeImage());
                if (!index)
                    return false;
                result.append(DisplayList::DrawPatternNativeImage { *index, item.destRect(), item.tileRect(), item.patternTransform(), item.phase(), item.spacing(), item.options() });
                return true;
            },
            [&](const DisplayList::DrawPatternImageBuffer& item) {
                auto index = imageIndex(item.imageBuffer());
                if (!index)
                    return false;
                result.append(DisplayList::DrawPatternImageBuffer { *index, item.destRect(), item.tileRect(), item.patternTransform(), item.phase(), item.spacing(), item.options() });
                return true;
            },
#if USE(SKIA)
            [](const DisplayList::DrawTextBlob&) { return false; },
#endif
            [&](const auto& item) {
                result.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, item);
                return true;
            });
    }

    bool replacePatternImage(SourceBrush& brush)
    {
        RefPtr pattern = brush.pattern();
        if (!pattern)
            return true;
        auto index = imageIndex(pattern->tileImage());
        if (!index)
            return false;
        brush.setPattern(WebCore::Pattern::create(SourceImage { SourceImage::ImageReference { *index } }, pattern->parameters()));
        return true;
    }

    std::optional<uint64_t> imageIndex(const SourceImage& image)
    {
        if (RefPtr nativeImage = image.nativeImageIfExists())
            return imageIndex(*nativeImage);
        if (RefPtr imageBuffer = image.imageBufferIfExists())
            return imageIndex(*imageBuffer);
        return std::nullopt;
    }

    std::optional<uint64_t> imageIndex(const NativeImage& image)
    {
        if (auto iterator = m_nativeImageIndices.find(&image); iterator != m_nativeImageIndices.end())
            return iterator->value;
        auto index = appendImage(image, std::nullopt);
        if (index)
            m_nativeImageIndices.add(Ref { image }, *index);
        return index;
    }

    std::optional<uint64_t> imageIndex(ImageBuffer& imageBuffer)
    {
        if (auto iterator = m_imageBufferIndices.find(&imageBuffer); iterator != m_imageBufferIndices.end())
            return iterator->value;
        RefPtr image = imageBuffer.copyNativeImage();
        if (!image)
            return std::nullopt;
        auto index = appendImage(*image, imageBuffer.parameters());
        if (index)
            m_imageBufferIndices.add(Ref { imageBuffer }, *index);
        return index;
    }

    std::optional<uint64_t> appendImage(const NativeImage& image, std::optional<ImageBufferParameters>&& imageBufferParameters)
    {
        RefPtr<ShareableBitmap> bitmap;
#if USE(CG)
        bitmap = ShareableBitmap::createFromImagePixels(image);
#endif
        if (!bitmap)
            bitmap = image.toShareableBitmap();
        if (!bitmap)
            return std::nullopt;
        auto handle = bitmap->createReadOnlyHandle();
        if (!handle)
            return std::nullopt;
        m_images.append(DisplayListDataImage { WTF::move(*handle), WTF::move(imageBufferParameters) });
        return m_images.size() - 1;
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
    HashMap<Ref<const NativeImage>, uint64_t> m_nativeImageIndices;
    HashMap<Ref<ImageBuffer>, uint64_t> m_imageBufferIndices;
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

static std::optional<Vector<SourceImage>> createImages(Vector<DisplayListDataImage>&& imageData)
{
    Vector<SourceImage> images(ReserveCapacityTag { }, imageData.size());
    for (auto& data : imageData) {
        RefPtr bitmap = ShareableBitmap::create(WTF::move(data.bitmap), SharedMemory::Protection::ReadOnly);
        if (!bitmap)
            return std::nullopt;
        RefPtr nativeImage = NativeImage::create(bitmap->createPlatformImage(DontCopyBackingStore, ShouldInterpolate::Yes));
        if (!nativeImage)
            return std::nullopt;
        if (!data.imageBufferParameters) {
            images.append(SourceImage { nativeImage.releaseNonNull() });
            continue;
        }

        auto& parameters = *data.imageBufferParameters;
        if (nativeImage->size() != parameters.backendSize())
            return std::nullopt;
        RefPtr imageBuffer = ImageBuffer::create(parameters.logicalSize, RenderingMode::Unaccelerated, parameters.purpose, parameters.resolutionScale, parameters.colorSpace, parameters.bufferFormat);
        if (!imageBuffer)
            return std::nullopt;
        imageBuffer->context().drawNativeImage(*nativeImage, FloatRect { { }, parameters.logicalSize }, FloatRect { { }, nativeImage->size() }, { CompositeOperator::Copy });
        images.append(SourceImage { imageBuffer.releaseNonNull() });
    }
    return images;
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

RefPtr<const DisplayList::DisplayList> DisplayListData::createDisplayList(bool shouldUseLockdownFontParser) &&
{
    auto fonts = createFonts(m_customFonts, m_fonts, shouldUseLockdownFontParser);
    if (!fonts)
        return nullptr;

    auto images = createImages(WTF::move(m_images));
    if (!images)
        return nullptr;

    Vector<Ref<const DisplayList::DisplayList>> displayLists(ReserveCapacityTag { }, m_bottomUpDisplayLists.size());
    for (auto& dataItems : m_bottomUpDisplayLists) {
        Vector<DisplayList::Item> items(ReserveCapacityTag { }, dataItems.size());
        for (auto& dataItem : dataItems) {
            WTF::switchOn(dataItem, [&](auto& item) {
                items.constructAndAppend(WTF::InPlaceType<std::decay_t<decltype(item)>>, WTF::move(item));
            });
        }
        RefPtr displayList = DisplayList::DisplayList::createResolvingResources(WTF::move(items), fonts->span(), images->span(), displayLists.span());
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
