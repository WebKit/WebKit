/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2005, 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Graham Dennis (graham.dennis@gmail.com)
 * Copyright (C) 2025 Samuel Weinig <sam@webkit.org>
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

#pragma once

#include <WebCore/GraphicsTypes.h>
#include <WebCore/RenderStyleConstants.h>
#include <WebCore/StyleBackgroundSize.h>
#include <WebCore/StyleCoordinatedValueList.h>
#include <WebCore/StyleImageOrNone.h>
#include <WebCore/StylePosition.h>
#include <WebCore/StyleRepeatStyle.h>
#include <ranges>

namespace WebCore {

class RenderElement;

namespace Style {

// Utilities for working with the BackgroundLayers and MaskLayers types.

template<typename T> concept FillLayer = CoordinatedValueListValue<T> && requires(T value) {
    { value.image() } -> std::same_as<const ImageOrNone&>;
    { value.hasImage() } -> std::same_as<bool>;
    { value.size() } -> std::same_as<const BackgroundSize&>;
    { value.attachment() } -> std::same_as<FillAttachment>;
    { value.clip() } -> std::same_as<FillBox>;
    { value.setClipMax(std::declval<FillBox>()) };
};

template<FillLayer T>
void computeClipMax(const CoordinatedValueList<T>& list)
{
    auto computedClipMax = FillBox::NoClip;
    for (auto& layer : list.usedValues() | std::views::reverse) {
        computedClipMax = clipMax(computedClipMax, layer.clip());
        layer.setClipMax(computedClipMax);
    }
}

template<FillLayer T>
bool imagesAreLoaded(const CoordinatedValueList<T>& list, const RenderElement& renderer)
{
    return std::ranges::all_of(list.usedValues(), [&renderer](auto& layer) {
        RefPtr image = layer.image().tryStyleImage();
        return !image || image->isLoaded(&renderer);
    });
}

template<FillLayer T>
bool hasImageInAnyLayer(const CoordinatedValueList<T>& list)
{
    return std::ranges::any_of(list.usedValues(), [](auto& layer) {
        return layer.hasImage();
    });
}

template<FillLayer T>
bool hasImageWithAttachment(const CoordinatedValueList<T>& list, FillAttachment attachment)
{
    return std::ranges::any_of(list.usedValues(), [&attachment](auto& layer) {
        return layer.hasImage() && layer.attachment() == attachment;
    });
}

template<FillLayer T>
bool hasHDRContent(const CoordinatedValueList<T>& list)
{
    return std::ranges::any_of(list.usedValues(), [](auto& layer) {
        RefPtr image = layer.image().tryStyleImage();
        if (auto* cachedImage = image ? image->cachedImage() : nullptr) {
            if (cachedImage->hasHDRContent())
                return true;
        }
        return false;
    });
}

template<FillLayer T>
bool hasEntirelyFixedBackground(const CoordinatedValueList<T>& list)
{
    return std::ranges::all_of(list.usedValues(), [](auto& layer) {
        return layer.hasImage() && layer.attachment() == FillAttachment::FixedBackground;
    });
}

template<FillLayer T>
bool hasAnyBackgroundClipText(const CoordinatedValueList<T>& list)
{
    return std::ranges::any_of(list.usedValues(), [](auto& layer) {
        return layer.clip() == FillBox::Text;
    });
}

template<FillLayer T>
RefPtr<Image> findLayerUsedImage(const CoordinatedValueList<T>& list, WrappedImagePtr image, bool& isNonEmpty)
{
    for (auto& layer : list.usedValues()) {
        RefPtr layerImage = layer.image().tryStyleImage();
        if (!layerImage || layerImage->data() != image)
            continue;

        // FIXME: This really needs to compute the tile rect with BackgroundPainter::calculateFillTileSize().
        isNonEmpty = WTF::switchOn(layer.size(),
            [&](const CSS::Keyword::Cover&) {
                return false;
            },
            [&](const CSS::Keyword::Contain&) {
                return false;
            },
            [&](const Style::BackgroundSize::LengthSize& size) {
                auto isAutoOrKnownNonZero = [](auto& length) {
                    return WTF::switchOn(length,
                        [](const Style::BackgroundSizeLength::Fixed& fixed) {
                            return !fixed.isZero();
                        },
                        [](const Style::BackgroundSizeLength::Percentage& percentage) {
                            return !percentage.isZero();
                        },
                        [](const Style::BackgroundSizeLength::Calc&) {
                            return false;
                        },
                        [](const CSS::Keyword::Auto&) {
                            return true;
                        }
                    );
                };
                return isAutoOrKnownNonZero(size.width()) && isAutoOrKnownNonZero(size.height());
            }
        );
        return layerImage;
    }

    isNonEmpty = false;
    return nullptr;
}

} // namespace Style
} // namespace WebCore
