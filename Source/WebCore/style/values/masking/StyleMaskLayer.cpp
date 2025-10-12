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
#include "StyleMaskLayer.h"

#include "CachedImage.h"
#include "StylePrimitiveKeyword+Logging.h"
#include "StylePrimitiveNumericTypes+Logging.h"
#include <wtf/PointerComparison.h>
#include <wtf/text/TextStream.h>

namespace WebCore {
namespace Style {

MaskLayer::MaskLayer(ImageOrNone&& image)
    : m_image(WTFMove(image))
    , m_position({ MaskLayer::initialFillXPosition(), MaskLayer::initialFillYPosition() })
    , m_size(MaskLayer::initialFillSize())
    , m_repeat(MaskLayer::initialFillRepeat())
    , m_clip(static_cast<unsigned>(MaskLayer::initialFillClip()))
    , m_origin(static_cast<unsigned>(MaskLayer::initialFillOrigin()))
    , m_composite(static_cast<unsigned>(MaskLayer::initialFillComposite()))
    , m_maskMode(static_cast<unsigned>(MaskLayer::initialFillMaskMode()))
{
}

MaskLayer::MaskLayer(CSS::Keyword::None)
    : MaskLayer { ImageOrNone { CSS::Keyword::None { } } }
{
}

MaskLayer::MaskLayer(RefPtr<StyleImage>&& image)
    : MaskLayer { ImageOrNone { WTFMove(image) } }
{
}

bool MaskLayer::operator==(const MaskLayer& other) const
{
    // NOTE: Default operator== is not used due to exclusion of m_clipMax.

    return m_image == other.m_image
        && m_position == other.m_position
        && m_clip == other.m_clip
        && m_composite == other.m_composite
        && m_origin == other.m_origin
        && m_repeat == other.m_repeat
        && m_size == other.m_size
        && m_maskMode == other.m_maskMode;
}

bool MaskLayer::hasOpaqueImage(const RenderElement& renderer) const
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

TextStream& operator<<(TextStream& ts, const MaskLayer& layer)
{
    TextStream::GroupScope scope(ts);

    ts << "mask-layer"_s;
    ts.dumpProperty("image"_s, layer.image());
    ts.dumpProperty("position"_s, layer.position());
    ts.dumpProperty("size"_s, layer.size());
    ts.dumpProperty("repeat"_s, layer.repeat());
    ts.dumpProperty("clip"_s, layer.clip());
    ts.dumpProperty("origin"_s, layer.origin());
    ts.dumpProperty("composite"_s, layer.composite());
    ts.dumpProperty("mask-mode"_s, layer.maskMode());

    return ts;
}

} // namespace Style
} // namespace WebCore
