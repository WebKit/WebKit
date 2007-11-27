/*
 * Copyright (C) 2003, 2004, 2005, 2006, 2007 Apple Inc.  All rights reserved.
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
#include "Color.h"

#if PLATFORM(CG)

#include <wtf/Assertions.h>
#include <ApplicationServices/ApplicationServices.h>

namespace WebCore {

Color::Color(CGColorRef color)
{
    if (!color) {
        m_color = 0;
        m_valid = false;
        return;
    }

    size_t numComponents = CGColorGetNumberOfComponents(color);
    const CGFloat* components = CGColorGetComponents(color);

    float r = 0;
    float g = 0;
    float b = 0;
    float a = 0;

    switch (numComponents) {
    case 2:
        r = g = b = components[0];
        a = components[1];
        break;
    case 4:
        r = components[0];
        g = components[1];
        b = components[2];
        a = components[3];
        break;
    default:
        ASSERT_NOT_REACHED();
    }

    m_color = makeRGBA(r * 255, g * 255, b * 255, a * 255);
}

#if !PLATFORM(MAC)

CGColorRef cgColor(const Color& c)
{
    CGColorRef color = NULL;
    CMProfileRef prof = NULL;
    CMGetSystemProfile(&prof);

    CGColorSpaceRef rgbSpace = CGColorSpaceCreateWithPlatformColorSpace(prof);

    if (rgbSpace != NULL)
    {
        float components[4] = {c.red() / 255.0f, c.green() / 255.0f, c.blue() / 255.0f, c.alpha() / 255.0f};
        color = CGColorCreate(rgbSpace, components);
        CGColorSpaceRelease(rgbSpace);
    }

    CMCloseProfile(prof);

    return color;
}

#endif // !PLATFORM(MAC)

}

#endif // PLATFORM(CG)
