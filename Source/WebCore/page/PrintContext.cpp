/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007-2025 Apple Inc. All rights reserved.
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

#include "AXIsolatedTree.h"
#include "AXObjectCacheInlines.h"
#include "CommonAtomStrings.h"
#include "ContainerNodeInlines.h"
#include "DocumentView.h"
#include "ElementTraversal.h"
#include "FrameDestructionObserverInlines.h"
#include "GraphicsContext.h"
#include "LocalFrame.h"
#include "LocalFrameInlines.h"
#include "LocalFrameView.h"
#include "Logging.h"
#include "NodeDocument.h"
#include "RenderStyleInlines.h"
#include "RenderView.h"
#include "Settings.h"
#include "StyleInheritedData.h"
#include "StyleResolver.h"
#include "StyleScope.h"
#include <wtf/TZoneMallocInlines.h>
#include <wtf/text/MakeString.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PrintContext);

Ref<PrintContext> PrintContext::create(LocalFrame* frame)
{
    return adoptRef(*new PrintContext(frame));
}

PrintContext::PrintContext(LocalFrame* frame)
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

    RELEASE_LOG(Printing, "Computing page rects and clearing existing page rects. Existing page rects size = %zu", m_pageRects.size());

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
    if (!frame()->settings().pageAtRuleMarginDescriptorsEnabled())
        return printMargin;
    // FIXME Currently no pseudo class is supported.
    auto style = frame()->document()->styleScope().resolver().styleForPage(0);

    float pixelToPointScaleFactor = 1.0f / CSS::pixelsPerPt;

    auto marginTop = style->marginTop().tryFixed();
    auto marginRight = style->marginRight().tryFixed();
    auto marginBottom = style->marginBottom().tryFixed();
    auto marginLeft = style->marginLeft().tryFixed();
    const auto& zoomFactor = style->usedZoomForLength();

    return {
        marginTop ? marginTop->resolveZoom(zoomFactor) * pixelToPointScaleFactor : printMargin.top(),
        marginRight ? marginRight->resolveZoom(zoomFactor) * pixelToPointScaleFactor : printMargin.right(),
        marginBottom ? marginBottom->resolveZoom(zoomFactor) * pixelToPointScaleFactor : printMargin.bottom(),
        marginLeft ? marginLeft->resolveZoom(zoomFactor) * pixelToPointScaleFactor : printMargin.left(),
    };
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
    RELEASE_LOG(Printing, "Computing page rects with page size and clearing existing page rects. Existing page rects size = %zu", m_pageRects.size());
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

    bool isHorizontal = view->writingMode().isHorizontal();

    int docLogicalHeight = isHorizontal ? docRect.height() : docRect.width();
    int pageLogicalHeight = isHorizontal ? pageHeight : pageWidth;
    int pageLogicalWidth = isHorizontal ? pageWidth : pageHeight;

    int inlineDirectionStart;
    int inlineDirectionEnd;
    int blockDirectionStart;
    int blockDirectionEnd;
    if (isHorizontal) {
        if (view->writingMode().isBlockFlipped()) {
            blockDirectionStart = docRect.maxY();
            blockDirectionEnd = docRect.y();
        } else {
            blockDirectionStart = docRect.y();
            blockDirectionEnd = docRect.maxY();
        }
        inlineDirectionStart = view->writingMode().isInlineLeftToRight() ? docRect.x() : docRect.maxX();
        inlineDirectionEnd = view->writingMode().isInlineLeftToRight() ? docRect.maxX() : docRect.x();
    } else {
        if (view->writingMode().isBlockFlipped()) {
            blockDirectionStart = docRect.maxX();
            blockDirectionEnd = docRect.x();
        } else {
            blockDirectionStart = docRect.x();
            blockDirectionEnd = docRect.maxX();
        }
        inlineDirectionStart = view->writingMode().isInlineTopToBottom() ? docRect.y() : docRect.maxY();
        inlineDirectionEnd = view->writingMode().isInlineTopToBottom() ? docRect.maxY() : docRect.y();
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

    RELEASE_LOG(Printing, "Computed page rects with page size. Page rects count = %zu pageCount = %u", m_pageRects.size(), pageCount);
}

void PrintContext::begin(float width, float height)
{
    if (!frame())
        return;

    Ref frame = *this->frame();
    // This function can be called multiple times to adjust printing parameters without going back to screen mode.
    m_isPrinting = true;

    FloatSize originalPageSize = FloatSize(width, height);
    FloatSize minLayoutSize = frame->resizePageRectsKeepingRatio(originalPageSize, FloatSize(width * minimumShrinkFactor(), height * minimumShrinkFactor()));

    // This changes layout, so callers need to make sure that they don't paint to screen while in printing mode.
    frame->setPrinting(true, minLayoutSize, originalPageSize, maximumShrinkFactor() / minimumShrinkFactor(), AdjustViewSize::Yes);
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
        useViewWidth = frame.document()->renderView()->writingMode().isHorizontal();

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

    RELEASE_LOG(Printing, "Spooling page. pageNumber = %d pageRects size = %zu", pageNumber, m_pageRects.size());

    RELEASE_ASSERT(pageNumber < static_cast<int>(m_pageRects.size()));

    // FIXME: Not correct for vertical text.
    IntRect pageRect = m_pageRects[pageNumber];
    float scale = width / pageRect.width();

    ctx.save();
    ctx.scale(scale);
    ctx.translate(-pageRect.x(), -pageRect.y());
    ctx.clip(pageRect);
    frame.view()->paintContents(ctx, pageRect);
    outputLinkedDestinations(ctx, *frame.protectedDocument(), pageRect);
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
    frame.setPrinting(false, FloatSize(), FloatSize(), 0, AdjustViewSize::Yes);
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

    auto* frame = element->document().frame();
    FloatRect pageRect(FloatPoint(0, 0), pageSizeInPixels);
    Ref printContext = PrintContext::create(frame);
    printContext->begin(pageRect.width(), pageRect.height());
    FloatSize scaledPageSize = pageSizeInPixels;
    scaledPageSize.scale(frame->view()->contentsSize().width() / pageRect.width());
    printContext->computePageRectsWithPageSize(scaledPageSize, false);

    int top = roundToInt(box->offsetTop());
    int left = roundToInt(box->offsetLeft());
    size_t pageNumber = 0;
    for (; pageNumber < printContext->pageCount(); pageNumber++) {
        const IntRect& page = printContext->pageRect(pageNumber);
        if (page.x() <= left && left < page.maxX() && page.y() <= top && top < page.maxY())
            return pageNumber;
    }
    return -1;
}

void PrintContext::collectLinkedDestinations(Document& document)
{
    for (RefPtr child = document.documentElement(); child; child = ElementTraversal::next(*child)) {
        String outAnchorName;
        if (RefPtr element = child->findAnchorElementForLink(outAnchorName))
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

String PrintContext::pageProperty(LocalFrame* frame, const String& propertyName, int pageNumber)
{
    ASSERT(frame);
    ASSERT(frame->document());

    Ref protectedFrame { *frame };

    RefPtr document = frame->document();
    Ref printContext = PrintContext::create(frame);
    printContext->begin(800); // Any width is OK here.
    document->updateLayout();
    auto style = document->styleScope().resolver().styleForPage(pageNumber);

    // Implement formatters for properties we care about.
    if (propertyName == "margin-left"_s) {
        if (auto marginLeft = style->marginLeft().tryFixed())
            return makeString(marginLeft->resolveZoom(style->usedZoomForLength()));
        return autoAtom();
    }
    if (propertyName == "line-height"_s) {
        return WTF::switchOn(style->lineHeight(),
            [&](const CSS::Keyword::Normal&) -> String {
                return "0"_s;
            },
            [&](const Style::LineHeight::Fixed& fixed) -> String {
                return makeString(fixed.resolveZoom(style->usedZoomForLength()));
            },
            [&](const Style::LineHeight::Percentage& percentage) -> String {
                return makeString(percentage.value);
            },
            [&](const Style::LineHeight::Calc&) -> String {
                return "0"_s;
            }
        );
    }
    if (propertyName == "font-size"_s)
        return makeString(style->fontDescription().computedSize());
    if (propertyName == "font-family"_s)
        return style->fontDescription().firstFamily();
    if (propertyName == "size"_s) {
        return WTF::switchOn(style->pageSize(),
            [&](const CSS::Keyword::Auto&) -> String {
                return "auto"_s;
            },
            [&](const CSS::Keyword::Landscape&) -> String {
                return "landscape"_s;
            },
            [&](const CSS::Keyword::Portrait&) -> String {
                return "portrait"_s;
            },
            [&](const Style::PageSize::Lengths& lengths) {
                return makeString(lengths.width().resolveZoom(Style::ZoomNeeded { }), ' ', lengths.height().resolveZoom(Style::ZoomNeeded { }));
            }
        );
    }

    return makeString("pageProperty() unimplemented for: "_s, propertyName);
}

bool PrintContext::isPageBoxVisible(LocalFrame* frame, int pageNumber)
{
    return frame->document()->isPageBoxVisible(pageNumber);
}

String PrintContext::pageSizeAndMarginsInPixels(LocalFrame* frame, int pageNumber, int width, int height, int marginTop, int marginRight, int marginBottom, int marginLeft)
{
    IntSize pageSize(width, height);
    frame->document()->pageSizeAndMarginsInPixels(pageNumber, pageSize, marginTop, marginRight, marginBottom, marginLeft);

    return makeString('(', pageSize.width(), ", "_s, pageSize.height(), ") "_s, marginTop, ' ', marginRight, ' ', marginBottom, ' ', marginLeft);
}

bool PrintContext::beginAndComputePageRectsWithPageSize(LocalFrame& frame, const FloatSize& pageSizeInPixels)
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

int PrintContext::numberOfPages(LocalFrame& frame, const FloatSize& pageSizeInPixels)
{
    Ref protectedFrame { frame };

    Ref printContext = PrintContext::create(&frame);
    if (!printContext->beginAndComputePageRectsWithPageSize(frame, pageSizeInPixels))
        return -1;

    return printContext->pageCount();
}

void PrintContext::spoolAllPagesWithBoundaries(LocalFrame& frame, GraphicsContext& graphicsContext, const FloatSize& pageSizeInPixels)
{
    Ref protectedFrame { frame };

    Ref printContext = PrintContext::create(&frame);
    if (!printContext->beginAndComputePageRectsWithPageSize(frame, pageSizeInPixels))
        return;

    const float pageWidth = pageSizeInPixels.width();
    const Vector<IntRect>& pageRects = printContext->pageRects();
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
        printContext->spoolPage(graphicsContext, pageIndex, pageWidth);
        graphicsContext.restore();

        currentHeight += pageSizeInPixels.height() + 1;
    }

    graphicsContext.restore();
}

}
