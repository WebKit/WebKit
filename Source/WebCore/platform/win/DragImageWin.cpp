/*
 * Copyright (C) 2007, 2008, 2013 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "DragImage.h"

#include "Element.h"
#include "FloatRoundedRect.h"
#include "FontCascade.h"
#include "FontDescription.h"
#include "FontSelector.h"
#include "GraphicsContext.h"
#include "HWndDC.h"
#include "Image.h"
#include "StringTruncator.h"
#include "TextIndicator.h"
#include "TextRun.h"
#include "WebCoreTextRenderer.h"
#include <wtf/RetainPtr.h>
#include <wtf/URL.h>
#include <wtf/win/GDIObject.h>

#include <windows.h>

namespace WebCore {

GDIObject<HBITMAP> allocImage(HDC, IntSize, PlatformGraphicsContext** targetRef);
void deallocContext(PlatformGraphicsContext* target);

IntSize dragImageSize(DragImageRef image)
{
    if (!image)
        return IntSize();
    BITMAP b;
    GetObject(image, sizeof(BITMAP), &b);
    return IntSize(b.bmWidth, b.bmHeight);
}

void deleteDragImage(DragImageRef image)
{
    if (image)
        ::DeleteObject(image);
}

DragImageRef dissolveDragImageToFraction(DragImageRef image, float)
{
    //We don't do this on windows as the dragimage is blended by the OS
    return image;
}
        
DragImageRef createDragImageIconForCachedImageFilename(const String& filename)
{
    SHFILEINFO shfi { };
    auto fname = filename.wideCharacters();
    if (FAILED(SHGetFileInfo(fname.data(), FILE_ATTRIBUTE_NORMAL, &shfi, sizeof(shfi), SHGFI_ICON | SHGFI_USEFILEATTRIBUTES)))
        return 0;

    ICONINFO iconInfo;
    if (!GetIconInfo(shfi.hIcon, &iconInfo)) {
        DestroyIcon(shfi.hIcon);
        return 0;
    }

    DestroyIcon(shfi.hIcon);
    DeleteObject(iconInfo.hbmMask);

    return iconInfo.hbmColor;
}

const float DragLabelBorderX = 4;
// Keep border_y in synch with DragController::LinkDragBorderInset.
const float DragLabelBorderY = 2;
const float DragLabelRadius = 5;
const float LabelBorderYOffset = 2;

const float MaxDragLabelWidth = 200;
const float MaxDragLabelStringWidth = (MaxDragLabelWidth - 2 * DragLabelBorderX);

const float DragLinkLabelFontsize = 11;
const float DragLinkUrlFontSize = 10;

static FontCascade dragLabelFont(int size, bool bold, FontRenderingMode renderingMode)
{
    FontCascade result;
    NONCLIENTMETRICS metrics;
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0);

    FontCascadeDescription description;
    description.setWeight(bold ? boldWeightValue() : normalWeightValue());
    description.setOneFamily(metrics.lfSmCaptionFont.lfFaceName);
    description.setSpecifiedSize((float)size);
    description.setComputedSize((float)size);
    description.setRenderingMode(renderingMode);
    result = FontCascade(WTFMove(description), 0, 0);
    result.update(0);
    return result;
}

DragImageRef createDragImageForLink(Element&, URL& url, const String& inLabel, TextIndicatorData&, FontRenderingMode fontRenderingMode, float)
{
    // This is more or less an exact match for the Mac OS X code.

    const FontCascade* labelFont;
    const FontCascade* urlFont;

    if (fontRenderingMode == FontRenderingMode::Alternate) {
        static const FontCascade alternateRenderingModeLabelFont = dragLabelFont(DragLinkLabelFontsize, true, FontRenderingMode::Alternate);
        static const FontCascade alternateRenderingModeURLFont = dragLabelFont(DragLinkUrlFontSize, false, FontRenderingMode::Alternate);
        labelFont = &alternateRenderingModeLabelFont;
        urlFont = &alternateRenderingModeURLFont;
    } else {
        static const FontCascade normalRenderingModeLabelFont = dragLabelFont(DragLinkLabelFontsize, true, FontRenderingMode::Normal);
        static const FontCascade normalRenderingModeURLFont = dragLabelFont(DragLinkUrlFontSize, false, FontRenderingMode::Normal);
        labelFont = &normalRenderingModeLabelFont;
        urlFont = &normalRenderingModeURLFont;
    }

    bool drawURLString = true;
    bool clipURLString = false;
    bool clipLabelString = false;

    String urlString = url.string(); 
    String label = inLabel;
    if (label.isEmpty()) {
        drawURLString = false;
        label = urlString;
    }

    // First step in drawing the link drag image width.
    TextRun labelRun(label);
    TextRun urlRun(urlString);
    IntSize labelSize(labelFont->width(labelRun), labelFont->fontMetrics().ascent() + labelFont->fontMetrics().descent());

    if (labelSize.width() > MaxDragLabelStringWidth) {
        labelSize.setWidth(MaxDragLabelStringWidth);
        clipLabelString = true;
    }
    
    IntSize urlStringSize;
    IntSize imageSize(labelSize.width() + DragLabelBorderX * 2, labelSize.height() + DragLabelBorderY * 2);

    if (drawURLString) {
        urlStringSize.setWidth(urlFont->width(urlRun));
        urlStringSize.setHeight(urlFont->fontMetrics().ascent() + urlFont->fontMetrics().descent()); 
        imageSize.setHeight(imageSize.height() + urlStringSize.height());
        if (urlStringSize.width() > MaxDragLabelStringWidth) {
            imageSize.setWidth(MaxDragLabelWidth);
            clipURLString = true;
        } else
            imageSize.setWidth(std::max(labelSize.width(), urlStringSize.width()) + DragLabelBorderX * 2);
    }

    // We now know how big the image needs to be, so we create and
    // fill the background
    HWndDC dc(0);
    auto workingDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (!workingDC)
        return 0;

    PlatformGraphicsContext* contextRef;
    auto image = allocImage(workingDC.get(), imageSize, &contextRef);
    if (!image)
        return 0;
        
    ::SelectObject(workingDC.get(), image.get());
    GraphicsContext context(contextRef);
    // On Mac alpha is {0.7, 0.7, 0.7, 0.8}, however we can't control alpha
    // for drag images on win, so we use 1
    static const Color backgroundColor(140, 140, 140);
    static const IntSize radii(DragLabelRadius, DragLabelRadius);
    IntRect rect(0, 0, imageSize.width(), imageSize.height());
    context.fillRoundedRect(FloatRoundedRect(rect, radii, radii, radii, radii), backgroundColor);
 
    // Draw the text
    static const Color topColor(0, 0, 0, 255); // original alpha = 0.75
    static const Color bottomColor(255, 255, 255, 127); // original alpha = 0.5
    if (drawURLString) {
        if (clipURLString)
            urlString = StringTruncator::rightTruncate(urlString, imageSize.width() - (DragLabelBorderX * 2.0f), *urlFont);
        IntPoint textPos(DragLabelBorderX, imageSize.height() - (LabelBorderYOffset + urlFont->fontMetrics().descent()));
        WebCoreDrawDoubledTextAtPoint(context, urlString, textPos, *urlFont, topColor, bottomColor);
    }
    
    if (clipLabelString)
        label = StringTruncator::rightTruncate(label, imageSize.width() - (DragLabelBorderX * 2.0f), *labelFont);

    IntPoint textPos(DragLabelBorderX, DragLabelBorderY + labelFont->pixelSize());
    WebCoreDrawDoubledTextAtPoint(context, label, textPos, *labelFont, topColor, bottomColor);

    deallocContext(contextRef);
    return image.leak();
}

}
