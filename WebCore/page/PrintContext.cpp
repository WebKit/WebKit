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
#include "RenderView.h"

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

    const float pageWidth = pageSizeInPixels.width();
    const float docWidth = root->layer()->width();
    const float docHeight = root->layer()->height();
    float currPageHeight = pageSizeInPixels.height();

    // always return at least one page, since empty files should print a blank page
    float printedPagesHeight = 0;
    do {
        float proposedBottom = std::min(docHeight, printedPagesHeight + pageSizeInPixels.height());
        m_frame->view()->adjustPageHeight(&proposedBottom, printedPagesHeight, proposedBottom, printedPagesHeight);
        currPageHeight = max(1.0f, proposedBottom - printedPagesHeight);
        if (allowHorizontalMultiPages) {
            for (float curWidth = 0; curWidth < docWidth; curWidth += pageWidth)
                m_pageRects.append(IntRect(curWidth, (int)printedPagesHeight, (int)pageWidth, (int)currPageHeight));
        } else
            m_pageRects.append(IntRect(0, (int)printedPagesHeight, (int)pageWidth, (int)currPageHeight));
        printedPagesHeight += currPageHeight;
    } while (printedPagesHeight < docHeight);
}

void PrintContext::begin(float width)
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
    float maxLayoutWidth = width * PrintingMaximumShrinkFactor;

    // FIXME: This will modify the rendering of the on-screen frame.
    // Could lead to flicker during printing.
    m_frame->setPrinting(true, minLayoutWidth, maxLayoutWidth, true);
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
    m_frame->setPrinting(false, 0, 0, true);
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
    printContext.begin(pageRect.width());
    printContext.computePageRectsWithPageSize(pageSizeInPixels, false);

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

int PrintContext::numberOfPages(Frame* frame, const FloatSize& pageSizeInPixels)
{
    frame->document()->updateLayout();

    FloatRect pageRect(FloatPoint(0, 0), pageSizeInPixels);
    PrintContext printContext(frame);
    printContext.begin(pageRect.width());
    printContext.computePageRectsWithPageSize(pageSizeInPixels, false);
    return printContext.pageCount();
}

}
