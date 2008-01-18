/**
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 *           (C) 2006 Allan Sandfeld Jensen (kde@carewolf.com)
 *           (C) 2006 Samuel Weinig (sam.weinig@gmail.com)
 * Copyright (C) 2003, 2004, 2005, 2006 Apple Computer, Inc.
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
#include "RenderImage.h"

#include "BitmapImage.h"
#include "Document.h"
#include "GraphicsContext.h"
#include "HTMLImageElement.h"
#include "HTMLInputElement.h"
#include "HTMLMapElement.h"
#include "HTMLNames.h"
#include "HitTestResult.h"
#include "Page.h"
#include "RenderView.h"

using namespace std;

namespace WebCore {

using namespace HTMLNames;

RenderImage::RenderImage(Node* node)
    : RenderReplaced(node, IntSize(0, 0))
    , m_cachedImage(0)
    , m_isAnonymousImage(false)
{
    updateAltText();
}

RenderImage::~RenderImage()
{
    if (m_cachedImage)
        m_cachedImage->deref(this);
}

void RenderImage::setCachedImage(CachedImage* newImage)
{
    if (m_isAnonymousImage || m_cachedImage == newImage)
        return;
    if (m_cachedImage)
        m_cachedImage->deref(this);
    m_cachedImage = newImage;
    if (m_cachedImage) {
        m_cachedImage->ref(this);
        if (m_cachedImage->errorOccurred())
            imageChanged(m_cachedImage);
    }
}

// If we'll be displaying either alt text or an image, add some padding.
static const unsigned short paddingWidth = 4;
static const unsigned short paddingHeight = 4;

// Alt text is restricted to this maximum size, in pixels.  These are
// signed integers because they are compared with other signed values.
static const int maxAltTextWidth = 1024;
static const int maxAltTextHeight = 256;

// Sets the image height and width to fit the alt text.  Returns true if the
// image size changed.
bool RenderImage::setImageSizeForAltText(CachedImage* newImage /* = 0 */)
{
    int imageWidth = 0;
    int imageHeight = 0;
  
    // If we'll be displaying either text or an image, add a little padding.
    if (!m_altText.isEmpty() || newImage) {
        imageWidth = paddingWidth;
        imageHeight = paddingHeight;
    }
  
    if (newImage) {
        imageWidth += newImage->image()->width();
        imageHeight += newImage->image()->height();
    }
  
    // we have an alt and the user meant it (its not a text we invented)
    if (!m_altText.isEmpty()) {
        const Font& font = style()->font();
        imageWidth = max(imageWidth, min(font.width(TextRun(m_altText.characters(), m_altText.length())), maxAltTextWidth));
        imageHeight = max(imageHeight, min(font.height(), maxAltTextHeight));
    }
  
    IntSize imageSize = IntSize(imageWidth, imageHeight);
    if (imageSize == intrinsicSize())
        return false;

    setIntrinsicSize(imageSize);
    return true;
}

void RenderImage::imageChanged(CachedImage* newImage)
{
    if (documentBeingDestroyed())
        return;

    if (hasBoxDecorations())
        RenderReplaced::imageChanged(newImage);
    
    if (newImage != m_cachedImage)
        return;

    bool imageSizeChanged = false;

    // Set image dimensions, taking into account the size of the alt text.
    if (newImage->errorOccurred())
        imageSizeChanged = setImageSizeForAltText(newImage);
    
    bool shouldRepaint = true;

    // Image dimensions have been changed, see what needs to be done
    if (newImage->imageSize() != intrinsicSize() || imageSizeChanged) {
        if (!newImage->errorOccurred())
            setIntrinsicSize(newImage->imageSize());

        // In the case of generated image content using :before/:after, we might not be in the
        // render tree yet.  In that case, we don't need to worry about check for layout, since we'll get a
        // layout when we get added in to the render tree hierarchy later.
        if (containingBlock()) {
            // lets see if we need to relayout at all..
            int oldwidth = m_width;
            int oldheight = m_height;
            if (!prefWidthsDirty())
                setPrefWidthsDirty(true);
            calcWidth();
            calcHeight();

            if (imageSizeChanged || m_width != oldwidth || m_height != oldheight) {
                shouldRepaint = false;
                if (!selfNeedsLayout())
                    setNeedsLayout(true);
            }

            m_width = oldwidth;
            m_height = oldheight;
        }
    }

    if (shouldRepaint)
        // FIXME: We always just do a complete repaint, since we always pass in the full image
        // rect at the moment anyway.
        repaintRectangle(contentBox());
}

void RenderImage::resetAnimation()
{
    if (m_cachedImage) {
        image()->resetAnimation();
        if (!needsLayout())
            repaint();
    }
}

void RenderImage::paintReplaced(PaintInfo& paintInfo, int tx, int ty)
{
    int cWidth = contentWidth();
    int cHeight = contentHeight();
    int leftBorder = borderLeft();
    int topBorder = borderTop();
    int leftPad = paddingLeft();
    int topPad = paddingTop();

    if (document()->printing() && !view()->printImages())
        return;

    GraphicsContext* context = paintInfo.context;

    if (!m_cachedImage || errorOccurred()) {
        if (paintInfo.phase == PaintPhaseSelection)
            return;

        if (cWidth > 2 && cHeight > 2) {
            // Draw an outline rect where the image should be.
            context->setStrokeStyle(SolidStroke);
            context->setStrokeColor(Color::lightGray);
            context->setFillColor(Color::transparent);
            context->drawRect(IntRect(tx + leftBorder + leftPad, ty + topBorder + topPad, cWidth, cHeight));

            bool errorPictureDrawn = false;
            int imageX = 0;
            int imageY = 0;
            // When calculating the usable dimensions, exclude the pixels of
            // the ouline rect so the error image/alt text doesn't draw on it.
            int usableWidth = cWidth - 2;
            int usableHeight = cHeight - 2;

            if (errorOccurred() && !image()->isNull() && (usableWidth >= image()->width()) && (usableHeight >= image()->height())) {
                // Center the error image, accounting for border and padding.
                int centerX = (usableWidth - image()->width()) / 2;
                if (centerX < 0)
                    centerX = 0;
                int centerY = (usableHeight - image()->height()) / 2;
                if (centerY < 0)
                    centerY = 0;
                imageX = leftBorder + leftPad + centerX + 1;
                imageY = topBorder + topPad + centerY + 1;
                context->drawImage(image(), IntPoint(tx + imageX, ty + imageY));
                errorPictureDrawn = true;
            }

            if (!m_altText.isEmpty()) {
                DeprecatedString text = m_altText.deprecatedString();
                text.replace('\\', backslashAsCurrencySymbol());
                context->setFont(style()->font());
                context->setFillColor(style()->color());
                int ax = tx + leftBorder + leftPad;
                int ay = ty + topBorder + topPad;
                const Font& font = style()->font();
                int ascent = font.ascent();

                // Only draw the alt text if it'll fit within the content box,
                // and only if it fits above the error image.
                TextRun textRun(reinterpret_cast<const UChar*>(text.unicode()), text.length());
                int textWidth = font.width(textRun);
                if (errorPictureDrawn) {
                    if (usableWidth >= textWidth && font.height() <= imageY)
                        context->drawText(textRun, IntPoint(ax, ay + ascent));
                } else if (usableWidth >= textWidth && cHeight >= font.height())
                    context->drawText(textRun, IntPoint(ax, ay + ascent));
            }
        }
    } else if (m_cachedImage && !image()->isNull()) {
#if PLATFORM(MAC)
        if (style()->highlight() != nullAtom && !paintInfo.context->paintingDisabled())
            paintCustomHighlight(tx - m_x, ty - m_y, style()->highlight(), true);
#endif

        IntRect rect(IntPoint(tx + leftBorder + leftPad, ty + topBorder + topPad), IntSize(cWidth, cHeight));

        HTMLImageElement* imageElt = (element() && element()->hasTagName(imgTag)) ? static_cast<HTMLImageElement*>(element()) : 0;
        CompositeOperator compositeOperator = imageElt ? imageElt->compositeOperator() : CompositeSourceOver;
        context->drawImage(image(), rect, compositeOperator, document()->page()->inLowQualityImageInterpolationMode());
    }
}

int RenderImage::minimumReplacedHeight() const
{
    return errorOccurred() ? intrinsicSize().height() : 0;
}

HTMLMapElement* RenderImage::imageMap()
{
    HTMLImageElement* i = element() && element()->hasTagName(imgTag) ? static_cast<HTMLImageElement*>(element()) : 0;
    return i ? i->document()->getImageMap(i->imageMap()) : 0;
}

bool RenderImage::nodeAtPoint(const HitTestRequest& request, HitTestResult& result, int _x, int _y, int _tx, int _ty, HitTestAction hitTestAction)
{
    bool inside = RenderReplaced::nodeAtPoint(request, result, _x, _y, _tx, _ty, hitTestAction);

    if (inside && element()) {
        int tx = _tx + m_x;
        int ty = _ty + m_y;
        
        HTMLMapElement* map = imageMap();
        if (map) {
            // we're a client side image map
            inside = map->mapMouseEvent(_x - tx, _y - ty, IntSize(contentWidth(), contentHeight()), result);
            result.setInnerNonSharedNode(element());
        }
    }

    return inside;
}

void RenderImage::updateAltText()
{
    if (!element())
        return;

    if (element()->hasTagName(inputTag))
        m_altText = static_cast<HTMLInputElement*>(element())->altText();
    else if (element()->hasTagName(imgTag))
        m_altText = static_cast<HTMLImageElement*>(element())->altText();
}

bool RenderImage::isWidthSpecified() const
{
    switch (style()->width().type()) {
        case Fixed:
        case Percent:
            return true;
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true?
        case Static:
        case Intrinsic:
        case MinIntrinsic:
            return false;
    }
    ASSERT(false);
    return false;
}

bool RenderImage::isHeightSpecified() const
{
    switch (style()->height().type()) {
        case Fixed:
        case Percent:
            return true;
        case Auto:
        case Relative: // FIXME: Shouldn't this case return true?
        case Static:
        case Intrinsic:
        case MinIntrinsic:
            return false;
    }
    ASSERT(false);
    return false;
}

int RenderImage::calcReplacedWidth() const
{
    if (m_cachedImage && m_cachedImage->imageHasRelativeWidth() && !m_cachedImage->usesImageContainerSize())
        if (RenderObject* cb = isPositioned() ? container() : containingBlock())
            m_cachedImage->setImageContainerSize(IntSize(cb->availableWidth(), cb->availableHeight()));
    
    int width;
    if (isWidthSpecified())
        width = calcReplacedWidthUsing(style()->width());
    else if (m_cachedImage && m_cachedImage->usesImageContainerSize())
        width = m_cachedImage->imageSize().width();
    else if (m_cachedImage && m_cachedImage->imageHasRelativeWidth())
        width = 0; // If the image is relatively-sized, set the width to 0 until there is a set container size.
    else
        width = calcAspectRatioWidth();

    int minW = calcReplacedWidthUsing(style()->minWidth());
    int maxW = style()->maxWidth().isUndefined() ? width : calcReplacedWidthUsing(style()->maxWidth());

    return max(minW, min(width, maxW));
}

int RenderImage::calcReplacedHeight() const
{
    int height;
    if (isHeightSpecified())
        height = calcReplacedHeightUsing(style()->height());
    else if (m_cachedImage && m_cachedImage->usesImageContainerSize())
        height = m_cachedImage->imageSize().height();
    else if (m_cachedImage && m_cachedImage->imageHasRelativeHeight())
        height = 0; // If the image is relatively-sized, set the height to 0 until there is a set container size.
    else
        height = calcAspectRatioHeight();

    int minH = calcReplacedHeightUsing(style()->minHeight());
    int maxH = style()->maxHeight().isUndefined() ? height : calcReplacedHeightUsing(style()->maxHeight());

    return max(minH, min(height, maxH));
}

int RenderImage::calcAspectRatioWidth() const
{
    IntSize size = intrinsicSize();
    if (!size.height())
        return 0;
    if (!m_cachedImage || m_cachedImage->errorOccurred())
        return size.width(); // Don't bother scaling.
    return RenderReplaced::calcReplacedHeight() * size.width() / size.height();
}

int RenderImage::calcAspectRatioHeight() const
{
    IntSize size = intrinsicSize();
    if (!size.width())
        return 0;
    if (!m_cachedImage || m_cachedImage->errorOccurred())
        return size.height(); // Don't bother scaling.
    return RenderReplaced::calcReplacedWidth() * size.height() / size.width();
}

void RenderImage::calcPrefWidths()
{
    ASSERT(prefWidthsDirty());

    m_maxPrefWidth = calcReplacedWidth() + paddingLeft() + paddingRight() + borderLeft() + borderRight();

    if (style()->width().isPercent() || style()->height().isPercent() || 
        style()->maxWidth().isPercent() || style()->maxHeight().isPercent() ||
        style()->minWidth().isPercent() || style()->minHeight().isPercent())
        m_minPrefWidth = 0;
    else
        m_minPrefWidth = m_maxPrefWidth;

    setPrefWidthsDirty(false);
}

Image* RenderImage::nullImage()
{
    static BitmapImage sharedNullImage;
    return &sharedNullImage;
}

} // namespace WebCore
