/*
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
 * Copyright (C) 2007 Apple Inc.
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

#include "GraphicsContext.h"
#include "Frame.h"
#include "FrameView.h"
#include "RenderLayer.h"
#include "RenderView.h"
#include <wtf/text/StringConcatenate.h>

using namespace WebCore;

namespace WebCore {

PrintContext::PrintContext(Frame* frame)
    : m_frame(frame)
    , m_isPrinting(false)
{
}

PrintContext::~PrintContext()
{
    if (m_isPrinting)
        end();
    m_pageRects.clear();
}

int PrintContext::pageCount() const
{
    return m_pageRects.size();
}

const IntRect& PrintContext::pageRect(int pageNumber) const
{
    return m_pageRects[pageNumber];
}

void PrintContext::computePageRects(const FloatRect& printRect, float headerHeight, float footerHeight, float userScaleFactor, float& outPageHeight)
{
    m_pageRects.clear();
    outPageHeight = 0;

    if (!m_frame->document() || !m_frame->view() || !m_frame->document()->renderer())
        return;

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return;
    }

    RenderView* root = toRenderView(m_frame->document()->renderer());

    float ratio = printRect.height() / printRect.width();

    float pageWidth  = (float)root->rightLayoutOverflow();
    float pageHeight = pageWidth * ratio;
    outPageHeight = pageHeight; // this is the height of the page adjusted by margins
    pageHeight -= headerHeight + footerHeight;

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return;
    }

    computePageRectsWithPageSizeInternal(FloatSize(pageWidth / userScaleFactor, pageHeight / userScaleFactor), false);
}

void PrintContext::computePageRectsWithPageSize(const FloatSize& pageSizeInPixels, bool allowHorizontalMultiPages)
{
    m_pageRects.clear();
    computePageRectsWithPageSizeInternal(pageSizeInPixels, allowHorizontalMultiPages);
}

void PrintContext::computePageRectsWithPageSizeInternal(const FloatSize& pageSizeInPixels, bool allowHorizontalMultiPages)
{
    if (!m_frame->document() || !m_frame->view() || !m_frame->document()->renderer())
        return;

    RenderView* root = toRenderView(m_frame->document()->renderer());

    int docWidth = root->layer()->width();
    int docHeight = root->layer()->height();
    int pageWidth = pageSizeInPixels.width();
    int pageHeight = pageSizeInPixels.height();

    unsigned pageCount = ceilf((float)docHeight / pageHeight);
    for (unsigned i = 0; i < pageCount; ++i) {
        if (allowHorizontalMultiPages) {
            for (int currWidth = 0; currWidth < docWidth; currWidth += pageWidth)
                m_pageRects.append(IntRect(currWidth, i * pageHeight, pageWidth, pageHeight));
        } else
            m_pageRects.append(IntRect(0, i * pageHeight, pageWidth, pageHeight));
    }
}

void PrintContext::begin(float width, float height)
{
    ASSERT(!m_isPrinting);
    m_isPrinting = true;

    // By imaging to a width a little wider than the available pixels,
    // thin pages will be scaled down a little, matching the way they
    // print in IE and Camino. This lets them use fewer sheets than they
    // would otherwise, which is presumably why other browsers do this.
    // Wide pages will be scaled down more than this.
    const float PrintingMinimumShrinkFactor = 1.25f;

    // This number determines how small we are willing to reduce the page content
    // in order to accommodate the widest line. If the page would have to be
    // reduced smaller to make the widest line fit, we just clip instead (this
    // behavior matches MacIE and Mozilla, at least)
    const float PrintingMaximumShrinkFactor = 2.0f;

    float minLayoutWidth = width * PrintingMinimumShrinkFactor;
    float minLayoutHeight = height * PrintingMinimumShrinkFactor;

    // FIXME: This will modify the rendering of the on-screen frame.
    // Could lead to flicker during printing.
    m_frame->setPrinting(true, FloatSize(minLayoutWidth, minLayoutHeight), PrintingMaximumShrinkFactor / PrintingMinimumShrinkFactor, Frame::AdjustViewSize);
}

void PrintContext::spoolPage(GraphicsContext& ctx, int pageNumber, float width)
{
    IntRect pageRect = m_pageRects[pageNumber];
    float scale = width / pageRect.width();

    ctx.save();
    ctx.scale(FloatSize(scale, scale));
    ctx.translate(-pageRect.x(), -pageRect.y());
    ctx.clip(pageRect);
    m_frame->view()->paintContents(&ctx, pageRect);
    ctx.restore();
}

void PrintContext::end()
{
    ASSERT(m_isPrinting);
    m_isPrinting = false;
    m_frame->setPrinting(false, FloatSize(), 0, Frame::AdjustViewSize);
}

static RenderBoxModelObject* enclosingBoxModelObject(RenderObject* object)
{

    while (object && !object->isBoxModelObject())
        object = object->parent();
    if (!object)
        return 0;
    return toRenderBoxModelObject(object);
}

int PrintContext::pageNumberForElement(Element* element, const FloatSize& pageSizeInPixels)
{
    // Make sure the element is not freed during the layout.
    RefPtr<Element> elementRef(element);
    element->document()->updateLayout();

    RenderBoxModelObject* box = enclosingBoxModelObject(element->renderer());
    if (!box)
        return -1;

    Frame* frame = element->document()->frame();
    FloatRect pageRect(FloatPoint(0, 0), pageSizeInPixels);
    PrintContext printContext(frame);
    printContext.begin(pageRect.width(), pageRect.height());
    FloatSize scaledPageSize = pageSizeInPixels;
    scaledPageSize.scale(frame->view()->contentsSize().width() / pageRect.width());
    printContext.computePageRectsWithPageSize(scaledPageSize, false);

    int top = box->offsetTop();
    int left = box->offsetLeft();
    int pageNumber = 0;
    for (; pageNumber < printContext.pageCount(); pageNumber++) {
        const IntRect& page = printContext.pageRect(pageNumber);
        if (page.x() <= left && left < page.right() && page.y() <= top && top < page.bottom())
            return pageNumber;
    }
    return -1;
}

String PrintContext::pageProperty(Frame* frame, const char* propertyName, int pageNumber)
{
    Document* document = frame->document();
    PrintContext printContext(frame);
    printContext.begin(800); // Any width is OK here.
    document->updateLayout();
    RefPtr<RenderStyle> style = document->styleForPage(pageNumber);

    // Implement formatters for properties we care about.
    if (!strcmp(propertyName, "margin-left")) {
        if (style->marginLeft().isAuto())
            return String("auto");
        return String::number(style->marginLeft().rawValue());
    }
    if (!strcmp(propertyName, "line-height"))
        return String::number(style->lineHeight().rawValue());
    if (!strcmp(propertyName, "font-size"))
        return String::number(style->fontDescription().computedPixelSize());
    if (!strcmp(propertyName, "font-family"))
        return style->fontDescription().family().family().string();
    if (!strcmp(propertyName, "size"))
        return makeString(String::number(style->pageSize().width().rawValue()), ' ', String::number(style->pageSize().height().rawValue()));

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

    // We don't have a makeString() function that takes up to 12 arguments, if this is a hot function, we can provide one.
    return makeString('(', String::number(pageSize.width()), ", ", String::number(pageSize.height()), ") ") +
           makeString(String::number(marginTop), ' ', String::number(marginRight), ' ', String::number(marginBottom), ' ', String::number(marginLeft));
}

int PrintContext::numberOfPages(Frame* frame, const FloatSize& pageSizeInPixels)
{
    frame->document()->updateLayout();

    FloatRect pageRect(FloatPoint(0, 0), pageSizeInPixels);
    PrintContext printContext(frame);
    printContext.begin(pageRect.width(), pageRect.height());
    // Account for shrink-to-fit.
    FloatSize scaledPageSize = pageSizeInPixels;
    scaledPageSize.scale(frame->view()->contentsSize().width() / pageRect.width());
    printContext.computePageRectsWithPageSize(scaledPageSize, false);
    return printContext.pageCount();
}

void PrintContext::spoolAllPagesWithBoundaries(Frame* frame, GraphicsContext& graphicsContext, const FloatSize& pageSizeInPixels)
{
    if (!frame->document() || !frame->view() || !frame->document()->renderer())
        return;

    frame->document()->updateLayout();

    PrintContext printContext(frame);
    printContext.begin(pageSizeInPixels.width(), pageSizeInPixels.height());

    float pageHeight;
    printContext.computePageRects(FloatRect(FloatPoint(0, 0), pageSizeInPixels), 0, 0, 1, pageHeight);

    const float pageWidth = pageSizeInPixels.width();
    const Vector<IntRect>& pageRects = printContext.pageRects();
    int totalHeight = pageRects.size() * (pageSizeInPixels.height() + 1) - 1;

    // Fill the whole background by white.
    graphicsContext.setFillColor(Color(255, 255, 255), ColorSpaceDeviceRGB);
    graphicsContext.fillRect(FloatRect(0, 0, pageWidth, totalHeight));

    graphicsContext.save();
    graphicsContext.translate(0, totalHeight);
    graphicsContext.scale(FloatSize(1, -1));

    int currentHeight = 0;
    for (size_t pageIndex = 0; pageIndex < pageRects.size(); pageIndex++) {
        // Draw a line for a page boundary if this isn't the first page.
        if (pageIndex > 0) {
            graphicsContext.save();
            graphicsContext.setStrokeColor(Color(0, 0, 255), ColorSpaceDeviceRGB);
            graphicsContext.setFillColor(Color(0, 0, 255), ColorSpaceDeviceRGB);
            graphicsContext.drawLine(IntPoint(0, currentHeight),
                                     IntPoint(pageWidth, currentHeight));
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
