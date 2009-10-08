/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All rights reserved.
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
#include "GraphicsContext.h"

#if PLATFORM(CG)
#include "GraphicsContextPlatformPrivateCG.h"
#elif PLATFORM(CAIRO)
#include "GraphicsContextPlatformPrivateCairo.h"
#endif

#include "BitmapInfo.h"
#include "TransformationMatrix.h"
#include "NotImplemented.h"
#include "Path.h"
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

class SVGResourceImage;

static void fillWithClearColor(HBITMAP bitmap)
{
    BITMAP bmpInfo;
    GetObject(bitmap, sizeof(bmpInfo), &bmpInfo);
    int bufferSize = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;
    memset(bmpInfo.bmBits, 0, bufferSize);
}

bool GraphicsContext::inTransparencyLayer() const { return m_data->m_transparencyCount; }

void GraphicsContext::setShouldIncludeChildWindows(bool include)
{
    m_data->m_shouldIncludeChildWindows = include;
}

bool GraphicsContext::shouldIncludeChildWindows() const
{
    return m_data->m_shouldIncludeChildWindows;
}

GraphicsContext::WindowsBitmap::WindowsBitmap(HDC hdc, IntSize size)
    : m_hdc(0)
    , m_size(size)
{
    BitmapInfo bitmapInfo = BitmapInfo::create(m_size);

    m_bitmap = CreateDIBSection(0, &bitmapInfo, DIB_RGB_COLORS, reinterpret_cast<void**>(&m_bitmapBuffer), 0, 0);
    if (!m_bitmap)
        return;

    m_hdc = CreateCompatibleDC(hdc);
    SelectObject(m_hdc, m_bitmap);

    BITMAP bmpInfo;
    GetObject(m_bitmap, sizeof(bmpInfo), &bmpInfo);
    m_bytesPerRow = bmpInfo.bmWidthBytes;
    m_bitmapBufferLength = bmpInfo.bmWidthBytes * bmpInfo.bmHeight;

    SetGraphicsMode(m_hdc, GM_ADVANCED);
}

GraphicsContext::WindowsBitmap::~WindowsBitmap()
{
    if (!m_bitmap)
        return;

    DeleteDC(m_hdc);
    DeleteObject(m_bitmap);
}

GraphicsContext::WindowsBitmap* GraphicsContext::createWindowsBitmap(IntSize size)
{
    return new WindowsBitmap(m_data->m_hdc, size);
}

HDC GraphicsContext::getWindowsContext(const IntRect& dstRect, bool supportAlphaBlend, bool mayCreateBitmap)
{
    // FIXME: Should a bitmap be created also when a shadow is set?
    if (mayCreateBitmap && inTransparencyLayer()) {
        if (dstRect.isEmpty())
            return 0;

        // Create a bitmap DC in which to draw.
        BitmapInfo bitmapInfo = BitmapInfo::create(dstRect.size());

        void* pixels = 0;
        HBITMAP bitmap = ::CreateDIBSection(NULL, &bitmapInfo, DIB_RGB_COLORS, &pixels, 0, 0);
        if (!bitmap)
            return 0;

        HDC bitmapDC = ::CreateCompatibleDC(m_data->m_hdc);
        ::SelectObject(bitmapDC, bitmap);

        // Fill our buffer with clear if we're going to alpha blend.
        if (supportAlphaBlend)
           fillWithClearColor(bitmap);

        // Make sure we can do world transforms.
        SetGraphicsMode(bitmapDC, GM_ADVANCED);

        // Apply a translation to our context so that the drawing done will be at (0,0) of the bitmap.
        XFORM xform = TransformationMatrix().translate(-dstRect.x(), -dstRect.y());

        ::SetWorldTransform(bitmapDC, &xform);

        return bitmapDC;
    }

    m_data->flush();
    m_data->save();
    return m_data->m_hdc;
}

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
    IntersectClipRect(m_hdc, clipRect.x(), clipRect.y(), clipRect.right(), clipRect.bottom());
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

static const double deg2rad = 0.017453292519943295769; // pi/180

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

void GraphicsContextPlatformPrivate::concatCTM(const TransformationMatrix& transform)
{
    if (!m_hdc)
        return;

    XFORM xform = transform;
    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

#if ENABLE(SVG)
GraphicsContext* contextForImage(SVGResourceImage*)
{
    // FIXME: This should go in GraphicsContextCG.cpp
    notImplemented();
    return 0;
}
#endif

}
