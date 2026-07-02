/*
 * Copyright (C) 2006-2024 Apple Inc. All rights reserved.
 * Copyright (C) 2013-2014 Google Inc. All rights reserved.
 * Copyright (C) 2019 Adobe. All rights reserved.
 * Copyright (c) 2020, 2021, 2022, 2026 Igalia S.L.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

#include "GraphicsContext.h"
#include "RenderLayer.h"

namespace WebCore {

class TransformPaintScope {
    WTF_MAKE_NONCOPYABLE(TransformPaintScope);
public:
    TransformPaintScope(GraphicsContext& context, const RenderLayer::LayerPaintingInfo& paintingInfo, const TransformationMatrix& transform, float deviceScaleFactor, const LayoutSize& adjustedSubpixelOffset, CheckedPtr<RenderLayer> newRootLayer = nullptr)
        : m_context(context)
        , m_oldTransform(context.getCTM())
        , m_affineTransform(transform.toAffineTransform())
        , m_transformedPaintingInfo(paintingInfo)
    {
        m_context.concatCTM(m_affineTransform);

        if (CheckedPtr regionContext = m_transformedPaintingInfo.regionContext)
            regionContext->pushTransform(m_affineTransform);

        if (newRootLayer)
            m_transformedPaintingInfo.rootLayer = newRootLayer.get();

        if (!m_transformedPaintingInfo.paintDirtyRect.isInfinite())
            m_transformedPaintingInfo.paintDirtyRect = LayoutRect(encloseRectToDevicePixels(valueOrDefault(transform.inverse()).mapRect(paintingInfo.paintDirtyRect), deviceScaleFactor));

        m_transformedPaintingInfo.subpixelOffset = adjustedSubpixelOffset;
    }

    ~TransformPaintScope()
    {
        if (CheckedPtr regionContext = m_transformedPaintingInfo.regionContext)
            regionContext->popTransform();

        m_context.setCTM(m_oldTransform);
    }

    const RenderLayer::LayerPaintingInfo& transformedPaintingInfo() const { return m_transformedPaintingInfo; }
    const AffineTransform& appliedTransform() const { return m_affineTransform; }

private:
    GraphicsContext& m_context;
    AffineTransform m_oldTransform;
    AffineTransform m_affineTransform;
    RenderLayer::LayerPaintingInfo m_transformedPaintingInfo;
};

} // namespace WebCore
