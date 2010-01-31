/*
 * Copyright (C) 2006, 2007, 2008 Apple Computer, Inc.  All rights reserved.
 * Copyright (C) 2007 Alp Toker <alp@atoker.com>
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
#include "Gradient.h"

#include "CSSParser.h"
#include "GraphicsContext.h"

#include <ApplicationServices/ApplicationServices.h>

namespace WebCore {

void Gradient::platformDestroy()
{
#ifdef BUILDING_ON_TIGER
    CGShadingRelease(m_gradient);
#else
    CGGradientRelease(m_gradient);
#endif
    m_gradient = 0;
}

#ifdef BUILDING_ON_TIGER
static void gradientCallback(void* info, const CGFloat* in, CGFloat* out)
{
    float r, g, b, a;
    static_cast<const Gradient*>(info)->getColor(*in, &r, &g, &b, &a);
    out[0] = r;
    out[1] = g;
    out[2] = b;
    out[3] = a;
}

CGShadingRef Gradient::platformGradient()
{
    if (m_gradient)
        return m_gradient;

    const CGFloat intervalRanges[2] = { 0, 1 };
    const CGFloat colorComponentRanges[4 * 2] = { 0, 1, 0, 1, 0, 1, 0, 1 };
    const CGFunctionCallbacks gradientCallbacks = { 0, gradientCallback, 0 };
    RetainPtr<CGFunctionRef> colorFunction(AdoptCF, CGFunctionCreate(this, 1, intervalRanges, 4, colorComponentRanges, &gradientCallbacks));

    static CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    if (m_radial)
        m_gradient = CGShadingCreateRadial(colorSpace, m_p0, m_r0, m_p1, m_r1, colorFunction.get(), true, true);
    else
        m_gradient = CGShadingCreateAxial(colorSpace, m_p0, m_p1, colorFunction.get(), true, true);

    return m_gradient;
}
#else
CGGradientRef Gradient::platformGradient()
{
    if (m_gradient)
        return m_gradient;

    static CGColorSpaceRef colorSpace = CGColorSpaceCreateDeviceRGB();

    sortStopsIfNecessary();
    
    const int cReservedStops = 3;
    Vector<CGFloat, 4 * cReservedStops> colorComponents;
    colorComponents.reserveCapacity(m_stops.size() * 4); // RGBA components per stop

    Vector<CGFloat, cReservedStops> locations;
    locations.reserveCapacity(m_stops.size());

    for (size_t i = 0; i < m_stops.size(); ++i) {
        colorComponents.uncheckedAppend(m_stops[i].red);
        colorComponents.uncheckedAppend(m_stops[i].green);
        colorComponents.uncheckedAppend(m_stops[i].blue);
        colorComponents.uncheckedAppend(m_stops[i].alpha);

        locations.uncheckedAppend(m_stops[i].stop);
    }
    
    m_gradient = CGGradientCreateWithColorComponents(colorSpace, colorComponents.data(), locations.data(), m_stops.size());

    return m_gradient;
}
#endif

void Gradient::fill(GraphicsContext* context, const FloatRect& rect)
{
    context->clip(rect);
    paint(context);
}

void Gradient::paint(GraphicsContext* context)
{
#ifdef BUILDING_ON_TIGER
    CGContextDrawShading(context->platformContext(), platformGradient());
#else
    CGGradientDrawingOptions extendOptions = kCGGradientDrawsBeforeStartLocation | kCGGradientDrawsAfterEndLocation;
    if (m_radial)
        CGContextDrawRadialGradient(context->platformContext(), platformGradient(), m_p0, m_r0, m_p1, m_r1, extendOptions);
    else
        CGContextDrawLinearGradient(context->platformContext(), platformGradient(), m_p0, m_p1, extendOptions);
#endif
}

} //namespace
