/*
 * Copyright (C) 2016 Apple Inc.  All rights reserved.
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
#include "DragImage.h"

#if USE(DIRECT2D)

#include "BitmapInfo.h"
#include "CachedImage.h"
#include "GraphicsContext.h"
#include "HWndDC.h"
#include "Image.h"
#include "NotImplemented.h"
#include "PlatformContextDirect2D.h"

#include <d2d1.h>
#include <windows.h>
#include <wtf/RetainPtr.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

void deallocContext(PlatformContextDirect2D* platformContext)
{
    if (platformContext) {
        if (auto* renderTarget = platformContext->renderTarget())
            renderTarget->Release();
    }
}

GDIObject<HBITMAP> allocImage(HDC dc, IntSize size, PlatformContextDirect2D** platformContext)
{
    BitmapInfo bmpInfo = BitmapInfo::create(size);

    LPVOID bits = nullptr;
    auto hbmp = adoptGDIObject(::CreateDIBSection(dc, &bmpInfo, DIB_RGB_COLORS, &bits, 0, 0));

    if (!platformContext || !hbmp)
        return hbmp;

    // FIXME: Use GDI Interop layer to create HBITMAP from D2D Bitmap
    notImplemented();
    return hbmp;
}

DragImageRef scaleDragImage(DragImageRef imageRef, FloatSize scale)
{
    // FIXME: due to the way drag images are done on windows we need 
    // to preprocess the alpha channel <rdar://problem/5015946>
    if (!imageRef)
        return nullptr;

    GDIObject<HBITMAP> hbmp;
    auto image = adoptGDIObject(imageRef);

    IntSize srcSize = dragImageSize(image.get());
    IntSize dstSize(static_cast<int>(srcSize.width() * scale.width()), static_cast<int>(srcSize.height() * scale.height()));

    HWndDC dc(nullptr);
    auto dstDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (dstDC)
        notImplemented();

    if (!hbmp)
        hbmp.swap(image);
    return hbmp.leak();
}

DragImageRef createDragImageFromImage(Image* img, ImageOrientation)
{
    HWndDC dc(nullptr);
    auto workingDC = adoptGDIObject(::CreateCompatibleDC(dc));
    if (!workingDC)
        return nullptr;

    notImplemented();
    return nullptr;
}
    
}

#endif
