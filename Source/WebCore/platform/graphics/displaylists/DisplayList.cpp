/*
 * Copyright (C) 2016-2023 Apple Inc. All rights reserved.
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
#include "DisplayList.h"

#include "Filter.h"
#include "Font.h"
#include "ImageBuffer.h"
#include "Logging.h"
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace DisplayList {

WTF_MAKE_TZONE_ALLOCATED_IMPL(DisplayList);

DisplayList::DisplayList(Vector<Item>&& items)
    : m_items(WTF::move(items))
{
    ASSERT(std::ranges::all_of(m_items, [](auto& item) {
        return isResolved(item);
    }));
}

RefPtr<const DisplayList> DisplayList::createResolvingResources(Vector<Item>&& items, std::span<const Ref<const FontBase>> fonts, std::span<const SourceImage> images, std::span<const Ref<const DisplayList>> displayLists)
{
    auto sourceImage = [&](uint64_t index) -> std::optional<SourceImage> {
        if (index >= images.size() || !images[index].isResolved())
            return std::nullopt;
        return images[index];
    };
    auto resolveNativeImage = [&](auto& item) {
        if (item.isResolved())
            return true;
        if (item.imageIndex() >= images.size())
            return false;
        RefPtr nativeImage = images[item.imageIndex()].nativeImageIfExists();
        if (!nativeImage)
            return false;
        item.setNativeImage(nativeImage.releaseNonNull());
        return true;
    };
    auto resolveImageBuffer = [&](auto& item) {
        if (item.isResolved())
            return true;
        if (item.imageIndex() >= images.size())
            return false;
        RefPtr imageBuffer = images[item.imageIndex()].imageBufferIfExists();
        if (!imageBuffer)
            return false;
        item.setImageBuffer(imageBuffer.releaseNonNull());
        return true;
    };
    auto resolvePattern = [&](const SourceBrush& brush) {
        RefPtr pattern = brush.pattern();
        if (!pattern)
            return true;
        auto index = pattern->tileImage().imageIndex();
        if (!index)
            return true;
        auto tileImage = sourceImage(*index);
        if (!tileImage)
            return false;
        pattern->setTileImage(WTF::move(*tileImage));
        return true;
    };

    for (auto& item : items) {
        bool isValid = WTF::switchOn(item,
            [&](ClipToImageBuffer& item) {
                return resolveImageBuffer(item);
            },
            [&](DrawDisplayList& item) {
                if (item.isResolved())
                    return true;
                if (item.displayListIndex() >= displayLists.size())
                    return false;
                item.setDisplayList(displayLists[item.displayListIndex()].copyRef());
                return true;
            },
            [&](DrawFilteredImageBuffer& item) {
                if (item.isResolved())
                    return true;
                if (auto index = item.sourceImageIndex()) {
                    if (*index >= images.size())
                        return false;
                    RefPtr imageBuffer = images[*index].imageBufferIfExists();
                    if (!imageBuffer)
                        return false;
                    item.setSourceImage(imageBuffer.releaseNonNull());
                }
                if (item.effectImageIndices().size() != item.filter().effectsOfType(FilterFunction::Type::FEImage).size())
                    return false;
                Vector<SourceImage> effectImages(ReserveCapacityTag { }, item.effectImageIndices().size());
                for (auto index : item.effectImageIndices()) {
                    auto image = sourceImage(index);
                    if (!image)
                        return false;
                    effectImages.append(WTF::move(*image));
                }
                item.setEffectImages(WTF::move(effectImages));
                return true;
            },
            [&](DrawGlyphs& item) {
                if (item.glyphs().size() != item.advances().size())
                    return false;
                if (item.isResolved())
                    return true;
                if (item.fontIndex() >= fonts.size())
                    return false;
                item.setFont(fonts[item.fontIndex()].copyRef());
                return true;
            },
            [&](DrawImageBuffer& item) {
                return resolveImageBuffer(item);
            },
            [&](DrawNativeImage& item) {
                return resolveNativeImage(item);
            },
            [&](DrawPatternImageBuffer& item) {
                return resolveImageBuffer(item);
            },
            [&](DrawPatternNativeImage& item) {
                return resolveNativeImage(item);
            },
            [&](SetState& item) {
                return resolvePattern(item.state().fillBrush()) && resolvePattern(item.state().strokeBrush());
            },
            [](auto&) {
                return true;
            });
        if (!isValid || !isResolved(item))
            return nullptr;
    }
    return create(WTF::move(items));
}

DisplayList::~DisplayList()
{
    for (CheckedRef observer : m_observers)
        observer->willDestroyDisplayList(*this);
}

String DisplayList::asText(OptionSet<AsTextFlag> flags) const
{
    TextStream stream(TextStream::LineMode::MultipleLine, TextStream::Formatting::SVGStyleRect);
    if (flags.contains(AsTextFlag::IncludeResourceIdentifiers))
        stream.dumpProperty("display-list-identifier"_s, this);

    for (const auto& item : m_items) {
        if (!shouldDumpItem(item, flags))
            continue;

        TextStream::GroupScope group(stream);
        dumpItem(stream, item, flags);
    }
    return stream.release();
}

void DisplayList::dump(TextStream& ts) const
{
    TextStream::GroupScope group(ts);
    ts << "display list"_s;

    for (const auto& item : m_items) {
        TextStream::GroupScope group(ts);
        dumpItem(ts, item, { AsTextFlag::IncludePlatformOperations, AsTextFlag::IncludeResourceIdentifiers });
    }
}

TextStream& operator<<(TextStream& ts, const DisplayList& displayList)
{
    displayList.dump(ts);
    return ts;
}

} // namespace DisplayList
} // namespace WebCore
