/*
 * Copyright (C) 2005, 2006, 2007, 2009, 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FontAntialiasingStateSaver_h
#define FontAntialiasingStateSaver_h

#if PLATFORM(IOS_FAMILY)

#import <pal/spi/cg/CoreGraphicsSPI.h>

namespace WebCore {

class FontAntialiasingStateSaver {
    WTF_MAKE_NONCOPYABLE(FontAntialiasingStateSaver);
public:
    FontAntialiasingStateSaver(CGContextRef context, bool useOrientationDependentFontAntialiasing)
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
        : m_context(context)
        , m_useOrientationDependentFontAntialiasing(useOrientationDependentFontAntialiasing)
#endif
    {
#if PLATFORM(IOS_FAMILY_SIMULATOR)
        UNUSED_PARAM(context);
        UNUSED_PARAM(useOrientationDependentFontAntialiasing);
#endif
    }

    ~FontAntialiasingStateSaver()
    {
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
        if (m_useOrientationDependentFontAntialiasing)
            CGContextSetFontAntialiasingStyle(m_context, m_oldAntialiasingStyle);
#endif
    }

    void setup(bool isLandscapeOrientation)
    {
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    m_oldAntialiasingStyle = CGContextGetFontAntialiasingStyle(m_context);

    if (m_useOrientationDependentFontAntialiasing)
        CGContextSetFontAntialiasingStyle(m_context, isLandscapeOrientation ? kCGFontAntialiasingStyleFilterLight : kCGFontAntialiasingStyleUnfiltered);
#else
    UNUSED_PARAM(isLandscapeOrientation);
#endif
    }

private:
#if !PLATFORM(IOS_FAMILY_SIMULATOR)
    CGContextRef m_context;
    bool m_useOrientationDependentFontAntialiasing;
    CGFontAntialiasingStyle m_oldAntialiasingStyle;
#endif
};

}

#endif

#endif // FontAntialiasingStateSaver_h
