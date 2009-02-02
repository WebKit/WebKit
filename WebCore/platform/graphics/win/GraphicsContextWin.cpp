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

#include "TransformationMatrix.h"
#include "NotImplemented.h"
#include "Path.h"
#include <wtf/MathExtras.h>

using namespace std;

namespace WebCore {

class SVGResourceImage;

bool GraphicsContext::inTransparencyLayer() const { return m_data->m_transparencyCount; }

void GraphicsContext::setShouldIncludeChildWindows(bool include)
{
    m_data->m_shouldIncludeChildWindows = include;
}

bool GraphicsContext::shouldIncludeChildWindows() const
{
    return m_data->m_shouldIncludeChildWindows;
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
    XFORM xform;
    xform.eM11 = size.width();
    xform.eM12 = 0.0f;
    xform.eM21 = 0.0f;
    xform.eM22 = size.height();
    xform.eDx = 0.0f;
    xform.eDy = 0.0f;
    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

static const double deg2rad = 0.017453292519943295769; // pi/180

void GraphicsContextPlatformPrivate::rotate(float degreesAngle)
{
    float radiansAngle = degreesAngle * deg2rad;
    float cosAngle = cosf(radiansAngle);
    float sinAngle = sinf(radiansAngle);
    XFORM xform;
    xform.eM11 = cosAngle;
    xform.eM12 = -sinAngle;
    xform.eM21 = sinAngle;
    xform.eM22 = cosAngle;
    xform.eDx = 0.0f;
    xform.eDy = 0.0f;
    ModifyWorldTransform(m_hdc, &xform, MWT_LEFTMULTIPLY);
}

void GraphicsContextPlatformPrivate::translate(float x , float y)
{
    if (!m_hdc)
        return;
    XFORM xform;
    xform.eM11 = 1.0f;
    xform.eM12 = 0.0f;
    xform.eM21 = 0.0f;
    xform.eM22 = 1.0f;
    xform.eDx = x;
    xform.eDy = y;
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
