/*
 * Copyright (C) 2009 Dirk Schulze <krit@webkit.org>
 * Copyright (C) 2013 Google Inc. All rights reserved.
 * Copyright (C) 2021 Apple Inc.  All rights reserved.
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
 */

#pragma once

#include "FilterFunction.h"
#include "FloatRect.h"
#include "GraphicsTypes.h"
#include "ImageBuffer.h"
#include "RenderingMode.h"

namespace WebCore {

class FilterEffect;

class Filter : public FilterFunction {
    using FilterFunction::apply;

public:
    FloatSize filterScale() const { return m_filterScale; }
    void setFilterScale(const FloatSize& filterScale) { m_filterScale = filterScale; }

    FloatRect sourceImageRect() const { return m_sourceImageRect; }
    void setSourceImageRect(const FloatRect& sourceImageRect) { m_sourceImageRect = sourceImageRect; }

    FloatRect filterRegion() const { return m_filterRegion; }
    void setFilterRegion(const FloatRect& filterRegion) { m_filterRegion = filterRegion; }

    virtual FloatSize scaledByFilterScale(FloatSize size) const { return size * m_filterScale; }
    virtual bool apply() = 0;

    ImageBuffer* sourceImage() const { return m_sourceImage.get(); }
    void setSourceImage(RefPtr<ImageBuffer>&& sourceImage) { m_sourceImage = WTFMove(sourceImage); }

    RenderingMode renderingMode() const { return m_renderingMode; }
    void setRenderingMode(RenderingMode renderingMode) { m_renderingMode = renderingMode; }

protected:
    Filter(Filter::Type filterType, RenderingMode renderingMode, const FloatSize& filterScale)
        : FilterFunction(filterType)
        , m_renderingMode(renderingMode)
        , m_filterScale(filterScale)
    {
    }

    Filter(Filter::Type filterType, RenderingMode renderingMode, const FloatSize& filterScale, const FloatRect& sourceImageRect, const FloatRect& filterRegion)
        : FilterFunction(filterType)
        , m_renderingMode(renderingMode)
        , m_filterScale(filterScale)
        , m_sourceImageRect(sourceImageRect)
        , m_filterRegion(filterRegion)
    {
    }

private:
    RenderingMode m_renderingMode;

    FloatSize m_filterScale;
    FloatRect m_sourceImageRect;
    FloatRect m_filterRegion;

    RefPtr<ImageBuffer> m_sourceImage;
};

} // namespace WebCore
