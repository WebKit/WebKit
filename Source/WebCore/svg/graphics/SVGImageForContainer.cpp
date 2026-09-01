/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#include "config.h"
#include "SVGImageForContainer.h"

#include "AffineTransform.h"
#include "DisplayListRecorderImpl.h"
#include "FloatRect.h"
#include "FloatSize.h"
#include "GraphicsContext.h"
#include "Image.h"
#include "NativeImage.h"

namespace WebCore {

SVGImageForContainer::SVGImageForContainer(SVGImage* image, SVGImage::ContainerContext&& containerContext)
    : m_image(image)
    , m_containerContext(WTF::move(containerContext))
{
}

FloatSize SVGImageForContainer::size(ImageOrientation) const
{
    FloatSize scaledContainerSize(m_containerContext.containerSize);
    scaledContainerSize.scale(m_containerContext.containerZoom);
    return FloatSize(roundedIntSize(scaledContainerSize));
}

// An SVG image is an isolated document, so its rendering must not depend on the host's paint
// state. Reset to what a new context starts with.
// We intentionally leave alpha alone: it applies to the image as a whole, not to its content,
// so draw() applies it around the replay.
static void resetToIsolatedDocumentState(GraphicsContext& context)
{
    context.setFillBrush(SourceBrush { Color::black });
    context.setFillRule(WindRule::NonZero);
    context.setStrokeBrush(SourceBrush { Color::black });
    context.setStrokeThickness(0);
    context.setStrokeStyle(StrokeStyle::SolidStroke);
    context.setCompositeMode({ CompositeOperator::SourceOver, BlendMode::Normal });
    context.clearDropShadow();
    context.setStyle({ });
    context.setTextDrawingMode(TextDrawingMode::Fill);
    context.setImageInterpolationQuality(InterpolationQuality::Default);
    context.setShouldAntialias(true);
    context.setShouldSmoothFonts(true);
    context.setShouldSubpixelQuantizeFonts(true);
    context.setShadowsIgnoreTransforms(false);
    context.setDrawLuminanceMask(false);
    context.setLineCap(LineCap::Butt);
    context.setLineJoin(LineJoin::Miter);
    context.setMiterLimit(10);
    context.setLineDash({ }, 0);
}

ImageDrawResult SVGImageForContainer::draw(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, ImagePaintingOptions options)
{
    RefPtr<SVGImage> image = m_image.get();
    if (!image)
        return ImageDrawResult::DidNothing;

    if (!image->displayListCacheEnabled())
        return image->drawForContainer(context, m_containerContext, dstRect, srcRect, options);

    DrawParameters drawParameters { dstRect, srcRect };
    if (!m_displayList || m_drawParameters != drawParameters) {
        DisplayList::RecorderImpl recorder(GraphicsContextState(), enclosingIntRect(dstRect), AffineTransform());
        image->drawForContainer(recorder, m_containerContext, dstRect, srcRect, options);
        m_displayList = recorder.takeDisplayList();
        m_drawParameters = drawParameters;
    }

    GraphicsContextStateSaver stateSaver(context);
    resetToIsolatedDocumentState(context);

    float alpha = context.alpha();
    TransparencyLayerScope transparencyScope(context, alpha, alpha < 1);

    context.drawDisplayList(protect(*m_displayList));
    return ImageDrawResult::DidDraw;
}

void SVGImageForContainer::drawPattern(GraphicsContext& context, const FloatRect& dstRect, const FloatRect& srcRect, const AffineTransform& patternTransform,
    const FloatPoint& phase, const FloatSize& spacing, ImagePaintingOptions options)
{
    protect(m_image)->drawPatternForContainer(context, m_containerContext, srcRect, patternTransform, phase, spacing, dstRect, options);
}

RefPtr<NativeImage> SVGImageForContainer::currentNativeImage()
{
    return protect(m_image)->nativeImage(size());
}

} // namespace WebCore
