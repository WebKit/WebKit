/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007-2019 Apple Inc.
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
#include "PrintContext.h"

#include "ElementTraversal.h"
#include "GraphicsContext.h"
#include "Frame.h"
#include "FrameView.h"
#include "LengthBox.h"
#include "RenderView.h"
#include "RuntimeEnabledFeatures.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/text/StringConcatenateNumbers.h>

namespace WebCore {

PrintContext::PrintContext(Frame* frame)
    : FrameDestructionObserver(frame)
{
}

PrintContext::~PrintContext()
{
    if (m_isPrinting)
        end();
}

void PrintContext::computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight, bool allowHorizontalTiling)
{
    if (!frame())
        return;

    auto& frame = *this->frame();
    m_pageRects.clear();
    outPageHeight = 0;

    if (!frame.document() || !frame.view() || !frame.document()->renderView())
        return;

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return;
    }

    RenderView* view = frame.document()->renderView();
    const IntRect& documentRect = view->documentRect();
    FloatSize pageSize = frame.resizePageRectsKeepingRatio(FloatSize(printRect.width(), printRect.height()), FloatSize(documentRect.width(), documentRect.height()));
    float pageWidth = pageSize.width();
    float pageHeight = pageSize.height();

    outPageHeight = pageHeight; // this is the height of the page adjusted by margins
    pageHeight -= headerHeight + footerHeight;

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return;
    }

    computePageRectsWithPageSizeInternal(FloatSize(pageWidth / userScaleFactor, pageHeight / userScaleFactor), allowHorizontalTiling);
}

FloatBoxExtent PrintContext::computedPageMargin(FloatBoxExtent printMargin)
{
    if (!frame() || !frame()->document())
        return printMargin;
    if (!RuntimeEnabledFeatures::sharedFeatures().pageAtRuleSupportEnabled())
        return printMargin;
    // FIXME Currently no pseudo class is supported.
    auto style = frame()->document()->styleScope().resolver().styleForPage(0);

    float pixelToPointScaleFactor = 1 / CSSPrimitiveValue::conversionToCanonicalUnitsScaleFactor(CSSUnitType::CSS_PT);
    return { style->marginTop().isAuto() ? printMargin.top() : style->marginTop().value() * pixelToPointScaleFactor,
        style->marginRight().isAuto() ? printMargin.right() : style->marginRight().value() * pixelToPointScaleFactor,
        style->marginBottom().isAuto() ? printMargin.bottom() : style->marginBottom().value() * pixelToPointScaleFactor,
        style->marginLeft().isAuto() ? printMargin.left() : style->marginLeft().value() * pixelToPointScaleFactor };
}

FloatSize PrintContext::computedPageSize(FloatSize pageSize, FloatBoxExtent printMargin)
{
    auto computedMargin = computedPageMargin(printMargin);
    if (computedMargin == printMargin)
        return pageSize;

    auto horizontalMarginDelta = (printMargin.left() - computedMargin.left()) + (printMargin.right() - computedMargin.right()); 
    auto verticalMarginDelta = (printMargin.top() - computedMargin.top()) + (printMargin.bottom() - computedMargin.bottom());
    return { pageSize.width() + horizontalMarginDelta, pageSize.height() + verticalMarginDelta };
}

void PrintContext::computePageRectsWithPageSize(const FloatSize& pageSizeInPixels, bool allowHorizontalTiling)
{
    m_pageRects.clear();
    computePageRectsWithPageSizeInternal(pageSizeInPixels, allowHorizontalTiling);
}

void PrintContext::computePageRectsWithPageSizeInternal(const FloatSize& pageSizeInPixels, bool allowInlineDirectionTiling)
{
    if (!frame())
        return;

    auto& frame = *this->frame();
    if (!frame.document() || !frame.view() || !frame.document()->renderView())
        return;

    RenderView* view = frame.document()->renderView();

    IntRect docRect = view->documentRect();

    int pageWidth = pageSizeInPixels.width();
    int pageHeight = pageSizeInPixels.height();

    bool isHorizontal = view->style().isHorizontalWritingMode();

    int docLogicalHeight = isHorizontal ? docRect.height() : docRect.width();
    int pageLogicalHeight = isHorizontal ? pageHeight : pageWidth;
    int pageLogicalWidth = isHorizontal ? pageWidth : pageHeight;

    int inlineDirectionStart;
    int inlineDirectionEnd;
    int blockDirectionStart;
    int blockDirectionEnd;
    if (isHorizontal) {
        if (view->style().isFlippedBlocksWritingMode()) {
            blockDirectionStart = docRect.maxY();
            blockDirectionEnd = docRect.y();
        } else {
            blockDirectionStart = docRect.y();
            blockDirectionEnd = docRect.maxY();
        }
        inlineDirectionStart = view->style().isLeftToRightDirection() ? docRect.x() : docRect.maxX();
        inlineDirectionEnd = view->style().isLeftToRightDirection() ? docRect.maxX() : docRect.x();
    } else {
        if (view->style().isFlippedBlocksWritingMode()) {
            blockDirectionStart = docRect.maxX();
            blockDirectionEnd = docRect.x();
        } else {
            blockDirectionStart = docRect.x();
            blockDirectionEnd = docRect.maxX();
        }
        inlineDirectionStart = view->style().isLeftToRightDirection() ? docRect.y() : docRect.maxY();
        inlineDirectionEnd = view->style().isLeftToRightDirection() ? docRect.maxY() : docRect.y();
    }

    unsigned pageCount = ceilf((float)docLogicalHeight / pageLogicalHeight);
    for (unsigned i = 0; i < pageCount; ++i) {
        int pageLogicalTop = blockDirectionEnd > blockDirectionStart ?
                                blockDirectionStart + i * pageLogicalHeight : 
                                blockDirectionStart - (i + 1) * pageLogicalHeight;
        if (allowInlineDirectionTiling) {
            for (int currentInlinePosition = inlineDirectionStart;
                 inlineDirectionEnd > inlineDirectionStart ? currentInlinePosition < inlineDirectionEnd : currentInlinePosition > inlineDirectionEnd;
                 currentInlinePosition += (inlineDirectionEnd > inlineDirectionStart ? pageLogicalWidth : -pageLogicalWidth)) {
                int pageLogicalLeft = inlineDirectionEnd > inlineDirectionStart ? currentInlinePosition : currentInlinePosition - pageLogicalWidth;
                IntRect pageRect(pageLogicalLeft, pageLogicalTop, pageLogicalWidth, pageLogicalHeight);
                if (!isHorizontal)
                    pageRect = pageRect.transposedRect();
                m_pageRects.append(pageRect);
            }
        } else {
            int pageLogicalLeft = inlineDirectionEnd > inlineDirectionStart ? inlineDirectionStart : inlineDirectionStart - pageLogicalWidth;
            IntRect pageRect(pageLogicalLeft, pageLogicalTop, pageLogicalWidth, pageLogicalHeight);
            if (!isHorizontal)
                pageRect = pageRect.transposedRect();
            m_pageRects.append(pageRect);
        }
    }
}

void PrintContext::begin(float width, float height)
{
    if (!frame())
        return;

    auto& frame = *this->frame();
    // This function can be called multiple times to adjust printing parameters without going back to screen mode.
    m_isPrinting = true;

    FloatSize originalPageSize = FloatSize(width, height);
    FloatSize minLayoutSize = frame.resizePageRectsKeepingRatio(originalPageSize, FloatSize(width * minimumShrinkFactor(), height * minimumShrinkFactor()));

    // This changes layout, so callers need to make sure that they don't paint to screen while in printing mode.
    frame.setPrinting(true, minLayoutSize, originalPageSize, maximumShrinkFactor() / minimumShrinkFactor(), AdjustViewSize);
}

float PrintContext::computeAutomaticScaleFactor(const FloatSize& availablePaperSize)
{
    if (!frame())
        return 1;

    auto& frame = *this->frame();
    if (!frame.view())
        return 1;

    bool useViewWidth = true;
    if (frame.document() && frame.document()->renderView())
        useViewWidth = frame.document()->renderView()->style().isHorizontalWritingMode();

    float viewLogicalWidth = useViewWidth ? frame.view()->contentsWidth() : frame.view()->contentsHeight();
    if (viewLogicalWidth < 1)
        return 1;

    float maxShrinkToFitScaleFactor = 1 / maximumShrinkFactor();
    float shrinkToFitScaleFactor = (useViewWidth ? availablePaperSize.width() : availablePaperSize.height()) / viewLogicalWidth;
    return std::max(maxShrinkToFitScaleFactor, shrinkToFitScaleFactor);
}

void PrintContext::spoolPage(GraphicsContext& ctx, int pageNumber, float width)
{
    if (!frame())
        return;

    auto& frame = *this->frame();
    if (!frame.view())
        return;

    // FIXME: Not correct for vertical text.
    IntRect pageRect = m_pageRects[pageNumber];
    float scale = width / pageRect.width();

    ctx.save();
    ctx.scale(scale);
    ctx.translate(-pageRect.x(), -pageRect.y());
    ctx.clip(pageRect);
    frame.view()->paintContents(ctx, pageRect);
    outputLinkedDestinations(ctx, *frame.document(), pageRect);
    ctx.restore();
}

void PrintContext::spoolRect(GraphicsContext& ctx, const IntRect& rect)
{
    if (!frame())
        return;

    auto& frame = *this->frame();
    if (!frame.view())
        return;

    // FIXME: Not correct for vertical text.
    ctx.save();
    ctx.translate(-rect.x(), -rect.y());
    ctx.clip(rect);
    frame.view()->paintContents(ctx, rect);
    outputLinkedDestinations(ctx, *frame.document(), rect);
    ctx.restore();
}

void PrintContext::end()
{
    if (!frame())
        return;

    auto& frame = *this->frame();
    ASSERT(m_isPrinting);
    m_isPrinting = false;
    frame.setPrinting(false, FloatSize(), FloatSize(), 0, AdjustViewSize);
    m_linkedDestinations = nullptr;
}

static inline RenderBoxModelObject* enclosingBoxModelObject(RenderElement* renderer)
{
    while (renderer && !is<RenderBoxModelObject>(*renderer))
        renderer = renderer->parent();
    return downcast<RenderBoxModelObject>(renderer);
}

int PrintContext::pageNumberForElement(Element* element, const FloatSize& pageSizeInPixels)
{
    // Make sure the element is not freed during the layout.
    RefPtr<Element> elementRef(element);
    element->document().updateLayout();

    auto* box = enclosingBoxModelObject(element->renderer());
    if (!box)
        return -1;

    Frame* frame = element->document().frame();
    FloatRect pageRect(FloatPoint(0, 0), pageSizeInPixels);
    PrintContext printContext(frame);
    printContext.begin(pageRect.width(), pageRect.height());
    FloatSize scaledPageSize = pageSizeInPixels;
    scaledPageSize.scale(frame->view()->contentsSize().width() / pageRect.width());
    printContext.computePageRectsWithPageSize(scaledPageSize, false);

    int top = roundToInt(box->offsetTop());
    int left = roundToInt(box->offsetLeft());
    size_t pageNumber = 0;
    for (; pageNumber < printContext.pageCount(); pageNumber++) {
        const IntRect& page = printContext.pageRect(pageNumber);
        if (page.x() <= left && left < page.maxX() && page.y() <= top && top < page.maxY())
            return pageNumber;
    }
    return -1;
}

void PrintContext::collectLinkedDestinations(Document& document)
{
    for (Element* child = document.documentElement(); child; child = ElementTraversal::next(*child)) {
        String outAnchorName;
        if (Element* element = child->findAnchorElementForLink(outAnchorName))
            m_linkedDestinations->add(outAnchorName, *element);
    }
}

void PrintContext::outputLinkedDestinations(GraphicsContext& graphicsContext, Document& document, const IntRect& pageRect)
{
    if (!graphicsContext.supportsInternalLinks())
        return;

    if (!m_linkedDestinations) {
        m_linkedDestinations = makeUnique<HashMap<String, Ref<Element>>>();
        collectLinkedDestinations(document);
    }

    for (const auto& it : *m_linkedDestinations) {
        RenderElement* renderer = it.value->renderer();
        if (!renderer)
            continue;

        FloatPoint point = renderer->absoluteAnchorRect().minXMinYCorner();
        point = point.expandedTo(FloatPoint());

        if (!pageRect.contains(roundedIntPoint(point)))
            continue;

        graphicsContext.addDestinationAtPoint(it.key, point);
    }
}

String PrintContext::pageProperty(Frame* frame, const char* propertyName, int pageNumber)
{
    ASSERT(frame);
    ASSERT(frame->document());

    Ref<Frame> protectedFrame(*frame);

    auto& document = *frame->document();
    PrintContext printContext(frame);
    printContext.begin(800); // Any width is OK here.
    document.updateLayout();
    auto style = document.styleScope().resolver().styleForPage(pageNumber);

    // Implement formatters for properties we care about.
    if (!strcmp(propertyName, "margin-left")) {
        if (style->marginLeft().isAuto())
            return "auto"_s;
        return String::number(style->marginLeft().value());
    }
    if (!strcmp(propertyName, "line-height"))
        return String::number(style->lineHeight().value());
    if (!strcmp(propertyName, "font-size"))
        return String::number(style->fontDescription().computedPixelSize());
    if (!strcmp(propertyName, "font-family"))
        return style->fontDescription().firstFamily();
    if (!strcmp(propertyName, "size"))
        return makeString(style->pageSize().width.value(), ' ', style->pageSize().height.value());

    return makeString("pageProperty() unimplemented for: ", propertyName);
}

bool PrintContext::isPageBoxVisible(Frame* frame, int pageNumber)
{
    return frame->document()->isPageBoxVisible(pageNumber);
}

String PrintContext::pageSizeAndMarginsInPixels(Frame* frame, int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    IntSize pageSize(width, height);
    frame->document()->pageSizeAndMarginsInPixels(pageNumber, pageSize, marginTop, marginRight, marginBottom, marginLeft);

    return makeString('(', pageSize.width(), ", ", pageSize.height(), ") ", marginTop, ' ', marginRight, ' ', marginBottom, ' ', marginLeft);
}

bool PrintContext::beginAndComputePageRectsWithPageSize(Frame& frame, const FloatSize& pageSizeInPixels)
{
    if (!frame.document() || !frame.view() || !frame.document()->renderView())
        return false;

    frame.document()->updateLayout();

    begin(pageSizeInPixels.width(), pageSizeInPixels.height());
    // Account for shrink-to-fit.
    FloatSize scaledPageSize = pageSizeInPixels;
    scaledPageSize.scale(frame.view()->contentsSize().width() / pageSizeInPixels.width());
    computePageRectsWithPageSize(scaledPageSize, false);

    return true;
}

int PrintContext::numberOfPages(Frame& frame, const FloatSize& pageSizeInPixels)
{
    Ref<Frame> protectedFrame(frame);

    PrintContext printContext(&frame);
    if (!printContext.beginAndComputePageRectsWithPageSize(frame, pageSizeInPixels))
        return -1;

    return printContext.pageCount();
}

void PrintContext::spoolAllPagesWithBoundaries(Frame& frame, GraphicsContext& graphicsContext, const FloatSize& pageSizeInPixels)
{
    Ref<Frame> protectedFrame(frame);

    PrintContext printContext(&frame);
    if (!printContext.beginAndComputePageRectsWithPageSize(frame, pageSizeInPixels))
        return;

    const float pageWidth = pageSizeInPixels.width();
    const Vector<IntRect>& pageRects = printContext.pageRects();
    int totalHeight = pageRects.size() * (pageSizeInPixels.height() + 1) - 1;

    // Fill the whole background by white.
    graphicsContext.setFillColor(Color::white);
    graphicsContext.fillRect(FloatRect(0, 0, pageWidth, totalHeight));

    graphicsContext.save();

    int currentHeight = 0;
    for (size_t pageIndex = 0; pageIndex < pageRects.size(); pageIndex++) {
        // Draw a line for a page boundary if this isn't the first page.
        if (pageIndex > 0) {
#if PLATFORM(COCOA)
            int boundaryLineY = currentHeight;
#else
            int boundaryLineY = currentHeight - 1;
#endif
            graphicsContext.save();
            graphicsContext.setStrokeColor(Color::blue);
            graphicsContext.setFillColor(Color::blue);
            graphicsContext.drawLine(IntPoint(0, boundaryLineY), IntPoint(pageWidth, boundaryLineY));
            graphicsContext.restore();
        }

        graphicsContext.save();
        graphicsContext.translate(0, currentHeight);
        printContext.spoolPage(graphicsContext, pageIndex, pageWidth);
        graphicsContext.restore();

        currentHeight += pageSizeInPixels.height() + 1;
    }

    graphicsContext.restore();
}

}
