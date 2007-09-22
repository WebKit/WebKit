/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "FrameWin.h"

#include <winsock2.h>
#include <windows.h>

#include "AffineTransform.h"
#include "FloatRect.h"
#include "Document.h"
#include "EditorClient.h"
#include "FrameLoader.h"
#include "FrameLoadRequest.h"
#include "FramePrivate.h"
#include "FrameView.h"
#include "HTMLIFrameElement.h"
#include "HTMLNames.h"
#include "HTMLTableCellElement.h"
#include "KeyboardEvent.h"
#include "NP_jsobject.h"
#include "Page.h"
#include "Plugin.h"
#include "PluginDatabaseWin.h"
#include "PluginViewWin.h"
#include "RegularExpression.h"
#include "RenderFrame.h"
#include "RenderTableCell.h"
#include "RenderView.h"
#include "ResourceHandle.h"
#include "TextResourceDecoder.h"
#include "kjs_proxy.h"
#include "kjs_window.h"
#include "npruntime_impl.h"
#include "runtime_root.h"
#include "GraphicsContext.h"
#include "Settings.h"

#if PLATFORM(CG)
#include <CoreGraphics/CoreGraphics.h>
#endif

using std::min;
using namespace KJS::Bindings;

namespace WebCore {

using namespace HTMLNames;

void Frame::clearPlatformScriptObjects()
{
}

KJS::Bindings::Instance* Frame::createScriptInstanceForWidget(Widget* widget)
{
    // FIXME: Ideally we'd have an isPluginView() here but we can't add that to the open source tree right now.
    if (widget->isFrameView())
        return 0;

    return static_cast<PluginViewWin*>(widget)->bindingInstance();
}


void computePageRectsForFrame(Frame* frame, const IntRect& printRect, float headerHeight, float footerHeight, float userScaleFactor,Vector<IntRect>& pages, int& outPageHeight)
{
    ASSERT(frame);

    pages.clear();
    outPageHeight = 0;

    if (!frame->document() || !frame->view() || !frame->document()->renderer())
        return;
 
    RenderView* root = static_cast<RenderView *>(frame->document()->renderer());

    if (!root) {
        LOG_ERROR("document to be printed has no renderer");
        return;
    }

    if (userScaleFactor <= 0) {
        LOG_ERROR("userScaleFactor has bad value %.2f", userScaleFactor);
        return;
    }
    
    float ratio = (float)printRect.height() / (float)printRect.width();
 
    float pageWidth  = (float) root->docWidth();
    float pageHeight = pageWidth * ratio;
    outPageHeight = (int) pageHeight;   // this is the height of the page adjusted by margins
    pageHeight -= (headerHeight + footerHeight);

    if (pageHeight <= 0) {
        LOG_ERROR("pageHeight has bad value %.2f", pageHeight);
        return;
    }

    float currPageHeight = pageHeight / userScaleFactor;
    float docHeight      = root->layer()->height();
    float docWidth       = root->layer()->width();
    float currPageWidth  = pageWidth / userScaleFactor;

    
    // always return at least one page, since empty files should print a blank page
    float printedPagesHeight = 0.0;
    do {
        float proposedBottom = min(docHeight, printedPagesHeight + pageHeight);
        frame->adjustPageHeight(&proposedBottom, printedPagesHeight, proposedBottom, printedPagesHeight);
        currPageHeight = max(1.0f, proposedBottom - printedPagesHeight);
       
        pages.append(IntRect(0,printedPagesHeight,currPageWidth,currPageHeight));
        printedPagesHeight += currPageHeight;
    } while (printedPagesHeight < docHeight);
}

static void drawRectIntoContext(IntRect rect, FrameView* view, GraphicsContext* gc)
{
    IntSize offset = view->scrollOffset();
    rect.move(-offset.width(), -offset.height());
    rect = view->convertToContainingWindow(rect);

    gc->concatCTM(AffineTransform().translate(-rect.x(), -rect.y()));

    view->paint(gc, rect);
}

HBITMAP imageFromSelection(Frame* frame, bool forceBlackText)
{
    frame->setPaintRestriction(forceBlackText ? PaintRestrictionSelectionOnlyBlackText : PaintRestrictionSelectionOnly);
    FloatRect fr = frame->selectionRect();
    IntRect ir((int)fr.x(), (int)fr.y(),(int)fr.width(),(int)fr.height());

    void* bits;
    HDC hdc = CreateCompatibleDC(0);
    int w = ir.width();
    int h = ir.height();
    BITMAPINFO bmp = { { sizeof(BITMAPINFOHEADER), w, h, 1, 32 } };

    HBITMAP hbmp = CreateDIBSection(0, &bmp, DIB_RGB_COLORS, (void**)&bits, 0, 0);
    HBITMAP hbmpOld = (HBITMAP)SelectObject(hdc, hbmp);
    CGColorSpaceRef deviceRGB = CGColorSpaceCreateDeviceRGB();
    CGContextRef context = CGBitmapContextCreate((void*)bits, w, h,
        8, w * sizeof(RGBQUAD), deviceRGB, kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst);
    CGColorSpaceRelease(deviceRGB);
    CGContextSaveGState(context);

    GraphicsContext gc(context);

    frame->document()->updateLayout();
    drawRectIntoContext(ir, frame->view(), &gc);

    CGContextRelease(context);
    SelectObject(hdc, hbmpOld);
    DeleteDC(hdc);

    frame->setPaintRestriction(PaintRestrictionNone);

    return hbmp;
}

DragImageRef Frame::dragImageForSelection()
{    
    if (selectionController()->isRange())
        return imageFromSelection(this, false);

    return 0;
}

void Frame::dashboardRegionsChanged()
{
}

} // namespace WebCore
