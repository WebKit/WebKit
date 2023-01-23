/*
 * Copyright (C) 2000 Lars Knoll (knoll@kde.org)
 *           (C) 2000 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003-2021 Apple Inc. All rights reserved.
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

#include "StyleImage.h"

namespace WebCore {

class CSSImageGeneratorValue;

class StyleGeneratedImage : public StyleImage {
public:
    virtual CSSImageGeneratorValue& imageValue() = 0;

protected:
    explicit StyleGeneratedImage(Type, bool isFixedSize);

    FloatSize imageSize(const RenderElement*, float multiplier) const final;
    void computeIntrinsicDimensions(const RenderElement*, Length& intrinsicWidth, Length& intrinsicHeight, FloatSize& intrinsicRatio) final;
    bool imageHasRelativeWidth() const final { return !m_fixedSize; }
    bool imageHasRelativeHeight() const final { return !m_fixedSize; }
    bool usesImageContainerSize() const final { return !m_fixedSize; }
    void setContainerContextForRenderer(const RenderElement&, const FloatSize& containerSize, float) final { m_containerSize = containerSize; }

    // All generated images must be able to compute their fixed size.
    virtual FloatSize fixedSize(const RenderElement&) const = 0;

    FloatSize m_containerSize;
    bool m_fixedSize;
};

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_STYLE_IMAGE(StyleGeneratedImage, isGeneratedImage)
