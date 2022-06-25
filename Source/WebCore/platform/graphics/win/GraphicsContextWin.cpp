/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008, 2013 Apple Inc. All rights reserved.
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
#include "GraphicsContext.h"

#if USE(CG)
#include "GraphicsContextPlatformPrivateCG.h"
#endif

#include "AffineTransform.h"
#include "BitmapInfo.h"
#include "TransformationMatrix.h"
#include "NotImplemented.h"
#include "Path.h"
#include <wtf/MathExtras.h>
#include <wtf/win/GDIObject.h>

namespace WebCore {

static void fillWithClearColor(HBITMAP bitmap)
{
    BITMAP bmpInfo;
    GetObject(bitmap, sizeof(bmpInfo), &bmpInfo);
    int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
    memset(bmpInfo.bmBits, 0, bufferSize);
}

HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend)
{
    if (!hasPlatformContext())
        return nullptr;
    HDC hdc = nullptr;
#if !USE(CAIRO)
    hdc = deprecatedPrivateContext()->m_hdc;
    if (hdc && !isInTransparencyLayer()) {
        deprecatedPrivateContext()->flush();
        deprecatedPrivateContext()->save();
        return deprecatedPrivateContext()->m_hdc;
    }
#endif
    // FIXME: Should a bitmap be created also when a shadow is set?
    if (dstRect.isEmpty())
        return 0;

    // Create a bitmap DC in which to draw.
    BitmapInfo bitmapInfo = BitmapInfo::create(dstRect.size());

    void* pixels = 0;
    HBITMAP bitmap = ::CreateDIBSection(nullptr, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
    if (!bitmap)
        return 0;

    auto bitmapDC = adoptGDIObject(::CreateCompatibleDC(hdc));
    ::SelectObject(bitmapDC.get(), bitmap);

    // Fill our buffer with clear if we're going to alpha blend.
    if (supportAlphaBlend)
        fillWithClearColor(bitmap);

    // Make sure we can do world transforms.
    ::SetGraphicsMode(bitmapDC.get(), GM_ADVANCED);

    // Apply a translation to our context so that the drawing done will be at (0,0) of the bitmap.
    XFORM xform = TransformationMatrix().translate(-dstRect.x(), -dstRect.y());

    ::SetWorldTransform(bitmapDC.get(), &xform);

    return bitmapDC.leak();
}

#if PLATFORM(WIN) && USE(CG)
void GraphicsContextPlatformPrivate::save()
{
    if (!m_hdc)
        return;
    SaveDC(m_hdc);
}

void GraphicsContextPlatformPrivate::restore()
{
    if (!m_hdc)
        return;
    RestoreDC(m_hdc, -1);
}

void GraphicsContextPlatformPrivate::clip(const FloatRect& clipRect)
{
    if (!m_hdc)
        return;
    auto clip = enclosingIntRect(clipRect);
    IntersectClipRect(m_hdc, clip.x(), clip.y(), clip.maxX(), clip.maxY());
}

void GraphicsContextPlatformPrivate::clip(const Path&)
{
    notImplemented();
}

void GraphicsContextPlatformPrivate::scale(const FloatSize& size)
{
    if (!m_hdc)
        return;

    XFORM xform = TransformationMatrix().scaleNonUniform(size.width(), size.height());
    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

void GraphicsContextPlatformPrivate::rotate(float degreesAngle)
{
    XFORM xform = TransformationMatrix().rotate(degreesAngle);
    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

void GraphicsContextPlatformPrivate::translate(float x , float y)
{
    if (!m_hdc)
        return;

    XFORM xform = TransformationMatrix().translate(x, y);
    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

void GraphicsContextPlatformPrivate::concatCTM(const AffineTransform& transform)
{
    if (!m_hdc)
        return;

    XFORM xform = transform.toTransformationMatrix();
    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

void GraphicsContextPlatformPrivate::setCTM(const AffineTransform& transform)
{
    if (!m_hdc)
        return;

    XFORM xform = transform.toTransformationMatrix();
    SetWorldTransform(m_hdc, &xform);
}
#endif

}
