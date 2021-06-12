/*
 * Copyright (C) 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
#include "FrameWin.h"

#if USE(CG)

#include "BitmapInfo.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContextCG.h"
#include "RenderObject.h"
#include "Settings.h"
#include <CoreGraphics/CoreGraphics.h>
#include <windows.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

static void drawRectIntoContext(IntRect rect, FrameView* view, GraphicsContext& gc)
{
    IntPoint scrollPosition = view->scrollPosition();
    rect.move(-scrollPosition.x(), -scrollPosition.y());
    rect = view->convertToContainingWindow(rect);

    gc.concatCTM(AffineTransform().translate(-rect.x(), -rect.y()));

    view->paint(gc, rect);
}

GDIObject<HBITMAP> imageFromRect(const Frame* frame, IntRect& ir)
{
    auto oldPaintBehavior = frame->view()->paintBehavior();
    frame->view()->setPaintBehavior(oldPaintBehavior | PaintBehavior::FlattenCompositingLayers);

    void* bits = nullptr;
    auto hdc = adoptGDIObject(::CreateCompatibleDC(0));
    int w = ir.width();
    int h = ir.height();
    BitmapInfo bmp = BitmapInfo::create(IntSize(w, h));

    GDIObject<HBITMAP> hbmp = adoptGDIObject(::CreateDIBSection(0, &bmp, DIB_RGB_COLORS, static_cast<void**>(&bits), 0, 0));
    if (!hbmp)
        return hbmp;

    HGDIOBJ hbmpOld = SelectObject(hdc.get(), hbmp.get());
    auto context = adoptCF(CGBitmapContextCreate(static_cast<void*>(bits), w, h,
        8, w * sizeof(RGBQUAD), sRGBColorSpaceRef(), kCGBitmapByteOrder32Little | kCGImageAlphaPremultipliedFirst));
    CGContextSaveGState(context.get());

    GraphicsContextCG gc(context.get());

    drawRectIntoContext(ir, frame->view(), gc);

    SelectObject(hdc.get(), hbmpOld);

    frame->view()->setPaintBehavior(oldPaintBehavior);

    return hbmp;
}

} // namespace WebCore

#endif
