/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#if USE(DIRECT2D)

#include "BitmapInfo.h"
#include "Frame.h"
#include "FrameSelection.h"
#include "FrameView.h"
#include "GraphicsContext.h"
#include "NotImplemented.h"
#include "RenderObject.h"
#include "Settings.h"
#include <d2d1.h>
#include <windows.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

GDIObject<HBITMAP> imageFromRect(const Frame* frame, IntRect& ir)
{
    if (!frame)
        return nullptr;

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

    notImplemented();

    SelectObject(hdc.get(), hbmpOld);

    frame->view()->setPaintBehavior(oldPaintBehavior);

    return hbmp;
}

} // namespace WebCore

#endif
