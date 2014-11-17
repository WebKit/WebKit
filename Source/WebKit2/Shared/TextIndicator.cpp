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

#if PLATFORM(COCOA)
#include "ArgumentCodersCF.h"
#endif

#include "ArgumentDecoder.h"
#include "ArgumentEncoder.h"
#include "ShareableBitmap.h"
#include "WebCoreArgumentCoders.h"
#include "WebFrame.h"
#include "WebPage.h"
#include <WebCore/Document.h>
#include <WebCore/Frame.h>
#include <WebCore/FrameSelection.h>
#include <WebCore/FrameView.h>
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

PassRefPtr<TextIndicator> TextIndicator::create(const TextIndicator::Data& data)
{
    return adoptRef(new TextIndicator(data));
}

PassRefPtr<TextIndicator> TextIndicator::createWithRange(const Range& range)
{
    Frame* frame = range.startContainer()->document().frame();

    if (!frame)
        return nullptr;

    VisibleSelection oldSelection = frame->selection().selection();
    frame->selection().setSelection(&range);

    RefPtr<TextIndicator> indicator = TextIndicator::createWithSelectionInFrame(*WebFrame::fromCoreFrame(*frame));

    frame->selection().setSelection(oldSelection);
    
    return indicator.release();
}

PassRefPtr<TextIndicator> TextIndicator::createWithSelectionInFrame(const WebFrame& frame)
{
    Frame& coreFrame = *frame.coreFrame();
    IntRect selectionRect = enclosingIntRect(coreFrame.selection().selectionBounds());
    RefPtr<ShareableBitmap> indicatorBitmap = frame.createSelectionSnapshot();
    if (!indicatorBitmap)
        return nullptr;

    // Store the selection rect in window coordinates, to be used subsequently
    // to determine if the indicator and selection still precisely overlap.
    IntRect selectionRectInWindowCoordinates = coreFrame.view()->contentsToWindow(selectionRect);

    Vector<FloatRect> textRects;
    coreFrame.selection().getClippedVisibleTextRectangles(textRects);

    // The bounding rect of all the text rects can be different than the selection
    // rect when the selection spans multiple lines; the indicator doesn't actually
    // care where the selection highlight goes, just where the text actually is.
    FloatRect textBoundingRectInWindowCoordinates;
    Vector<FloatRect> textRectsInWindowCoordinates;
    for (const FloatRect& textRect : textRects) {
        FloatRect textRectInWindowCoordinates = coreFrame.view()->contentsToWindow(enclosingIntRect(textRect));
        textRectsInWindowCoordinates.append(textRectInWindowCoordinates);
        textBoundingRectInWindowCoordinates.unite(textRectInWindowCoordinates);
    }

    Vector<FloatRect> textRectsInBoundingRectCoordinates;
    for (auto rect : textRectsInWindowCoordinates) {
        rect.moveBy(-textBoundingRectInWindowCoordinates.location());
        textRectsInBoundingRectCoordinates.append(rect);
    }

    TextIndicator::Data data;
    data.selectionRectInWindowCoordinates = selectionRectInWindowCoordinates;
    data.textBoundingRectInWindowCoordinates = textBoundingRectInWindowCoordinates;
    data.textRectsInBoundingRectCoordinates = textRectsInBoundingRectCoordinates;
    data.contentImageScaleFactor = frame.page()->deviceScaleFactor();
    data.contentImage = indicatorBitmap;

    return TextIndicator::create(data);
}

TextIndicator::TextIndicator(const TextIndicator::Data& data)
    : m_data(data)
{
    ASSERT(m_data.contentImageScaleFactor != 1 || m_data.contentImage->size() == enclosingIntRect(m_data.selectionRectInWindowCoordinates).size());

    if (textIndicatorsForTextRectsOverlap(m_data.textRectsInBoundingRectCoordinates)) {
        m_data.textRectsInBoundingRectCoordinates[0] = unionRect(m_data.textRectsInBoundingRectCoordinates);
        m_data.textRectsInBoundingRectCoordinates.shrink(1);
    }
}

TextIndicator::~TextIndicator()
{
}

FloatRect TextIndicator::frameRect() const
{
    return outsetIndicatorRectIncludingShadow(m_data.textBoundingRectInWindowCoordinates);
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

void TextIndicator::drawContentImage(WebCore::GraphicsContext& graphicsContext, WebCore::FloatRect textRect)
{
    FloatRect imageRect = textRect;
    imageRect.move(m_data.textBoundingRectInWindowCoordinates.location() - m_data.selectionRectInWindowCoordinates.location());
    m_data.contentImage->paint(graphicsContext, m_data.contentImageScaleFactor, enclosingIntRect(textRect).location(), enclosingIntRect(imageRect));
}

void TextIndicator::draw(GraphicsContext& graphicsContext, const IntRect& /*dirtyRect*/)
{
#if ENABLE(LEGACY_TEXT_INDICATOR_STYLE)
    for (auto& textRect : m_data.textRectsInBoundingRectCoordinates) {
        FloatRect borderedTextRect = textRect;
        borderedTextRect.move(leftBorderThickness, topBorderThickness);

        FloatRect outerPathRect = inflateRect(borderedTextRect, horizontalOutsetToCenterOfLightBorder, verticalOutsetToCenterOfLightBorder);
        FloatRect innerPathRect = inflateRect(borderedTextRect, horizontalPaddingInsideLightBorder, verticalPaddingInsideLightBorder);

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

            drawContentImage(graphicsContext, textRect);
        }
    }
#else
    for (auto& textRect : m_data.textRectsInBoundingRectCoordinates) {
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

            drawContentImage(graphicsContext, textRect);
        }
    }
#endif
}

void TextIndicator::Data::encode(IPC::ArgumentEncoder& encoder) const
{
    encoder << selectionRectInWindowCoordinates;
    encoder << textBoundingRectInWindowCoordinates;
    encoder << textRectsInBoundingRectCoordinates;
    encoder << contentImageScaleFactor;

    ShareableBitmap::Handle contentImageHandle;
    if (contentImage)
        contentImage->createHandle(contentImageHandle, SharedMemory::ReadOnly);
    encoder << contentImageHandle;
}

bool TextIndicator::Data::decode(IPC::ArgumentDecoder& decoder, TextIndicator::Data& textIndicatorData)
{
    if (!decoder.decode(textIndicatorData.selectionRectInWindowCoordinates))
        return false;

    if (!decoder.decode(textIndicatorData.textBoundingRectInWindowCoordinates))
        return false;

    if (!decoder.decode(textIndicatorData.textRectsInBoundingRectCoordinates))
        return false;

    if (!decoder.decode(textIndicatorData.contentImageScaleFactor))
        return false;

    ShareableBitmap::Handle contentImageHandle;
    if (!decoder.decode(contentImageHandle))
        return false;

    if (!contentImageHandle.isNull())
        textIndicatorData.contentImage = ShareableBitmap::create(contentImageHandle, SharedMemory::ReadOnly);

    return true;
}

} // namespace WebKit
