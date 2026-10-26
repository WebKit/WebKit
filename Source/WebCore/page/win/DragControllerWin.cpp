/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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
#include "DragController.h"

#include "DataTransfer.h"
#include "Document.h"
#include "DragData.h"
#include "Element.h"
#include "Pasteboard.h"
#include "TextIndicator.h"
#include "markup.h"
#include "windows.h"

#if USE(CAIRO)
#include "FontCascade.h"
#include "GraphicsContextCairo.h"
#include "HWndDC.h"
#include "StringTruncator.h"
#include "TextRun.h"
#include "WebCoreTextRenderer.h"
#endif

namespace WebCore {

const int DragController::MaxOriginalImageArea = 1500 * 1500;
const int DragController::DragIconRightInset = 7;
const int DragController::DragIconBottomInset = 3;

const float DragController::DragImageAlpha = 0.75f;

std::optional<DragOperation> DragController::dragOperation(const DragData& dragData)
{
    // FIXME: To match the macOS behaviour we should return std::nullopt.
    // If we are a modal window, we are the drag source, or the window is an attached sheet.
    // If this can be determined from within WebCore operationForDrag can be pulled into
    // WebCore itself.
    if (dragData.containsURL() && !m_didInitiateDrag)
        return DragOperation::Copy;
    return std::nullopt;
}

bool DragController::isCopyKeyDown(const DragData&)
{
    return ::GetAsyncKeyState(VK_CONTROL);
}
    
const IntSize& DragController::maxDragImageSize()
{
    static const IntSize maxDragImageSize(200, 200);
    
    return maxDragImageSize;
}

void DragController::cleanupAfterSystemDrag()
{
}

void DragController::declareAndWriteDragImage(DataTransfer& dataTransfer, Element& element, const URL& url, const String& label)
{
    Pasteboard& pasteboard = dataTransfer.pasteboard();

    // FIXME: Do we really need this check?
    if (!pasteboard.writableDataObject())
        return;

    // Order is important here for Explorer's sake
    pasteboard.writeURLToWritableDataObject(url, label);
    pasteboard.writeImageToDataObject(element, url);
    pasteboard.writeMarkup(serializeFragment(element, SerializedNodes::SubtreeIncludingNode, nullptr, ResolveURLs::Yes));
}

#if USE(CAIRO)

constexpr float DragLabelBorderX = 4;
constexpr float DragLabelBorderY = 2;
constexpr float DragLabelRadius = 5;
constexpr float LabelBorderYOffset = 2;

constexpr float MaxDragLabelWidth = 200;
constexpr float MaxDragLabelStringWidth = (MaxDragLabelWidth - 2 * DragLabelBorderX);

constexpr float DragLinkLabelFontsize = 11;
constexpr float DragLinkURLFontSize = 10;

static FontCascade dragLabelFont(int size, bool bold)
{
    FontCascade result;
    NONCLIENTMETRICS metrics;
    metrics.cbSize = sizeof(metrics);
    SystemParametersInfo(SPI_GETNONCLIENTMETRICS, metrics.cbSize, &metrics, 0);

    FontCascadeDescription description;
    description.setWeight(bold ? boldWeightValue() : normalWeightValue());
    description.setOneFamily(metrics.lfSmCaptionFont.lfFaceName);
    description.setComputedSize((float)size);
    description.setUsedSize((float)size);
    result = FontCascade(WTF::move(description));
    result.update();
    return result;
}

std::pair<DragImageRef, RefPtr<TextIndicator>> DragController::dragImageAndTextIndicatorForLink(Element&, URL& url, const String& inLabel, float)
{
    static const FontCascade labelFont = dragLabelFont(DragLinkLabelFontsize, true);
    static const FontCascade urlFont = dragLabelFont(DragLinkURLFontSize, false);

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
    IntSize labelSize(labelFont.width(labelRun), labelFont.metricsOfPrimaryFont().intAscent() + labelFont.metricsOfPrimaryFont().intDescent());

    if (labelSize.width() > MaxDragLabelStringWidth) {
        labelSize.setWidth(MaxDragLabelStringWidth);
        clipLabelString = true;
    }

    IntSize urlStringSize;
    IntSize imageSize(labelSize.width() + DragLabelBorderX * 2, labelSize.height() + DragLabelBorderY * 2);

    if (drawURLString) {
        urlStringSize.setWidth(urlFont.width(urlRun));
        urlStringSize.setHeight(urlFont.metricsOfPrimaryFont().intAscent() + urlFont.metricsOfPrimaryFont().intDescent());
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
    GDIObject workingDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (!workingDC)
        return { };

    PlatformGraphicsContext* contextRef;
    auto image = allocImage(workingDC.get(), imageSize, &contextRef);
    if (!image)
        return { };

    ::SelectObject(workingDC.get(), image.get());
    GraphicsContextCairo context(contextRef);
    // On Mac alpha is {0.7, 0.7, 0.7, 0.8}, however we can't control alpha
    // for drag images on win, so we use 1
    constexpr auto backgroundColor = SRGBA<uint8_t> { 140, 140, 140 };
    static const IntSize radii(DragLabelRadius, DragLabelRadius);
    IntRect rect(0, 0, imageSize.width(), imageSize.height());
    context.fillRoundedRect(FloatRoundedRect(rect, radii, radii, radii, radii), backgroundColor);

    // Draw the text
    constexpr auto topColor = Color::black; // original alpha = 0.75
    constexpr auto bottomColor = Color::white.colorWithAlphaByte(127); // original alpha = 0.5
    if (drawURLString) {
        if (clipURLString)
            urlString = StringTruncator::rightTruncate(urlString, imageSize.width() - (DragLabelBorderX * 2.0f), urlFont);
        IntPoint textPos(DragLabelBorderX, imageSize.height() - (LabelBorderYOffset + urlFont.metricsOfPrimaryFont().intDescent()));
        WebCoreDrawDoubledTextAtPoint(context, urlString, textPos, urlFont, topColor, bottomColor);
    }

    if (clipLabelString)
        label = StringTruncator::rightTruncate(label, imageSize.width() - (DragLabelBorderX * 2.0f), labelFont);

    IntPoint textPos(DragLabelBorderX, DragLabelBorderY + labelFont.size());
    WebCoreDrawDoubledTextAtPoint(context, label, textPos, labelFont, topColor, bottomColor);

    deallocContext(contextRef);
    return { image.leak(), nullptr };
}

#endif // USE(CAIRO)

}
