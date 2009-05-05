/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
 *               2008 Eric Seidel <eric@webkit.org>
 *               2008 Dirk Schulze <krit@webkit.org>
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"

#if ENABLE(SVG)
#include "SVGPaintServerGradient.h"

#include "FloatConversion.h"
#include "GraphicsContext.h"
#include "ImageBuffer.h"
#include "RenderObject.h"
#include "RenderView.h"
#include "SVGGradientElement.h"
#include "SVGPaintServerLinearGradient.h"
#include "SVGPaintServerRadialGradient.h"
#include "SVGRenderSupport.h"
#include "SVGRenderTreeAsText.h"

using namespace std;

namespace WebCore {

static TextStream& operator<<(TextStream& ts, GradientSpreadMethod m)
{
    switch (m) {
        case SpreadMethodPad:
            ts << "PAD"; break;
        case SpreadMethodRepeat:
            ts << "REPEAT"; break;
        case SpreadMethodReflect:
            ts << "REFLECT"; break;
    }

    return ts;
}

static TextStream& operator<<(TextStream& ts, const Vector<SVGGradientStop>& l)
{
    ts << "[";
    for (Vector<SVGGradientStop>::const_iterator it = l.begin(); it != l.end(); ++it) {
        ts << "(" << it->first << "," << it->second << ")";
        if (it + 1 != l.end())
            ts << ", ";
    }
    ts << "]";
    return ts;
}

SVGPaintServerGradient::SVGPaintServerGradient(const SVGGradientElement* owner)
    : m_boundingBoxMode(true)
    , m_ownerElement(owner)
#if PLATFORM(CG)
    , m_savedContext(0)
    , m_imageBuffer(0)
#endif
{
    ASSERT(owner);
}

SVGPaintServerGradient::~SVGPaintServerGradient()
{
}

Gradient* SVGPaintServerGradient::gradient() const
{
    return m_gradient.get();
}

void SVGPaintServerGradient::setGradient(PassRefPtr<Gradient> gradient)
{
    m_gradient = gradient;
}

bool SVGPaintServerGradient::boundingBoxMode() const
{
    return m_boundingBoxMode;
}

void SVGPaintServerGradient::setBoundingBoxMode(bool mode)
{
    m_boundingBoxMode = mode;
}

TransformationMatrix SVGPaintServerGradient::gradientTransform() const
{
    return m_gradientTransform;
}

void SVGPaintServerGradient::setGradientTransform(const TransformationMatrix& transform)
{
    m_gradientTransform = transform;
}

#if PLATFORM(CG)
static inline const RenderObject* findTextRootObject(const RenderObject* start)
{
    while (start && !start->isSVGText())
        start = start->parent();
    ASSERT(start);
    ASSERT(start->isSVGText());

    return start;
}

static inline bool createMaskAndSwapContextForTextGradient(
    GraphicsContext*& context, GraphicsContext*& savedContext,
    OwnPtr<ImageBuffer>& imageBuffer, const RenderObject* object)
{
    FloatRect maskBBox = const_cast<RenderObject*>(findTextRootObject(object))->objectBoundingBox();
    IntRect maskRect = enclosingIntRect(object->absoluteTransform().mapRect(maskBBox));

    IntSize maskSize(maskRect.width(), maskRect.height());
    clampImageBufferSizeToViewport(object->view()->frameView(), maskSize);

    auto_ptr<ImageBuffer> maskImage = ImageBuffer::create(maskSize, false);

    if (!maskImage.get())
        return false;

    GraphicsContext* maskImageContext = maskImage->context();

    maskImageContext->save();
    maskImageContext->translate(-maskRect.x(), -maskRect.y());
    maskImageContext->concatCTM(object->absoluteTransform());

    imageBuffer.set(maskImage.release());
    savedContext = context;

    context = maskImageContext;

    return true;
}

static inline TransformationMatrix clipToTextMask(GraphicsContext* context,
    OwnPtr<ImageBuffer>& imageBuffer, const RenderObject* object,
    const SVGPaintServerGradient* gradientServer)
{
    FloatRect maskBBox = const_cast<RenderObject*>(findTextRootObject(object))->objectBoundingBox();

    // Fixup transformations to be able to clip to mask
    TransformationMatrix transform = object->absoluteTransform();
    FloatRect textBoundary = transform.mapRect(maskBBox);

    IntSize maskSize(lroundf(textBoundary.width()), lroundf(textBoundary.height()));
    clampImageBufferSizeToViewport(object->view()->frameView(), maskSize);
    textBoundary.setSize(textBoundary.size().shrunkTo(maskSize));

    // Clip current context to mask image (gradient)
    context->concatCTM(transform.inverse());
    context->clipToImageBuffer(textBoundary, imageBuffer.get());
    context->concatCTM(transform);

    TransformationMatrix matrix;
    if (gradientServer->boundingBoxMode()) {
        matrix.translate(maskBBox.x(), maskBBox.y());
        matrix.scaleNonUniform(maskBBox.width(), maskBBox.height());
    }
    matrix.multiply(gradientServer->gradientTransform());
    return matrix;
}
#endif

bool SVGPaintServerGradient::setup(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType type, bool isPaintingText) const
{
    m_ownerElement->buildGradient();

    const SVGRenderStyle* style = object->style()->svgStyle();
    bool isFilled = (type & ApplyToFillTargetType) && style->hasFill();
    bool isStroked = (type & ApplyToStrokeTargetType) && style->hasStroke();

    ASSERT(isFilled && !isStroked || !isFilled && isStroked);

    context->save();

    if (isPaintingText) {
#if PLATFORM(CG)
        if (!createMaskAndSwapContextForTextGradient(context, m_savedContext, m_imageBuffer, object)) {
            context->restore();
            return false;
        }
#endif
        context->setTextDrawingMode(isFilled ? cTextFill : cTextStroke);
    }

    if (isFilled) {
        context->setAlpha(style->fillOpacity());
        context->setFillGradient(m_gradient);
        context->setFillRule(style->fillRule());
    }
    if (isStroked) {
        context->setAlpha(style->strokeOpacity());
        context->setStrokeGradient(m_gradient);
        applyStrokeStyleToContext(context, object->style(), object);
    }

    TransformationMatrix matrix;
    // CG platforms will handle the gradient space transform for text in
    // teardown, so we don't apply it here.  For non-CG platforms, we
    // want the text bounding box applied to the gradient space transform now,
    // so the gradient shader can use it.
#if PLATFORM(CG)
    if (boundingBoxMode() && !isPaintingText) {
#else
    if (boundingBoxMode()) {
#endif
        FloatRect bbox = object->objectBoundingBox();
        // Don't use gradients for 1d objects like horizontal/vertical 
        // lines or rectangles without width or height.
        if (bbox.width() == 0 || bbox.height() == 0) {
            Color color(0, 0, 0);
            context->setStrokeColor(color);
            return true;
        }
        matrix.translate(bbox.x(), bbox.y());
        matrix.scaleNonUniform(bbox.width(), bbox.height());
    }
    matrix.multiply(gradientTransform());
    m_gradient->setGradientSpaceTransform(matrix);

    return true;
}

void SVGPaintServerGradient::teardown(GraphicsContext*& context, const RenderObject* object, SVGPaintTargetType, bool isPaintingText) const
{
#if PLATFORM(CG)
    // renderPath() is not used when painting text, so we paint the gradient during teardown()
    if (isPaintingText && m_savedContext) {

        // Restore on-screen drawing context
        context = m_savedContext;
        m_savedContext = 0;

        TransformationMatrix matrix = clipToTextMask(context, m_imageBuffer, object, this);
        m_gradient->setGradientSpaceTransform(matrix);
        context->setFillGradient(m_gradient);

        FloatRect maskBBox = const_cast<RenderObject*>(findTextRootObject(object))->objectBoundingBox();

        context->fillRect(maskBBox);

        m_imageBuffer.clear(); // we're done with our text mask buffer
    }
#endif
    context->restore();
}

TextStream& SVGPaintServerGradient::externalRepresentation(TextStream& ts) const
{
    // Gradients/patterns aren't setup, until they are used for painting. Work around that fact.
    m_ownerElement->buildGradient();

    // abstract, don't stream type
    ts  << "[stops=" << gradientStops() << "]";
    if (m_gradient->spreadMethod() != SpreadMethodPad)
        ts << "[method=" << m_gradient->spreadMethod() << "]";
    if (!boundingBoxMode())
        ts << " [bounding box mode=" << boundingBoxMode() << "]";
    if (!gradientTransform().isIdentity())
        ts << " [transform=" << gradientTransform() << "]";

    return ts;
}

} // namespace WebCore

#endif
