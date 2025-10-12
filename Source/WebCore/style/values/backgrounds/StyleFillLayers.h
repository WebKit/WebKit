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
#include <WebCore/StyleImage.h>
#include <WebCore/StyleValueTypes.h>
#include <wtf/RefCountedFixedVector.h>

namespace WebCore {

class RenderElement;

namespace Style {

template<typename T> struct FillLayers {
    using Layer = T;
    using Container = RefCountedFixedVector<Layer>;
    using iterator = typename Container::iterator;
    using const_iterator = typename Container::const_iterator;
    using reverse_iterator = typename Container::reverse_iterator;
    using const_reverse_iterator = typename Container::const_reverse_iterator;

    FillLayers()
        : FillLayers { Layer { CSS::Keyword::None { } } }
    {
    }

    FillLayers(Layer&& layer)
        : m_layers { Container::create({ WTFMove(layer) }) }
    {
    }

    FillLayers(Ref<Container>&& layers)
        : m_layers { WTFMove(layers) }
    {
        RELEASE_ASSERT(!m_layers->isEmpty());
    }

    iterator begin() LIFETIME_BOUND { return m_layers->begin(); }
    iterator end() LIFETIME_BOUND { return m_layers->end(); }
    const_iterator begin() const LIFETIME_BOUND { return m_layers->begin(); }
    const_iterator end() const LIFETIME_BOUND { return m_layers->end(); }

    reverse_iterator rbegin() LIFETIME_BOUND { return m_layers->rbegin(); }
    reverse_iterator rend() LIFETIME_BOUND { return m_layers->rend(); }
    const_reverse_iterator rbegin() const LIFETIME_BOUND { return m_layers->rbegin(); }
    const_reverse_iterator rend() const LIFETIME_BOUND { return m_layers->rend(); }

    // Unlike most containers, `first()` and `last()` are always valid because the minimum number of elements
    // in Layers is 1.

    T& first() LIFETIME_BOUND { return m_layers->first(); }
    const T& first() const LIFETIME_BOUND { return m_layers->first(); }

    T& last() LIFETIME_BOUND { return m_layers->last(); }
    const T& last() const LIFETIME_BOUND { return m_layers->last(); }

    T& operator[](size_t i) LIFETIME_BOUND { return m_layers.get()[i]; }
    const T& operator[](size_t i) const LIFETIME_BOUND { return m_layers.get()[i]; }

    unsigned size() const { return m_layers->size(); }
    bool isEmpty() const { return m_layers->isEmpty(); }

    FillLayers& access() LIFETIME_BOUND
    {
        if (!m_layers->hasOneRef())
            m_layers = m_layers->clone();
        return *this;
    }

    void computeClipMax() const;

    bool imagesAreLoaded(const RenderElement*) const;
    bool hasImage() const { return hasImageInAnyLayer(); }
    bool hasImageInAnyLayer() const;
    bool hasImageWithAttachment(FillAttachment) const;
    bool hasHDRContent() const;
    bool hasEntirelyFixedBackground() const;
    bool hasAnyBackgroundClipText() const;
    RefPtr<StyleImage> findLayerUsedImage(WrappedImagePtr, bool& isNonEmpty) const;

    bool operator==(const FillLayers& other) const
    {
        return arePointingToEqualData(m_layers, other.m_layers);
    }

private:
    Ref<Container> m_layers;
};

template<typename Layer>
void FillLayers<Layer>::computeClipMax() const
{
    auto computedClipMax = FillBox::NoClip;
    for (auto& layer : makeReversedRange(*this)) {
        computedClipMax = clipMax(computedClipMax, layer.clip());
        layer.setClipMax(computedClipMax);
    }
}

template<typename Layer>
bool FillLayers<Layer>::imagesAreLoaded(const RenderElement* renderer) const
{
    return std::ranges::all_of(*this, [&renderer](auto& layer) {
        RefPtr image = layer.image().tryStyleImage();
        return !image || image->isLoaded(renderer);
    });
}

template<typename Layer>
bool FillLayers<Layer>::hasImageInAnyLayer() const
{
    return std::ranges::any_of(*this, [](auto& layer) {
        return layer.hasImage();
    });
}

template<typename Layer>
bool FillLayers<Layer>::hasImageWithAttachment(FillAttachment attachment) const
{
    return std::ranges::any_of(*this, [&attachment](auto& layer) {
        return layer.hasImage() && layer.attachment() == attachment;
    });
}

template<typename Layer>
bool FillLayers<Layer>::hasHDRContent() const
{
    return std::ranges::any_of(*this, [](auto& layer) {
        RefPtr image = layer.image().tryStyleImage();
        if (auto* cachedImage = image ? image->cachedImage() : nullptr) {
            if (cachedImage->hasHDRContent())
                return true;
        }
        return false;
    });
}

template<typename Layer>
bool FillLayers<Layer>::hasEntirelyFixedBackground() const
{
    return std::ranges::all_of(*this, [](auto& layer) {
        return layer.hasImage() && layer.attachment() == FillAttachment::FixedBackground;
    });
}

template<typename Layer>
bool FillLayers<Layer>::hasAnyBackgroundClipText() const
{
    return std::ranges::any_of(*this, [](auto& layer) {
        return layer.clip() == FillBox::Text;
    });
}

template<typename Layer>
RefPtr<StyleImage> FillLayers<Layer>::findLayerUsedImage(WrappedImagePtr image, bool& isNonEmpty) const
{
    for (auto& layer : *this) {
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

template<typename Layer>
TextStream& operator<<(TextStream& ts, const FillLayers<Layer>& value)
{
    logForCSSOnRangeLike(ts, value, ", "_s);
    return ts;
}

} // namespace Style
} // namespace WebCore
