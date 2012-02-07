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
#include "GraphicsTypes.h"
#include "Node.h"
#include "Page.h"
#include "Range.h"
#include "RenderBoxModelObject.h"
#include "RenderInline.h"
#include "RenderObject.h"
#include "Settings.h"
#include "StyledElement.h"
#include "TextRun.h"
#include <wtf/text/StringBuilder.h>

namespace WebCore {

namespace {

#if OS(WINDOWS)
static const unsigned fontHeightPx = 12;
#elif OS(MAC_OS_X) || OS(UNIX)
static const unsigned fontHeightPx = 11;
#endif

const static int rectInflatePx = 4;
const static int borderWidthPx = 1;
const static int tooltipPadding = 4;

const static int arrowTipOffset = 20;
const static float arrowHeight = 7;
const static float arrowHalfWidth = 7;

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

void drawOutlinedQuadWithClip(GraphicsContext& context, const FloatQuad& quad, const FloatQuad& clipQuad, const Color& fillColor)
{
    context.save();
    Path clipQuadPath = quadToPath(clipQuad);
    context.clipOut(clipQuadPath);
    drawOutlinedQuad(context, quad, fillColor, Color::transparent);
    context.restore();
}

void drawHighlightForBox(GraphicsContext& context, const FloatQuad& contentQuad, const FloatQuad& paddingQuad, const FloatQuad& borderQuad, const FloatQuad& marginQuad, HighlightData* highlightData)
{
    bool hasMargin = highlightData->margin != Color::transparent;
    bool hasBorder = highlightData->border != Color::transparent;
    bool hasPadding = highlightData->padding != Color::transparent;
    bool hasContent = highlightData->content != Color::transparent || highlightData->contentOutline != Color::transparent;

    FloatQuad clipQuad;
    Color clipColor;
    if (hasMargin && (!hasBorder || marginQuad != borderQuad)) {
        drawOutlinedQuadWithClip(context, marginQuad, borderQuad, highlightData->margin);
        clipQuad = borderQuad;
    }
    if (hasBorder && (!hasPadding || borderQuad != paddingQuad)) {
        drawOutlinedQuadWithClip(context, borderQuad, paddingQuad, highlightData->border);
        clipQuad = paddingQuad;
    }
    if (hasPadding && (!hasContent || paddingQuad != contentQuad)) {
        drawOutlinedQuadWithClip(context, paddingQuad, contentQuad, highlightData->padding);
        clipQuad = contentQuad;
    }
    if (hasContent)
        drawOutlinedQuad(context, contentQuad, highlightData->content, highlightData->contentOutline);
}

void drawHighlightForSVGRenderer(GraphicsContext& context, const Vector<FloatQuad>& absoluteQuads, HighlightData* highlightData)
{
    for (size_t i = 0; i < absoluteQuads.size(); ++i)
        drawOutlinedQuad(context, absoluteQuads[i], highlightData->content, Color::transparent);
}

inline LayoutSize frameToMainFrameOffset(Frame* frame)
{
    LayoutPoint mainFramePoint = frame->page()->mainFrame()->view()->rootViewToContents(frame->view()->contentsToRootView(LayoutPoint()));
    return toLayoutSize(mainFramePoint);
}

int drawSubstring(const TextRun& globalTextRun, int offset, int length, const Color& textColor, const Font& font, GraphicsContext& context, const LayoutRect& titleRect)
{
    context.setFillColor(textColor, ColorSpaceDeviceRGB);
    context.drawText(font, globalTextRun, LayoutPoint(titleRect.x() + rectInflatePx, titleRect.y() + font.fontMetrics().height()), offset, offset + length);
    return offset + length;
}

float calculateArrowTipX(const LayoutRect& anchorBox, const LayoutRect& titleRect)
{
    const static int anchorTipOffsetPx = 2;

    int minX = titleRect.x() + arrowHalfWidth;
    int maxX = titleRect.maxX() - arrowHalfWidth;
    int anchorX = anchorBox.x();
    int anchorMaxX = anchorBox.maxX();

    int x = titleRect.x() + arrowTipOffset; // Default tooltip position.
    if (x < anchorX)
        x = anchorX + anchorTipOffsetPx;
    else if (x > anchorMaxX)
        x = anchorMaxX - anchorTipOffsetPx;

    if (x < minX)
        x = minX;
    else if (x > maxX)
        x = maxX;

    return x;
}

void setUpFontDescription(FontDescription& fontDescription, WebCore::Settings* settings)
{
#define TOOLTIP_FONT_FAMILIES(size, ...) \
static const unsigned tooltipFontFaceSize = size;\
static const AtomicString* tooltipFontFace[size] = { __VA_ARGS__ };

#if OS(WINDOWS)
TOOLTIP_FONT_FAMILIES(2, new AtomicString("Consolas"), new AtomicString("Lucida Console"))
#elif OS(MAC_OS_X)
TOOLTIP_FONT_FAMILIES(2, new AtomicString("Menlo"), new AtomicString("Monaco"))
#elif OS(UNIX)
TOOLTIP_FONT_FAMILIES(1, new AtomicString("dejavu sans mono"))
#endif
// In the default case, we get the settings-provided monospace font.

#undef TOOLTIP_FONT_FAMILIES

    fontDescription.setRenderingMode(settings->fontRenderingMode());
    fontDescription.setComputedSize(fontHeightPx);

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

void drawElementTitle(GraphicsContext& context, Node* node, RenderObject* renderer, const LayoutRect& boundingBox, const LayoutRect& anchorBox, const FloatRect& visibleRect, WebCore::Settings* settings)
{
    DEFINE_STATIC_LOCAL(Color, backgroundColor, (255, 255, 194));
    DEFINE_STATIC_LOCAL(Color, tagColor, (136, 18, 128)); // Same as .webkit-html-tag.
    DEFINE_STATIC_LOCAL(Color, attrColor, (26, 26, 166)); // Same as .webkit-html-attribute-value.
    DEFINE_STATIC_LOCAL(Color, normalColor, (Color::black));
    DEFINE_STATIC_LOCAL(Color, pxAndBorderColor, (128, 128, 128));

    DEFINE_STATIC_LOCAL(String, pxString, ("px"));
    const static UChar timesUChar[] = { 0x0020, 0x00D7, 0x0020, 0 };
    DEFINE_STATIC_LOCAL(String, timesString, (timesUChar)); // &times; string

    FontCachePurgePreventer fontCachePurgePreventer;

    Element* element = static_cast<Element*>(node);
    bool isXHTML = element->document()->isXHTMLDocument();
    StringBuilder nodeTitle;
    nodeTitle.append(isXHTML ? element->nodeName() : element->nodeName().lower());
    unsigned tagNameLength = nodeTitle.length();

    const AtomicString& idValue = element->getIdAttribute();
    unsigned idStringLength = 0;
    String idString;
    if (!idValue.isNull() && !idValue.isEmpty()) {
        nodeTitle.append("#");
        nodeTitle.append(idValue);
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
            nodeTitle.append(".");
            nodeTitle.append(className);
            classesStringLength += 1 + className.length();
        }
    }

    RenderBoxModelObject* modelObject = renderer->isBoxModelObject() ? toRenderBoxModelObject(renderer) : 0;

    String widthNumberPart = " " + String::number(modelObject ? adjustForAbsoluteZoom(modelObject->offsetWidth(), modelObject) : boundingBox.width());
    nodeTitle.append(widthNumberPart);
    nodeTitle.append(pxString);
    nodeTitle.append(timesString);
    String heightNumberPart = String::number(modelObject ? adjustForAbsoluteZoom(modelObject->offsetHeight(), modelObject) : boundingBox.height());
    nodeTitle.append(heightNumberPart);
    nodeTitle.append(pxString);

    FontDescription desc;
    setUpFontDescription(desc, settings);
    Font font = Font(desc, 0, 0);
    font.update(0);

    TextRun nodeTitleRun(nodeTitle.toString());
    LayoutPoint titleBasePoint = LayoutPoint(anchorBox.x(), anchorBox.maxY() - 1);
    titleBasePoint.move(rectInflatePx, rectInflatePx);
    LayoutRect titleRect = enclosingLayoutRect(font.selectionRectForText(nodeTitleRun, titleBasePoint, fontHeightPx));
    titleRect.inflate(rectInflatePx);

    // The initial offsets needed to compensate for a 1px-thick border stroke (which is not a part of the rectangle).
    int dx = -borderWidthPx;
    int dy = borderWidthPx;

    // If the tip sticks beyond the right of visibleRect, right-align the tip with the said boundary.
    if (titleRect.maxX() + dx > visibleRect.maxX())
        dx = visibleRect.maxX() - titleRect.maxX();

    // If the tip sticks beyond the left of visibleRect, left-align the tip with the said boundary.
    if (titleRect.x() + dx < visibleRect.x())
        dx = visibleRect.x() - titleRect.x() - borderWidthPx;

    // If the tip sticks beyond the bottom of visibleRect, show the tip at top of bounding box.
    if (titleRect.maxY() + dy > visibleRect.maxY()) {
        dy = anchorBox.y() - titleRect.maxY() - borderWidthPx;
        // If the tip still sticks beyond the bottom of visibleRect, bottom-align the tip with the said boundary.
        if (titleRect.maxY() + dy > visibleRect.maxY())
            dy = visibleRect.maxY() - titleRect.maxY();
    }

    // If the tip sticks beyond the top of visibleRect, show the tip at top of visibleRect.
    if (titleRect.y() + dy < visibleRect.y())
        dy = visibleRect.y() - titleRect.y() + borderWidthPx;

    titleRect.move(dx, dy);

    bool isArrowAtTop = titleRect.y() > anchorBox.y();
    titleRect.move(0, tooltipPadding * (isArrowAtTop ? 1 : -1));

    {
        float arrowTipX = calculateArrowTipX(anchorBox, titleRect);
        int arrowBaseY = isArrowAtTop ? titleRect.y() : titleRect.maxY();
        int arrowOppositeY = isArrowAtTop ? titleRect.maxY() : titleRect.y();

        FloatPoint points[8];
        points[0] = FloatPoint(arrowTipX - arrowHalfWidth, arrowBaseY);
        points[1] = FloatPoint(arrowTipX, arrowBaseY + arrowHeight * (isArrowAtTop ? -1 : 1));
        points[2] = FloatPoint(arrowTipX + arrowHalfWidth, arrowBaseY);
        points[3] = FloatPoint(titleRect.maxX(), arrowBaseY);
        points[4] = FloatPoint(titleRect.maxX(), arrowOppositeY);
        points[5] = FloatPoint(titleRect.x(), arrowOppositeY);
        points[6] = FloatPoint(titleRect.x(), arrowBaseY);
        points[7] = points[0];

        Path path;
        path.moveTo(points[0]);
        for (int i = 1; i < 8; ++i)
            path.addLineTo(points[i]);

        context.save();
        context.translate(0.5f, 0.5f);
        context.setStrokeColor(pxAndBorderColor, ColorSpaceDeviceRGB);
        context.setFillColor(backgroundColor, ColorSpaceDeviceRGB);
        context.setStrokeThickness(borderWidthPx);
        context.fillPath(path);
        context.strokePath(path);
        context.restore();
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

static void getOrDrawNodeHighlight(GraphicsContext* context, HighlightData* highlightData, Highlight* highlight)
{
    Node* node = highlightData->node.get();
    RenderObject* renderer = node->renderer();
    Frame* containingFrame = node->document()->frame();

    if (!renderer || !containingFrame)
        return;

    LayoutSize mainFrameOffset = frameToMainFrameOffset(containingFrame);
    LayoutRect boundingBox = renderer->absoluteBoundingBoxRect();

    boundingBox.move(mainFrameOffset);

    LayoutRect titleAnchorBox = boundingBox;

    FrameView* view = containingFrame->page()->mainFrame()->view();
    FloatRect visibleRect = view->visibleContentRect();
    // Don't translate the context if the frame is rendered in page coordinates.
    if (context && !view->delegatesScrolling())
        context->translate(-visibleRect.x(), -visibleRect.y());

    // RenderSVGRoot should be highlighted through the isBox() code path, all other SVG elements should just dump their absoluteQuads().
#if ENABLE(SVG)
    bool isSVGRenderer = renderer->node() && renderer->node()->isSVGElement() && !renderer->isSVGRoot();
#else
    bool isSVGRenderer = false;
#endif

    if (isSVGRenderer) {
        highlight->type = HighlightTypeRects;
        renderer->absoluteQuads(highlight->quads);
        for (size_t i = 0; i < highlight->quads.size(); ++i)
            highlight->quads[i] += mainFrameOffset;

        if (context)
            drawHighlightForSVGRenderer(*context, highlight->quads, highlightData);
    } else if (renderer->isBox() || renderer->isRenderInline()) {
        LayoutRect contentBox;
        LayoutRect paddingBox;
        LayoutRect borderBox;
        LayoutRect marginBox;

        if (renderer->isBox()) {
            RenderBox* renderBox = toRenderBox(renderer);

            // RenderBox returns the "pure" content area box, exclusive of the scrollbars (if present), which also count towards the content area in CSS.
            contentBox = renderBox->contentBoxRect();
            contentBox.setWidth(contentBox.width() + renderBox->verticalScrollbarWidth());
            contentBox.setHeight(contentBox.height() + renderBox->horizontalScrollbarHeight());

            paddingBox = LayoutRect(contentBox.x() - renderBox->paddingLeft(), contentBox.y() - renderBox->paddingTop(),
                    contentBox.width() + renderBox->paddingLeft() + renderBox->paddingRight(), contentBox.height() + renderBox->paddingTop() + renderBox->paddingBottom());
            borderBox = LayoutRect(paddingBox.x() - renderBox->borderLeft(), paddingBox.y() - renderBox->borderTop(),
                    paddingBox.width() + renderBox->borderLeft() + renderBox->borderRight(), paddingBox.height() + renderBox->borderTop() + renderBox->borderBottom());
            marginBox = LayoutRect(borderBox.x() - renderBox->marginLeft(), borderBox.y() - renderBox->marginTop(),
                    borderBox.width() + renderBox->marginLeft() + renderBox->marginRight(), borderBox.height() + renderBox->marginTop() + renderBox->marginBottom());
        } else {
            RenderInline* renderInline = toRenderInline(renderer);

            // RenderInline's bounding box includes paddings and borders, excludes margins.
            borderBox = renderInline->linesBoundingBox();
            paddingBox = LayoutRect(borderBox.x() + renderInline->borderLeft(), borderBox.y() + renderInline->borderTop(),
                    borderBox.width() - renderInline->borderLeft() - renderInline->borderRight(), borderBox.height() - renderInline->borderTop() - renderInline->borderBottom());
            contentBox = LayoutRect(paddingBox.x() + renderInline->paddingLeft(), paddingBox.y() + renderInline->paddingTop(),
                    paddingBox.width() - renderInline->paddingLeft() - renderInline->paddingRight(), paddingBox.height() - renderInline->paddingTop() - renderInline->paddingBottom());
            // Ignore marginTop and marginBottom for inlines.
            marginBox = LayoutRect(borderBox.x() - renderInline->marginLeft(), borderBox.y(),
                    borderBox.width() + renderInline->marginLeft() + renderInline->marginRight(), borderBox.height());
        }

        FloatQuad absContentQuad = renderer->localToAbsoluteQuad(FloatRect(contentBox));
        FloatQuad absPaddingQuad = renderer->localToAbsoluteQuad(FloatRect(paddingBox));
        FloatQuad absBorderQuad = renderer->localToAbsoluteQuad(FloatRect(borderBox));
        FloatQuad absMarginQuad = renderer->localToAbsoluteQuad(FloatRect(marginBox));

        absContentQuad.move(mainFrameOffset);
        absPaddingQuad.move(mainFrameOffset);
        absBorderQuad.move(mainFrameOffset);
        absMarginQuad.move(mainFrameOffset);

        titleAnchorBox = absMarginQuad.enclosingBoundingBox();

        highlight->type = HighlightTypeNode;
        highlight->quads.append(absMarginQuad);
        highlight->quads.append(absBorderQuad);
        highlight->quads.append(absPaddingQuad);
        highlight->quads.append(absContentQuad);

        if (context)
            drawHighlightForBox(*context, absContentQuad, absPaddingQuad, absBorderQuad, absMarginQuad, highlightData);
    }

    // Draw node title if necessary.

    if (!node->isElementNode())
        return;

    if (context && highlightData->showInfo)
        drawElementTitle(*context, node, renderer, boundingBox, titleAnchorBox, visibleRect, containingFrame->settings());
}

static void getOrDrawRectHighlight(GraphicsContext* context, Document* document, HighlightData* highlightData, Highlight *highlight)
{
    if (!document)
        return;

    FloatRect highlightRect(*(highlightData->rect));

    highlight->type = HighlightTypeRects;
    highlight->quads.append(highlightRect);

    if (context) {
        FrameView* view = document->frame()->view();
        if (!view->delegatesScrolling()) {
            FloatRect visibleRect = view->visibleContentRect();
            context->translate(-visibleRect.x(), -visibleRect.y());
        }

        drawOutlinedQuad(*context, highlightRect, highlightData->content, highlightData->contentOutline);
    }
}

} // anonymous namespace

namespace DOMNodeHighlighter {

void drawHighlight(GraphicsContext& context, Document* document, HighlightData* highlightData)
{
    if (!highlightData)
        return;

    Highlight highlight;
    if (highlightData->node)
        getOrDrawNodeHighlight(&context, highlightData, &highlight);
    else if (highlightData->rect)
        getOrDrawRectHighlight(&context, document, highlightData, &highlight);
}

void getHighlight(Document* document, HighlightData* highlightData, Highlight* highlight)
{
    if (!highlightData)
        return;

    highlight->contentColor = highlightData->content;
    highlight->paddingColor = highlightData->padding;
    highlight->borderColor = highlightData->border;
    highlight->marginColor = highlightData->margin;
    highlight->type = HighlightTypeRects;

    if (highlightData->node)
        getOrDrawNodeHighlight(0, highlightData, highlight);
    else if (highlightData->rect)
        getOrDrawRectHighlight(0, document, highlightData, highlight);
}

} // namespace DOMNodeHighlighter

} // namespace WebCore

#endif // ENABLE(INSPECTOR)
