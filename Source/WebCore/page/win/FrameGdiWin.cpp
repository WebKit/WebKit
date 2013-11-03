/*
 * Copyright (C) 2006, 2007 Apple Inc.  All rights reserved.
 * Copyright (C) 2007-2009 Torch Mobile, Inc.
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

#include "Frame.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include <wtf/win/GDIObject.h>

namespace WebCore {

extern HDC g_screenDC;

GDIObject<HBITMAP> imageFromRect(Frame* frame, IntRect& ir)
{
    int w;
    int h;
    FrameView* view = frame->view();
    if (view->parent()) {
        ir.setLocation(view->parent()->convertChildToSelf(view, ir.location()));
        w = ir.width() * frame->pageZoomFactor() + 0.5;
        h = ir.height() * frame->pageZoomFactor() + 0.5;
    } else {
        ir = view->contentsToWindow(ir);
        w = ir.width();
        h = ir.height();
    }

    auto bmpDC = adoptGDIObject(::CreateCompatibleDC(g_screenDC));
    auto hBmp = adoptGDIObject(::CreateCompatibleBitmap(g_screenDC, w, h));
    if (!hBmp)
        return nullptr;

    HGDIOBJ hbmpOld = ::SelectObject(bmpDC.get(), hBmp.get());

    {
        GraphicsContext gc(bmpDC.get());
        view->paint(&gc, ir);
    }

    ::SelectObject(bmpDC.get(), hbmpOld);

    return hBmp;
}

} // namespace WebCore
