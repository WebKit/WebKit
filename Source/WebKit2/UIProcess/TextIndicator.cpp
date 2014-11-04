/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TextIndicator.h"

#include "ShareableBitmap.h"
#include <WebCore/GeometryUtilities.h>
#include <WebCore/Gradient.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/IntRect.h>
#include <WebCore/Path.h>

#if PLATFORM(MAC) && __MAC_OS_X_VERSION_MIN_REQUIRED < 101000
#define ENABLE_LEGACY_TEXT_INDICATOR_STYLE 1
#else
#define ENABLE_LEGACY_TEXT_INDICATOR_STYLE 0
#endif

using namespace WebCore;

#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
static const float cornerRadius = 3.0;

static const float shadowOffsetX = 0.0;
static const float shadowOffsetY = 1.0;
static const float shadowBlurRadius = 3.0;

static const int shadowRed = 0;
static const int shadowGreen = 0;
static const int shadowBlue = 0;
static const int shadowAlpha = 204;

static const float lightBorderThickness = 1.0;
static const float horizontalPaddingInsideLightBorder = 3.0;
static const float verticalPaddingInsideLightBorder = 1.0;

static const float horizontalBorderInsideShadow = lightBorderThickness + horizontalPaddingInsideLightBorder;
static const float verticalBorderInsideShadow = lightBorderThickness + verticalPaddingInsideLightBorder;

static const float leftBorderThickness = horizontalBorderInsideShadow + shadowOffsetX + shadowBlurRadius / 2.0;
static const float topBorderThickness = verticalBorderInsideShadow - shadowOffsetY + shadowBlurRadius / 2.0;
static const float rightBorderThickness = horizontalBorderInsideShadow - shadowOffsetX + shadowBlurRadius / 2.0;
static const float bottomBorderThickness = verticalBorderInsideShadow + shadowOffsetY + shadowBlurRadius / 2.0;

static const float horizontalOutsetToCenterOfLightBorder = horizontalBorderInsideShadow - lightBorderThickness / 2.0;
static const float verticalOutsetToCenterOfLightBorder = verticalBorderInsideShadow - lightBorderThickness / 2.0;

static const int lightBorderRed = 245;
static const int lightBorderGreen = 230;
static const int lightBorderBlue = 0;
static const int lightBorderAlpha = 255;

static const int gradientDarkRed = 237;
static const int gradientDarkGreen = 204;
static const int gradientDarkBlue = 0;
static const int gradientDarkAlpha = 255;

static const int gradientLightRed = 242;
static const int gradientLightGreen = 239;
static const int gradientLightBlue = 0;
static const int gradientLightAlpha = 255;
#else
const float flatStyleHorizontalBorder = 2;
const float flatStyleVerticalBorder = 1;
const float flatShadowOffsetX = 0;
const float flatShadowOffsetY = 5;
const float flatShadowBlurRadius = 25;
const float flatRimShadowBlurRadius = 2;
#endif

namespace WebKit {

PassRefPtr<TextIndicator> TextIndicator::create(const FloatRect& selectionRectInWindowCoordinates, const Vector<FloatRect>& textRectsInSelectionRectCoordinates, float contentImageScaleFactor, const ShareableBitmap::Handle& contentImageHandle)
{
    RefPtr<ShareableBitmap> contentImage = ShareableBitmap::create(contentImageHandle);
    if (!contentImage)
        return 0;
    ASSERT(contentImageScaleFactor != 1 || contentImage->size() == enclosingIntRect(selectionRectInWindowCoordinates).size());

    return adoptRef(new TextIndicator(selectionRectInWindowCoordinates, textRectsInSelectionRectCoordinates, contentImageScaleFactor, contentImage.release()));
}

static FloatRect inflateRect(const FloatRect& rect, float inflateX, float inflateY)
{
    FloatRect inflatedRect = rect;
    inflatedRect.inflateX(inflateX);
    inflatedRect.inflateY(inflateY);
    return inflatedRect;
}

static FloatRect outsetIndicatorRectIncludingShadow(const FloatRect rect)
{
#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
    FloatRect outsetRect = rect;
    outsetRect.move(-leftBorderThickness, -topBorderThickness);
    outsetRect.expand(leftBorderThickness + rightBorderThickness, topBorderThickness + bottomBorderThickness);
    return outsetRect;
#else
    return inflateRect(rect, flatShadowBlurRadius + flatStyleHorizontalBorder, flatShadowBlurRadius + flatStyleVerticalBorder);
#endif
}

static bool textIndicatorsForTextRectsOverlap(const Vector<FloatRect>& textRects)
{
    size_t count = textRects.size();
    if (count <= 1)
        return false;

    Vector<FloatRect> indicatorRects;
    indicatorRects.reserveInitialCapacity(count);

    for (size_t i = 0; i < count; ++i) {
        FloatRect indicatorRect = outsetIndicatorRectIncludingShadow(textRects[i]);

        for (size_t j = indicatorRects.size(); j; ) {
            --j;
            if (indicatorRect.intersects(indicatorRects[j]))
                return true;
        }

        indicatorRects.uncheckedAppend(indicatorRect);
    }

    return false;
}

TextIndicator::TextIndicator(const WebCore::FloatRect& selectionRectInWindowCoordinates, const Vector<WebCore::FloatRect>& textRectsInSelectionRectCoordinates, float contentImageScaleFactor, PassRefPtr<ShareableBitmap> contentImage)
    : m_selectionRectInWindowCoordinates(selectionRectInWindowCoordinates)
    , m_textRectsInSelectionRectCoordinates(textRectsInSelectionRectCoordinates)
    , m_contentImageScaleFactor(contentImageScaleFactor)
    , m_contentImage(contentImage)
{
    if (textIndicatorsForTextRectsOverlap(m_textRectsInSelectionRectCoordinates)) {
        m_textRectsInSelectionRectCoordinates[0] = unionRect(m_textRectsInSelectionRectCoordinates);
        m_textRectsInSelectionRectCoordinates.shrink(1);
    }
}

TextIndicator::~TextIndicator()
{
}

FloatRect TextIndicator::frameRect() const
{
    return outsetIndicatorRectIncludingShadow(m_selectionRectInWindowCoordinates);
}

#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
static inline Color lightBorderColor()
{
    return Color(lightBorderRed, lightBorderGreen, lightBorderBlue, lightBorderAlpha);
}

static inline Color shadowColor()
{
    return Color(shadowRed, shadowGreen, shadowBlue, shadowAlpha);
}

static inline Color gradientLightColor()
{
    return Color(gradientLightRed, gradientLightGreen, gradientLightBlue, gradientLightAlpha);
}

static inline Color gradientDarkColor()
{
    return Color(gradientDarkRed, gradientDarkGreen, gradientDarkBlue, gradientDarkAlpha);
}

static Path pathWithRoundedRect(const FloatRect& pathRect, float radius)
{
    Path path;
    path.addRoundedRect(pathRect, FloatSize(radius, radius));

    return path;
}
#else
static inline Color flatHighlightColor()
{
    return Color(255, 255, 0, 255);
}

static inline Color flatRimShadowColor()
{
    return Color(0, 0, 0, 38);
}

static inline Color flatDropShadowColor()
{
    return Color(0, 0, 0, 51);
}
#endif
    
void TextIndicator::draw(GraphicsContext& graphicsContext, const IntRect& /*dirtyRect*/)
{
#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
    for (size_t i = 0; i < m_textRectsInSelectionRectCoordinates.size(); ++i) {
        FloatRect textRect = m_textRectsInSelectionRectCoordinates[i];
        textRect.move(leftBorderThickness, topBorderThickness);

        FloatRect outerPathRect = inflateRect(textRect, horizontalOutsetToCenterOfLightBorder, verticalOutsetToCenterOfLightBorder);
        FloatRect innerPathRect = inflateRect(textRect, horizontalPaddingInsideLightBorder, verticalPaddingInsideLightBorder);

        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.setShadow(FloatSize(shadowOffsetX, shadowOffsetY), shadowBlurRadius, shadowColor(), ColorSpaceSRGB);
            graphicsContext.setFillColor(lightBorderColor(), ColorSpaceDeviceRGB);
            graphicsContext.fillPath(pathWithRoundedRect(outerPathRect, cornerRadius));
        }

        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.clip(pathWithRoundedRect(innerPathRect, cornerRadius));
            RefPtr<Gradient> gradient = Gradient::create(FloatPoint(innerPathRect.x(), innerPathRect.y()), FloatPoint(innerPathRect.x(), innerPathRect.maxY()));
            gradient->addColorStop(0, gradientLightColor());
            gradient->addColorStop(1, gradientDarkColor());
            graphicsContext.setFillGradient(gradient.releaseNonNull());
            graphicsContext.fillRect(outerPathRect);
        }

        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.translate(FloatSize(roundf(leftBorderThickness), roundf(topBorderThickness)));

            IntRect contentImageRect = enclosingIntRect(m_textRectsInSelectionRectCoordinates[i]);
            m_contentImage->paint(graphicsContext, m_contentImageScaleFactor, contentImageRect.location(), contentImageRect);
        }
    }
#else
    for (auto& textRect : m_textRectsInSelectionRectCoordinates) {
        FloatRect blurRect = textRect;
        blurRect.move(flatShadowBlurRadius + flatStyleHorizontalBorder, flatShadowBlurRadius + flatStyleVerticalBorder);
        FloatRect outerPathRect = inflateRect(blurRect, flatStyleHorizontalBorder, flatStyleVerticalBorder);

        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.setShadow(FloatSize(), flatRimShadowBlurRadius, flatRimShadowColor(), ColorSpaceSRGB);
            graphicsContext.setFillColor(flatHighlightColor(), ColorSpaceSRGB);
            graphicsContext.fillRect(outerPathRect);
            graphicsContext.setShadow(FloatSize(flatShadowOffsetX, flatShadowOffsetY), flatShadowBlurRadius, flatDropShadowColor(), ColorSpaceSRGB);
            graphicsContext.fillRect(outerPathRect);
        }

        {
            GraphicsContextStateSaver stateSaver(graphicsContext);
            graphicsContext.translate(FloatSize(flatShadowBlurRadius + flatStyleHorizontalBorder, flatShadowBlurRadius + flatStyleVerticalBorder));

            IntRect contentImageRect = enclosingIntRect(textRect);
            m_contentImage->paint(graphicsContext, m_contentImageScaleFactor, contentImageRect.location(), contentImageRect);
        }
    }
#endif
}

} // namespace WebKit
