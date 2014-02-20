/*
 * Copyright (C) 2012 Adobe Systems Incorporated. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials
 *    provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include "config.h"
#include "ShapeInfo.h"

#if ENABLE(CSS_SHAPES)

#include "LengthFunctions.h"
#include "RenderBlock.h"
#include "RenderBox.h"
#include "RenderImage.h"
#include "RenderRegion.h"
#include "RenderStyle.h"
#include "Shape.h"

namespace WebCore {

template<class RenderType>
void ShapeInfo<RenderType>::setReferenceBoxLogicalSize(LayoutSize newReferenceBoxLogicalSize)
{
    bool isHorizontalWritingMode = this->styleForWritingMode().isHorizontalWritingMode();
    switch (referenceBox()) {
    case MarginBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.expand(m_renderer.horizontalMarginExtent(), m_renderer.verticalMarginExtent());
        else
            newReferenceBoxLogicalSize.expand(m_renderer.verticalMarginExtent(), m_renderer.horizontalMarginExtent());
        break;
    case BorderBox:
        break;
    case PaddingBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.horizontalBorderExtent(), m_renderer.verticalBorderExtent());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.verticalBorderExtent(), m_renderer.horizontalBorderExtent());
        break;
    case ContentBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.horizontalBorderAndPaddingExtent(), m_renderer.verticalBorderAndPaddingExtent());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.verticalBorderAndPaddingExtent(), m_renderer.horizontalBorderAndPaddingExtent());
        break;
    case Fill:
    case Stroke:
    case ViewBox:
    case BoxMissing:
        ASSERT_NOT_REACHED();
        break;
    }

    if (m_referenceBoxLogicalSize == newReferenceBoxLogicalSize)
        return;
    markShapeAsDirty();
    m_referenceBoxLogicalSize = newReferenceBoxLogicalSize;
}

bool checkShapeImageOrigin(Document& document, CachedImage& cachedImage)
{
    if (cachedImage.isOriginClean(document.securityOrigin()))
        return true;

    const URL& url = cachedImage.url();
    String urlString = url.isNull() ? "''" : url.stringCenterEllipsizedToLength();
    document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Unsafe attempt to load URL " + urlString + ".");

    return false;
}

static LayoutRect getShapeImageReplacedRect(const RenderBox& renderBox, const StyleImage& styleImage)
{
    if (renderBox.isRenderImage()) {
        const RenderImage& renderImage = *toRenderImage(&renderBox);
        return renderImage.replacedContentRect(renderBox.intrinsicSize());
    }

    ASSERT(styleImage.cachedImage());
    ASSERT(styleImage.cachedImage()->hasImage());
    return LayoutRect(LayoutPoint(), styleImage.cachedImage()->image()->size());
}

static LayoutRect getShapeImageMarginRect(const RenderBox& renderBox, const LayoutSize& referenceBoxLogicalSize)
{
    LayoutPoint marginBoxOrigin(-renderBox.marginLogicalLeft() - renderBox.borderAndPaddingLogicalLeft(), -renderBox.marginBefore() - renderBox.borderBefore() - renderBox.paddingBefore());
    LayoutSize marginBoxSizeDelta(renderBox.marginLogicalWidth() + renderBox.borderAndPaddingLogicalWidth(), renderBox.marginLogicalHeight() + renderBox.borderAndPaddingLogicalHeight());
    return LayoutRect(marginBoxOrigin, referenceBoxLogicalSize + marginBoxSizeDelta);
}

template<class RenderType>
const Shape& ShapeInfo<RenderType>::computedShape() const
{
    if (Shape* shape = m_shape.get())
        return *shape;

    WritingMode writingMode = this->styleForWritingMode().writingMode();
    Length margin = m_renderer.style().shapeMargin();
    Length padding = m_renderer.style().shapePadding();
    float shapeImageThreshold = m_renderer.style().shapeImageThreshold();
    const ShapeValue* shapeValue = this->shapeValue();
    ASSERT(shapeValue);

    switch (shapeValue->type()) {
    case ShapeValue::Shape:
        ASSERT(shapeValue->shape());
        m_shape = Shape::createShape(shapeValue->shape(), m_referenceBoxLogicalSize, writingMode, margin, padding);
        break;
    case ShapeValue::Image: {
        ASSERT(shapeValue->image());
        const StyleImage& styleImage = *(shapeValue->image());
        const LayoutRect& imageRect = getShapeImageReplacedRect(m_renderer, styleImage);
        const LayoutRect& marginRect = getShapeImageMarginRect(m_renderer, m_referenceBoxLogicalSize);
        m_shape = Shape::createRasterShape(styleImage, shapeImageThreshold, imageRect, marginRect, writingMode, margin, padding);
        break;
    }
    case ShapeValue::Box: {
        // FIXME This does not properly compute the rounded corners as specified in all conditions.
        // https://bugs.webkit.org/show_bug.cgi?id=127982
        const RoundedRect& shapeRect = m_renderer.style().getRoundedBorderFor(LayoutRect(LayoutPoint(), m_referenceBoxLogicalSize), &(m_renderer.view()));
        m_shape = Shape::createLayoutBoxShape(shapeRect, writingMode, margin, padding);
        break;
    }
    case ShapeValue::Outside:
        // Outside should have already resolved to a different shape value
        ASSERT_NOT_REACHED();
    }

    ASSERT(m_shape);
    return *m_shape;
}

template<class RenderType>
SegmentList ShapeInfo<RenderType>::computeSegmentsForLine(LayoutUnit lineTop, LayoutUnit lineHeight) const
{
    ASSERT(lineHeight >= 0);
    SegmentList segments;

    getIntervals((lineTop - logicalTopOffset()), std::min(lineHeight, shapeLogicalBottom() - lineTop), segments);

    for (size_t i = 0; i < segments.size(); i++) {
        segments[i].logicalLeft += logicalLeftOffset();
        segments[i].logicalRight += logicalLeftOffset();
    }

    return segments;
}

template<class RenderType>
inline LayoutUnit borderBeforeInWritingMode(const RenderType& renderer, WritingMode writingMode)
{
    switch (writingMode) {
    case TopToBottomWritingMode: return renderer.borderTop();
    case BottomToTopWritingMode: return renderer.borderBottom();
    case LeftToRightWritingMode: return renderer.borderLeft();
    case RightToLeftWritingMode: return renderer.borderRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderBefore();
}

template<class RenderType>
inline LayoutUnit borderAndPaddingBeforeInWritingMode(const RenderType& renderer, WritingMode writingMode)
{
    switch (writingMode) {
    case TopToBottomWritingMode: return renderer.borderTop() + renderer.paddingTop();
    case BottomToTopWritingMode: return renderer.borderBottom() + renderer.paddingBottom();
    case LeftToRightWritingMode: return renderer.borderLeft() + renderer.paddingLeft();
    case RightToLeftWritingMode: return renderer.borderRight() + renderer.paddingRight();
    }

    ASSERT_NOT_REACHED();
    return renderer.borderAndPaddingBefore();
}

template<class RenderType>
LayoutUnit ShapeInfo<RenderType>::logicalTopOffset() const
{
    switch (referenceBox()) {
    case MarginBox: return -m_renderer.marginBefore(&styleForWritingMode());
    case BorderBox: return LayoutUnit();
    case PaddingBox: return borderBeforeInWritingMode(m_renderer, styleForWritingMode().writingMode());
    case ContentBox: return borderAndPaddingBeforeInWritingMode(m_renderer, styleForWritingMode().writingMode());
    case Fill: break;
    case Stroke: break;
    case ViewBox: break;
    case BoxMissing: break;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

template<class RenderType>
inline LayoutUnit borderStartWithStyleForWritingMode(const RenderType& renderer, const RenderStyle& style)
{
    if (style.isHorizontalWritingMode()) {
        if (style.isLeftToRightDirection())
            return renderer.borderLeft();

        return renderer.borderRight();
    }
    if (style.isLeftToRightDirection())
        return renderer.borderTop();

    return renderer.borderBottom();
}

template<class RenderType>
inline LayoutUnit borderAndPaddingStartWithStyleForWritingMode(const RenderType& renderer, const RenderStyle& style)
{
    if (style.isHorizontalWritingMode()) {
        if (style.isLeftToRightDirection())
            return renderer.borderLeft() + renderer.paddingLeft();

        return renderer.borderRight() + renderer.paddingRight();
    }
    if (style.isLeftToRightDirection())
        return renderer.borderTop() + renderer.paddingTop();

    return renderer.borderBottom() + renderer.paddingBottom();
}

template<class RenderType>
LayoutUnit ShapeInfo<RenderType>::logicalLeftOffset() const
{
    if (m_renderer.isRenderRegion())
        return LayoutUnit();

    switch (referenceBox()) {
    case MarginBox: return -m_renderer.marginStart(&styleForWritingMode());
    case BorderBox: return LayoutUnit();
    case PaddingBox: return borderStartWithStyleForWritingMode(m_renderer, styleForWritingMode());
    case ContentBox: return borderAndPaddingStartWithStyleForWritingMode(m_renderer, styleForWritingMode());
    case Fill: break;
    case Stroke: break;
    case ViewBox: break;
    case BoxMissing: break;
    }

    ASSERT_NOT_REACHED();
    return LayoutUnit();
}

template class ShapeInfo<RenderBlock>;
template class ShapeInfo<RenderBox>;

}
#endif
