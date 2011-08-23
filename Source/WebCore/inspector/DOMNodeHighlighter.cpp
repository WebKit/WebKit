/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "DOMNodeHighlighter.h"

#if ENABLE(INSPECTOR)

#include "Element.h"
#include "FontCache.h"
#include "FontFamily.h"
#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "Node.h"
#include "Page.h"
#include "Range.h"
#include "RenderInline.h"
#include "Settings.h"
#include "StyledElement.h"
#include "TextRun.h"

namespace WebCore {

namespace {

const static int rectInflatePx = 4;
const static int fontHeightPx = 12;
const static int borderWidthPx = 1;
const static int tooltipPadding = 4;

const static int arrowTipOffset = 20;
const static int arrowHeight = 7;
const static int arrowHalfWidth = 7;

Path quadToPath(const FloatQuad& quad)
{
    Path quadPath;
    quadPath.moveTo(quad.p1());
    quadPath.addLineTo(quad.p2());
    quadPath.addLineTo(quad.p3());
    quadPath.addLineTo(quad.p4());
    quadPath.closeSubpath();
    return quadPath;
}

void drawOutlinedQuad(GraphicsContext& context, const FloatQuad& quad, const Color& fillColor, const Color& outlineColor)
{
    static const int outlineThickness = 2;

    Path quadPath = quadToPath(quad);

    // Clip out the quad, then draw with a 2px stroke to get a pixel
    // of outline (because inflating a quad is hard)
    {
        context.save();
        context.clipOut(quadPath);

        context.setStrokeThickness(outlineThickness);
        context.setStrokeColor(outlineColor, ColorSpaceDeviceRGB);
        context.strokePath(quadPath);

        context.restore();
    }

    // Now do the fill
    context.setFillColor(fillColor, ColorSpaceDeviceRGB);
    context.fillPath(quadPath);
}

void drawOutlinedQuadWithClip(GraphicsContext& context, const FloatQuad& quad, const FloatQuad& clipQuad, const Color& fillColor, const Color& outlineColor)
{
    context.save();
    Path clipQuadPath = quadToPath(clipQuad);
    context.clipOut(clipQuadPath);
    drawOutlinedQuad(context, quad, fillColor, outlineColor);
    context.restore();
}

void drawHighlightForBox(GraphicsContext& context, const FloatQuad& contentQuad, const FloatQuad& paddingQuad, const FloatQuad& borderQuad, const FloatQuad& marginQuad, HighlightData* highlightData)
{
    bool hasMargin = highlightData->margin != Color::transparent || highlightData->marginOutline != Color::transparent;
    bool hasBorder = highlightData->border != Color::transparent || highlightData->borderOutline != Color::transparent;
    bool hasPadding = highlightData->padding != Color::transparent || highlightData->paddingOutline != Color::transparent;
    bool hasContent = highlightData->content != Color::transparent || highlightData->contentOutline != Color::transparent;

    FloatQuad clipQuad;
    Color clipColor;
    if (hasMargin && (!hasBorder || marginQuad != borderQuad)) {
        drawOutlinedQuadWithClip(context, marginQuad, borderQuad, highlightData->margin, highlightData->marginOutline);
        clipQuad = borderQuad;
        clipColor = highlightData->marginOutline;
    }
    if (hasBorder && (!hasPadding || borderQuad != paddingQuad)) {
        drawOutlinedQuadWithClip(context, borderQuad, paddingQuad, highlightData->border, highlightData->borderOutline);
        clipQuad = paddingQuad;
        clipColor = highlightData->borderOutline;
    }
    if (hasPadding && (!hasContent || paddingQuad != contentQuad)) {
        drawOutlinedQuadWithClip(context, paddingQuad, contentQuad, highlightData->padding, highlightData->paddingOutline);
        clipQuad = contentQuad;
        clipColor = highlightData->paddingOutline;
    }
    if (hasContent)
        drawOutlinedQuad(context, contentQuad, highlightData->content, highlightData->contentOutline);
    else {
        if (clipColor.isValid())
            drawOutlinedQuadWithClip(context, clipQuad, clipQuad, clipColor, clipColor);
    }
}

void drawHighlightForLineBoxesOrSVGRenderer(GraphicsContext& context, const Vector<FloatQuad>& lineBoxQuads, HighlightData* highlightData)
{
    for (size_t i = 0; i < lineBoxQuads.size(); ++i)
        drawOutlinedQuad(context, lineBoxQuads[i], highlightData->content, highlightData->contentOutline);
}

inline LayoutSize frameToMainFrameOffset(Frame* frame)
{
    LayoutPoint mainFramePoint = frame->page()->mainFrame()->view()->windowToContents(frame->view()->contentsToWindow(LayoutPoint()));
    return toLayoutSize(mainFramePoint);
}

int drawSubstring(const TextRun& globalTextRun, int offset, int length, const Color& textColor, const Font& font, GraphicsContext& context, const LayoutRect& titleRect)
{
    context.setFillColor(textColor, ColorSpaceDeviceRGB);
    context.drawText(font, globalTextRun, LayoutPoint(titleRect.x() + rectInflatePx, titleRect.y() + font.fontMetrics().height()), offset, offset + length);
    return offset + length;
}

int calculateArrowTipX(const LayoutRect& anchorBox, const LayoutRect& titleRect)
{
    int anchorX = anchorBox.x();

    // Check for heavily misaligned tooltip first.
    if (titleRect.x() > anchorBox.maxX())
        return titleRect.x() + arrowHalfWidth;

    if (titleRect.maxX() < anchorX)
        return titleRect.maxX() - arrowHalfWidth;

    int x = titleRect.x() + arrowTipOffset;
    if (x < anchorX)
        x = anchorX + arrowTipOffset;
    if (x > titleRect.maxX() - arrowHalfWidth)
        x = titleRect.maxX() - arrowHalfWidth;

    return x;
}

void setUpFontFamilies(FontDescription& fontDescription, WebCore::Settings* settings)
{
#define TOOLTIP_FONT_FAMILIES(size, ...) \
static unsigned tooltipFontFaceSize = size;\
static const AtomicString* tooltipFontFace[size] = { __VA_ARGS__ };

#if OS(WINDOWS)
TOOLTIP_FONT_FAMILIES(2, new AtomicString("Consolas"), new AtomicString("Lucida Console"))
#elif OS(UNIX)
TOOLTIP_FONT_FAMILIES(1, new AtomicString("dejavu sans mono"))
#elif OS(MAC_OS_X)
TOOLTIP_FONT_FAMILIES(2, new AtomicString("Menlo"), new AtomicString("Monaco"))
#endif
// In the default case, we get the settings-provided monospace font.

#undef TOOLTIP_FONT_FAMILIES

    const AtomicString& fixedFontFamily = settings->fixedFontFamily();
    if (!fixedFontFamily.isEmpty()) {
        fontDescription.setGenericFamily(FontDescription::MonospaceFamily);
        FontFamily* currentFamily = 0;
        for (unsigned i = 0; i < tooltipFontFaceSize; ++i) {
            if (!currentFamily) {
                fontDescription.firstFamily().setFamily(*tooltipFontFace[i]);
                fontDescription.firstFamily().appendFamily(0);
                currentFamily = &fontDescription.firstFamily();
            } else {
                RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
                newFamily->setFamily(*tooltipFontFace[i]);
                currentFamily->appendFamily(newFamily);
                currentFamily = newFamily.get();
            }
        }
        RefPtr<SharedFontFamily> newFamily = SharedFontFamily::create();
        newFamily->setFamily(fixedFontFamily);
        currentFamily->appendFamily(newFamily);
        currentFamily = newFamily.get();
    }
}

void drawElementTitle(GraphicsContext& context, Node* node, const LayoutRect& boundingBox, const LayoutRect& anchorBox, const FloatRect& overlayRect, WebCore::Settings* settings)
{

    DEFINE_STATIC_LOCAL(Color, backgroundColor, (255, 255, 194, 255));
    DEFINE_STATIC_LOCAL(Color, tagColor, (136, 18, 128)); // Same as .webkit-html-tag.
    DEFINE_STATIC_LOCAL(Color, attrColor, (26, 26, 166)); // Same as .webkit-html-attribute-value.
    DEFINE_STATIC_LOCAL(Color, normalColor, (Color::black));
    DEFINE_STATIC_LOCAL(Color, pxAndBorderColor, (128, 128, 128));

    DEFINE_STATIC_LOCAL(String, pxString, ("px"));
    const static UChar timesUChar[] = { 0x00D7, 0 };
    DEFINE_STATIC_LOCAL(String, timesString, (timesUChar)); // &times; string

    FontCachePurgePreventer fontCachePurgePreventer;

    Element* element = static_cast<Element*>(node);
    bool isXHTML = element->document()->isXHTMLDocument();
    String nodeTitle(isXHTML ? element->nodeName() : element->nodeName().lower());
    unsigned tagNameLength = nodeTitle.length();

    const AtomicString& idValue = element->getIdAttribute();
    unsigned idStringLength = 0;
    String idString;
    if (!idValue.isNull() && !idValue.isEmpty()) {
        nodeTitle += "#";
        nodeTitle += idValue;
        idStringLength = 1 + idValue.length();
    }

    HashSet<AtomicString> usedClassNames;
    unsigned classesStringLength = 0;
    if (element->hasClass() && element->isStyledElement()) {
        const SpaceSplitString& classNamesString = static_cast<StyledElement*>(element)->classNames();
        size_t classNameCount = classNamesString.size();
        for (size_t i = 0; i < classNameCount; ++i) {
            const AtomicString& className = classNamesString[i];
            if (usedClassNames.contains(className))
                continue;
            usedClassNames.add(className);
            nodeTitle += ".";
            nodeTitle += className;
            classesStringLength += 1 + className.length();
        }
    }

    String widthNumberPart = " " + String::number(boundingBox.width());
    nodeTitle += widthNumberPart + pxString;
    nodeTitle += timesString;
    String heightNumberPart = String::number(boundingBox.height());
    nodeTitle += heightNumberPart + pxString;

    FontDescription desc;
    desc.setRenderingMode(settings->fontRenderingMode());
    desc.setComputedSize(fontHeightPx);
    setUpFontFamilies(desc, settings);
    Font font = Font(desc, 0, 0);
    font.update(0);

    TextRun nodeTitleRun(nodeTitle);
    LayoutPoint titleBasePoint = LayoutPoint(anchorBox.x(), anchorBox.maxY() - 1);
    titleBasePoint.move(rectInflatePx, rectInflatePx);
    LayoutRect titleRect = enclosingLayoutRect(font.selectionRectForText(nodeTitleRun, titleBasePoint, fontHeightPx));
    titleRect.inflate(rectInflatePx);

    // The initial offsets needed to compensate for a 1px-thick border stroke (which is not a part of the rectangle).
    LayoutUnit dx = -borderWidthPx;
    LayoutUnit dy = borderWidthPx;

    // If the tip sticks beyond the right of overlayRect, right-align the tip with the said boundary.
    if (titleRect.maxX() > overlayRect.maxX())
        dx = overlayRect.maxX() - titleRect.maxX();

    // If the tip sticks beyond the left of overlayRect, left-align the tip with the said boundary.
    if (titleRect.x() + dx < overlayRect.x())
        dx = overlayRect.x() - titleRect.x() - borderWidthPx;

    // If the tip sticks beyond the bottom of overlayRect, show the tip at top of bounding box.
    if (titleRect.maxY() > overlayRect.maxY()) {
        dy = anchorBox.y() - titleRect.maxY() - borderWidthPx;
        // If the tip still sticks beyond the bottom of overlayRect, bottom-align the tip with the said boundary.
        if (titleRect.maxY() + dy > overlayRect.maxY())
            dy = overlayRect.maxY() - titleRect.maxY();
    }

    // If the tip sticks beyond the top of overlayRect, show the tip at top of overlayRect.
    if (titleRect.y() + dy < overlayRect.y())
        dy = overlayRect.y() - titleRect.y() + borderWidthPx;

    titleRect.move(dx, dy);

    bool isArrowAtTop = titleRect.y() > anchorBox.y();
    titleRect.move(0, tooltipPadding * (isArrowAtTop ? 1 : -1));

    context.setStrokeColor(pxAndBorderColor, ColorSpaceDeviceRGB);
    context.setStrokeThickness(borderWidthPx);
    context.setFillColor(backgroundColor, ColorSpaceDeviceRGB);
    context.drawRect(titleRect);

    {
        int arrowTipX = calculateArrowTipX(anchorBox, titleRect);
        FloatPoint arrowPoints[3];
        float arrowBaseY = isArrowAtTop ? titleRect.y() : titleRect.maxY();
        arrowPoints[0] = FloatPoint(arrowTipX - arrowHalfWidth, arrowBaseY);
        arrowPoints[1] = FloatPoint(arrowTipX, arrowBaseY + arrowHeight * (isArrowAtTop ? -1 : 1));
        arrowPoints[2] = FloatPoint(arrowTipX + arrowHalfWidth, arrowBaseY);
        context.drawConvexPolygon(3, arrowPoints);

        context.setStrokeColor(backgroundColor, ColorSpaceDeviceRGB);
        context.setFillColor(backgroundColor, ColorSpaceDeviceRGB);
        context.setStrokeThickness(borderWidthPx + 1);
        LayoutPoint startPoint = LayoutPoint(arrowPoints[0].x() + 1, arrowPoints[0].y());
        LayoutPoint endPoint = LayoutPoint(arrowPoints[2].x() - 1, arrowPoints[2].y());
        context.drawLine(startPoint, endPoint);
    }

    int currentPos = 0;
    currentPos = drawSubstring(nodeTitleRun, currentPos, tagNameLength, tagColor, font, context, titleRect);
    if (idStringLength)
        currentPos = drawSubstring(nodeTitleRun, currentPos, idStringLength, attrColor, font, context, titleRect);
    if (classesStringLength)
        currentPos = drawSubstring(nodeTitleRun, currentPos, classesStringLength, attrColor, font, context, titleRect);
    currentPos = drawSubstring(nodeTitleRun, currentPos, widthNumberPart.length(), normalColor, font, context, titleRect);
    currentPos = drawSubstring(nodeTitleRun, currentPos, pxString.length() + timesString.length(), pxAndBorderColor, font, context, titleRect);
    currentPos = drawSubstring(nodeTitleRun, currentPos, heightNumberPart.length(), normalColor, font, context, titleRect);
    drawSubstring(nodeTitleRun, currentPos, pxString.length(), pxAndBorderColor, font, context, titleRect);
}

void drawNodeHighlight(GraphicsContext& context, HighlightData* highlightData)
{
    Node* node = highlightData->node.get();
    RenderObject* renderer = node->renderer();
    Frame* containingFrame = node->document()->frame();

    if (!renderer || !containingFrame)
        return;

    LayoutSize mainFrameOffset = frameToMainFrameOffset(containingFrame);
    LayoutRect boundingBox = renderer->absoluteBoundingBoxRect(true);

    boundingBox.move(mainFrameOffset);

    LayoutRect titleAnchorBox = boundingBox;

    FrameView* view = containingFrame->page()->mainFrame()->view();
    FloatRect overlayRect = view->visibleContentRect();
    if (!overlayRect.contains(boundingBox) && !boundingBox.contains(enclosingLayoutRect(overlayRect)))
        overlayRect = view->visibleContentRect();
    context.translate(-overlayRect.x(), -overlayRect.y());

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
#if ENABLE(SVG)
    bool isSVGRenderer = renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot();
#else
    bool isSVGRenderer = false;
#endif

    if (renderer->isBox() && !isSVGRenderer) {
        RenderBox* renderBox = toRenderBox(renderer);

        // RenderBox returns the "pure" content area box, exclusive of the scrollbars (if present), which also count towards the content area in CSS.
        LayoutRect contentBox = renderBox->contentBoxRect();
        contentBox.setWidth(contentBox.width() + renderBox->verticalScrollbarWidth());
        contentBox.setHeight(contentBox.height() + renderBox->horizontalScrollbarHeight());

        LayoutRect paddingBox(contentBox.x() - renderBox->paddingLeft(), contentBox.y() - renderBox->paddingTop(),
                           contentBox.width() + renderBox->paddingLeft() + renderBox->paddingRight(), contentBox.height() + renderBox->paddingTop() + renderBox->paddingBottom());
        LayoutRect borderBox(paddingBox.x() - renderBox->borderLeft(), paddingBox.y() - renderBox->borderTop(),
                          paddingBox.width() + renderBox->borderLeft() + renderBox->borderRight(), paddingBox.height() + renderBox->borderTop() + renderBox->borderBottom());
        LayoutRect marginBox(borderBox.x() - renderBox->marginLeft(), borderBox.y() - renderBox->marginTop(),
                          borderBox.width() + renderBox->marginLeft() + renderBox->marginRight(), borderBox.height() + renderBox->marginTop() + renderBox->marginBottom());

        FloatQuad absContentQuad = renderBox->localToAbsoluteQuad(FloatRect(contentBox));
        FloatQuad absPaddingQuad = renderBox->localToAbsoluteQuad(FloatRect(paddingBox));
        FloatQuad absBorderQuad = renderBox->localToAbsoluteQuad(FloatRect(borderBox));
        FloatQuad absMarginQuad = renderBox->localToAbsoluteQuad(FloatRect(marginBox));

        absContentQuad.move(mainFrameOffset);
        absPaddingQuad.move(mainFrameOffset);
        absBorderQuad.move(mainFrameOffset);
        absMarginQuad.move(mainFrameOffset);

        titleAnchorBox = absMarginQuad.enclosingBoundingBox();

        drawHighlightForBox(context, absContentQuad, absPaddingQuad, absBorderQuad, absMarginQuad, highlightData);
    } else if (renderer->isRenderInline() || isSVGRenderer) {
        // FIXME: We should show margins/padding/border for inlines.
        Vector<FloatQuad> lineBoxQuads;
        renderer->absoluteQuads(lineBoxQuads);
        for (unsigned i = 0; i < lineBoxQuads.size(); ++i)
            lineBoxQuads[i] += mainFrameOffset;

        drawHighlightForLineBoxesOrSVGRenderer(context, lineBoxQuads, highlightData);
    }

    // Draw node title if necessary.

    if (!node->isElementNode())
        return;

    if (highlightData->showInfo)
        drawElementTitle(context, node, boundingBox, titleAnchorBox, overlayRect, containingFrame->settings());
}

void drawRectHighlight(GraphicsContext& context, Document* document, HighlightData* highlightData)
{
    if (!document)
        return;
    FrameView* view = document->frame()->view();

    FloatRect overlayRect = view->visibleContentRect();
    context.translate(-overlayRect.x(), -overlayRect.y());

    static const int outlineThickness = 2;

    Path quadPath = quadToPath(FloatRect(*(highlightData->rect)));

    // Clip out the quad, then draw with a 2px stroke to get a pixel
    // of outline (because inflating a quad is hard)
    {
        context.save();
        context.clipOut(quadPath);

        context.setStrokeThickness(outlineThickness);
        context.setStrokeColor(highlightData->contentOutline, ColorSpaceDeviceRGB);
        context.strokePath(quadPath);

        context.restore();
    }

    // Now do the fill
    context.setFillColor(highlightData->content, ColorSpaceDeviceRGB);
    context.fillPath(quadPath);
}

} // anonymous namespace

namespace DOMNodeHighlighter {

void drawHighlight(GraphicsContext& context, Document* document, HighlightData* highlightData)
{
    if (!highlightData)
        return;

    if (highlightData->node)
        drawNodeHighlight(context, highlightData);
    else if (highlightData->rect)
        drawRectHighlight(context, document, highlightData);
}

} // namespace DOMNodeHighlighter

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
