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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER “AS IS” AND ANY
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

#include "ShapeOutsideInfo.h"

#include "BoxShape.h"
#include "FloatingObjects.h"
#include "LengthFunctions.h"
#include "RenderBlockFlow.h"
#include "RenderBox.h"
#include "RenderFragmentContainer.h"
#include "RenderImage.h"

namespace WebCore {

LayoutRect ShapeOutsideInfo::computedShapePhysicalBoundingBox() const
{
    LayoutRect physicalBoundingBox = computedShape().shapeMarginLogicalBoundingBox();
    physicalBoundingBox.setX(physicalBoundingBox.x() + logicalLeftOffset());
    physicalBoundingBox.setY(physicalBoundingBox.y() + logicalTopOffset());
    if (m_renderer.style().isFlippedBlocksWritingMode())
        physicalBoundingBox.setY(m_renderer.logicalHeight() - physicalBoundingBox.maxY());
    if (!m_renderer.style().isHorizontalWritingMode())
        physicalBoundingBox = physicalBoundingBox.transposedRect();
    return physicalBoundingBox;
}

FloatPoint ShapeOutsideInfo::shapeToRendererPoint(const FloatPoint& point) const
{
    FloatPoint result = FloatPoint(point.x() + logicalLeftOffset(), point.y() + logicalTopOffset());
    if (m_renderer.style().isFlippedBlocksWritingMode())
        result.setY(m_renderer.logicalHeight() - result.y());
    if (!m_renderer.style().isHorizontalWritingMode())
        result = result.transposedPoint();
    return result;
}

FloatSize ShapeOutsideInfo::shapeToRendererSize(const FloatSize& size) const
{
    if (!m_renderer.style().isHorizontalWritingMode())
        return size.transposedSize();
    return size;
}

static inline CSSBoxType referenceBox(const ShapeValue& shapeValue)
{
    if (shapeValue.cssBox() == CSSBoxType::BoxMissing) {
        if (shapeValue.type() == ShapeValue::Type::Image)
            return CSSBoxType::ContentBox;
        return CSSBoxType::MarginBox;
    }
    return shapeValue.cssBox();
}

void ShapeOutsideInfo::setReferenceBoxLogicalSize(LayoutSize newReferenceBoxLogicalSize)
{
    bool isHorizontalWritingMode = m_renderer.containingBlock()->style().isHorizontalWritingMode();
    switch (referenceBox(*m_renderer.style().shapeOutside())) {
    case CSSBoxType::MarginBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.expand(m_renderer.horizontalMarginExtent(), m_renderer.verticalMarginExtent());
        else
            newReferenceBoxLogicalSize.expand(m_renderer.verticalMarginExtent(), m_renderer.horizontalMarginExtent());
        break;
    case CSSBoxType::BorderBox:
        break;
    case CSSBoxType::PaddingBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.horizontalBorderExtent(), m_renderer.verticalBorderExtent());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.verticalBorderExtent(), m_renderer.horizontalBorderExtent());
        break;
    case CSSBoxType::ContentBox:
        if (isHorizontalWritingMode)
            newReferenceBoxLogicalSize.shrink(m_renderer.horizontalBorderAndPaddingExtent(), m_renderer.verticalBorderAndPaddingExtent());
        else
            newReferenceBoxLogicalSize.shrink(m_renderer.verticalBorderAndPaddingExtent(), m_renderer.horizontalBorderAndPaddingExtent());
        break;
    case CSSBoxType::FillBox:
    case CSSBoxType::StrokeBox:
    case CSSBoxType::ViewBox:
    case CSSBoxType::BoxMissing:
        ASSERT_NOT_REACHED();
        break;
    }

    if (m_referenceBoxLogicalSize == newReferenceBoxLogicalSize)
        return;
    markShapeAsDirty();
    m_referenceBoxLogicalSize = newReferenceBoxLogicalSize;
}

static inline bool checkShapeImageOrigin(Document& document, const StyleImage& styleImage)
{
    if (styleImage.isGeneratedImage())
        return true;

    ASSERT(styleImage.cachedImage());
    CachedImage& cachedImage = *(styleImage.cachedImage());
    if (cachedImage.isOriginClean(&document.securityOrigin()))
        return true;

    const URL& url = cachedImage.url();
    String urlString = url.isNull() ? "''" : url.stringCenterEllipsizedToLength();
    document.addConsoleMessage(MessageSource::Security, MessageLevel::Error, "Unsafe attempt to load URL " + urlString + ".");

    return false;
}

static LayoutRect getShapeImageMarginRect(const RenderBox& renderBox, const LayoutSize& referenceBoxLogicalSize)
{
    LayoutPoint marginBoxOrigin(-renderBox.marginLogicalLeft() - renderBox.borderAndPaddingLogicalLeft(), -renderBox.marginBefore() - renderBox.borderBefore() - renderBox.paddingBefore());
    LayoutSize marginBoxSizeDelta(renderBox.marginLogicalWidth() + renderBox.borderAndPaddingLogicalWidth(), renderBox.marginLogicalHeight() + renderBox.borderAndPaddingLogicalHeight());
    LayoutSize marginRectSize(referenceBoxLogicalSize + marginBoxSizeDelta);
    marginRectSize.clampNegativeToZero();
    return LayoutRect(marginBoxOrigin, marginRectSize);
}

std::unique_ptr<Shape> ShapeOutsideInfo::createShapeForImage(StyleImage* styleImage, float shapeImageThreshold, WritingMode writingMode, float margin) const
{
    LayoutSize imageSize = m_renderer.calculateImageIntrinsicDimensions(styleImage, m_referenceBoxLogicalSize, RenderImage::ScaleByEffectiveZoom);
    styleImage->setContainerContextForRenderer(m_renderer, imageSize, m_renderer.style().effectiveZoom());

    const LayoutRect& marginRect = getShapeImageMarginRect(m_renderer, m_referenceBoxLogicalSize);
    const LayoutRect& imageRect = is<RenderImage>(m_renderer)
        ? downcast<RenderImage>(m_renderer).replacedContentRect()
        : LayoutRect(LayoutPoint(), imageSize);

    ASSERT(!styleImage->isPending());
    RefPtr<Image> image = styleImage->image(const_cast<RenderBox*>(&m_renderer), imageSize);
    return Shape::createRasterShape(image.get(), shapeImageThreshold, imageRect, marginRect, writingMode, margin);
}

const Shape& ShapeOutsideInfo::computedShape() const
{
    if (Shape* shape = m_shape.get())
        return *shape;

    const RenderStyle& style = m_renderer.style();
    ASSERT(m_renderer.containingBlock());
    const RenderStyle& containingBlockStyle = m_renderer.containingBlock()->style();

    WritingMode writingMode = containingBlockStyle.writingMode();
    float margin = floatValueForLength(m_renderer.style().shapeMargin(), m_renderer.containingBlock() ? m_renderer.containingBlock()->contentWidth() : 0_lu);
    float shapeImageThreshold = style.shapeImageThreshold();
    const ShapeValue& shapeValue = *style.shapeOutside();

    switch (shapeValue.type()) {
    case ShapeValue::Type::Shape:
        ASSERT(shapeValue.shape());
        m_shape = Shape::createShape(*shapeValue.shape(), m_referenceBoxLogicalSize, writingMode, margin);
        break;
    case ShapeValue::Type::Image:
        ASSERT(shapeValue.isImageValid());
        m_shape = createShapeForImage(shapeValue.image(), shapeImageThreshold, writingMode, margin);
        break;
    case ShapeValue::Type::Box: {
        RoundedRect shapeRect = computeRoundedRectForBoxShape(referenceBox(shapeValue), m_renderer);
        if (!containingBlockStyle.isHorizontalWritingMode())
            shapeRect = shapeRect.transposedRect();
        m_shape = Shape::createBoxShape(shapeRect, writingMode, margin);
        break;
    }
    }

    ASSERT(m_shape);
    return *m_shape;
}

static inline LayoutUnit borderBeforeInWritingMode(const RenderBox& renderer, WritingMode writingMode)
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

static inline LayoutUnit borderAndPaddingBeforeInWritingMode(const RenderBox& renderer, WritingMode writingMode)
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

LayoutUnit ShapeOutsideInfo::logicalTopOffset() const
{
    switch (referenceBox(*m_renderer.style().shapeOutside())) {
    case CSSBoxType::MarginBox:
        return -m_renderer.marginBefore(&m_renderer.containingBlock()->style());
    case CSSBoxType::BorderBox:
        return 0_lu;
    case CSSBoxType::PaddingBox:
        return borderBeforeInWritingMode(m_renderer, m_renderer.containingBlock()->style().writingMode());
    case CSSBoxType::ContentBox:
        return borderAndPaddingBeforeInWritingMode(m_renderer, m_renderer.containingBlock()->style().writingMode());
    case CSSBoxType::FillBox:
        break;
    case CSSBoxType::StrokeBox:
        break;
    case CSSBoxType::ViewBox:
        break;
    case CSSBoxType::BoxMissing:
        break;
    }
    
    ASSERT_NOT_REACHED();
    return 0_lu;
}

static inline LayoutUnit borderStartWithStyleForWritingMode(const RenderBox& renderer, const RenderStyle& style)
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

static inline LayoutUnit borderAndPaddingStartWithStyleForWritingMode(const RenderBox& renderer, const RenderStyle& style)
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

LayoutUnit ShapeOutsideInfo::logicalLeftOffset() const
{
    if (m_renderer.isRenderFragmentContainer())
        return 0_lu;
    
    switch (referenceBox(*m_renderer.style().shapeOutside())) {
    case CSSBoxType::MarginBox:
        return -m_renderer.marginStart(&m_renderer.containingBlock()->style());
    case CSSBoxType::BorderBox:
        return 0_lu;
    case CSSBoxType::PaddingBox:
        return borderStartWithStyleForWritingMode(m_renderer, m_renderer.containingBlock()->style());
    case CSSBoxType::ContentBox:
        return borderAndPaddingStartWithStyleForWritingMode(m_renderer, m_renderer.containingBlock()->style());
    case CSSBoxType::FillBox:
        break;
    case CSSBoxType::StrokeBox:
        break;
    case CSSBoxType::ViewBox:
        break;
    case CSSBoxType::BoxMissing:
        break;
    }

    ASSERT_NOT_REACHED();
    return 0_lu;
}

bool ShapeOutsideInfo::isEnabledFor(const RenderBox& box)
{
    ShapeValue* shapeValue = box.style().shapeOutside();
    if (!box.isFloating() || !shapeValue)
        return false;

    switch (shapeValue->type()) {
    case ShapeValue::Type::Shape: return shapeValue->shape();
    case ShapeValue::Type::Image: return shapeValue->isImageValid() && checkShapeImageOrigin(box.document(), *(shapeValue->image()));
    case ShapeValue::Type::Box: return true;
    }

    ASSERT_NOT_REACHED();
    return false;
}

ShapeOutsideDeltas ShapeOutsideInfo::computeDeltasForContainingBlockLine(const RenderBlockFlow& containingBlock, const FloatingObject& floatingObject, LayoutUnit lineTop, LayoutUnit lineHeight)
{
    ASSERT(lineHeight >= 0);
    LayoutUnit borderBoxTop = containingBlock.logicalTopForFloat(floatingObject) + containingBlock.marginBeforeForChild(m_renderer);
    LayoutUnit borderBoxLineTop = lineTop - borderBoxTop;

    if (isShapeDirty() || !m_shapeOutsideDeltas.isForLine(borderBoxLineTop, lineHeight)) {
        LayoutUnit referenceBoxLineTop = borderBoxLineTop - logicalTopOffset();
        LayoutUnit floatMarginBoxWidth = std::max<LayoutUnit>(0_lu, containingBlock.logicalWidthForFloat(floatingObject));

        if (computedShape().lineOverlapsShapeMarginBounds(referenceBoxLineTop, lineHeight)) {
            LineSegment segment = computedShape().getExcludedInterval((borderBoxLineTop - logicalTopOffset()), std::min(lineHeight, shapeLogicalBottom() - borderBoxLineTop));
            if (segment.isValid) {
                LayoutUnit logicalLeftMargin = containingBlock.style().isLeftToRightDirection() ? containingBlock.marginStartForChild(m_renderer) : containingBlock.marginEndForChild(m_renderer);
                LayoutUnit rawLeftMarginBoxDelta = segment.logicalLeft + logicalLeftOffset() + logicalLeftMargin;
                LayoutUnit leftMarginBoxDelta = clampTo<LayoutUnit>(rawLeftMarginBoxDelta, 0_lu, floatMarginBoxWidth);

                LayoutUnit logicalRightMargin = containingBlock.style().isLeftToRightDirection() ? containingBlock.marginEndForChild(m_renderer) : containingBlock.marginStartForChild(m_renderer);
                LayoutUnit rawRightMarginBoxDelta = segment.logicalRight + logicalLeftOffset() - containingBlock.logicalWidthForChild(m_renderer) - logicalRightMargin;
                LayoutUnit rightMarginBoxDelta = clampTo<LayoutUnit>(rawRightMarginBoxDelta, -floatMarginBoxWidth, 0_lu);

                m_shapeOutsideDeltas = ShapeOutsideDeltas(leftMarginBoxDelta, rightMarginBoxDelta, true, borderBoxLineTop, lineHeight);
                return m_shapeOutsideDeltas;
            }
        }

        // Lines that do not overlap the shape should act as if the float
        // wasn't there for layout purposes. So we set the deltas to remove the
        // entire width of the float
        m_shapeOutsideDeltas = ShapeOutsideDeltas(floatMarginBoxWidth, -floatMarginBoxWidth, false, borderBoxLineTop, lineHeight);
    }

    return m_shapeOutsideDeltas;
}

}
