/*
 * Copyright (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2008, 2014 Apple Inc. All rights reserved.
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

#include "config.h"
#include "StyleBackgroundLayer.h"

#include "CachedImage.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

BackgroundLayer::BackgroundLayer()
    : m_image(BackgroundLayer::initialImage())
    , m_positionX(BackgroundLayer::initialPositionX())
    , m_positionY(BackgroundLayer::initialPositionY())
    , m_size(BackgroundLayer::initialSize())
    , m_repeat(BackgroundLayer::initialRepeat())
    , m_attachment(static_cast<unsigned>(BackgroundLayer::initialAttachment()))
    , m_clip(static_cast<unsigned>(BackgroundLayer::initialClip()))
    , m_origin(static_cast<unsigned>(BackgroundLayer::initialOrigin()))
    , m_blendMode(static_cast<unsigned>(BackgroundLayer::initialBlendMode()))
    , m_clipMax(static_cast<unsigned>(BackgroundLayer::initialClip()))
{
}

BackgroundLayer::BackgroundLayer(ImageOrNone&& image)
    : BackgroundLayer { }
{
    setImage(WTF::move(image));
}

BackgroundLayer::BackgroundLayer(CSS::Keyword::None keyword)
    : BackgroundLayer { ImageOrNone { keyword } }
{
}

BackgroundLayer::BackgroundLayer(RefPtr<Image>&& image)
    : BackgroundLayer { ImageOrNone { WTF::move(image) } }
{
}

bool BackgroundLayer::operator==(const BackgroundLayer& other) const
{
    // NOTE: Default operator== is not used due to exclusion of m_clipMax.

    return allOfCoordinatedValueListProperties<BackgroundLayer>([this, &other]<auto propertyID>() {
        using PropertyAccessor = CoordinatedValueListPropertyConstAccessor<propertyID>;
        return PropertyAccessor { *this } == PropertyAccessor { other };
    });
}

bool BackgroundLayer::hasOpaqueImage(const RenderElement& renderer) const
{
    RefPtr image = m_image.tryStyleImage();
    if (!image)
        return false;

    if (composite() == CompositeOperator::Clear
        || composite() == CompositeOperator::Copy)
        return true;

    return blendMode() == BlendMode::Normal
        && composite() == CompositeOperator::SourceOver
        && image->knownToBeOpaque(renderer);
}

// MARK: - Blending

auto Blending<BackgroundLayer>::canBlend(const BackgroundLayer& a, const BackgroundLayer& b) -> bool
{
    return a.size().hasSameType(b.size());
}

// MARK: - Logging

TextStream& operator<<(TextStream& ts, const BackgroundLayer& layer)
{
    TextStream::GroupScope scope(ts);

    ts << "background-layer"_s;
    ts.dumpProperty("image"_s, layer.image());
    ts.dumpProperty("position"_s, layer.position());
    ts.dumpProperty("size"_s, layer.size());
    ts.dumpProperty("repeat"_s, layer.repeat());
    ts.dumpProperty("clip"_s, layer.clip());
    ts.dumpProperty("origin"_s, layer.origin());
    ts.dumpProperty("blend-mode"_s, layer.blendMode());

    return ts;
}

} // namespace Style
} // namespace WebCore
