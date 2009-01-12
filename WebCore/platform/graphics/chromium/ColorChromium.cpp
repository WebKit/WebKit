/*
 * Copyright (C) 2008 Google Inc. All rights reserved.
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

namespace WebCore {

Color focusRingColor()
{
// FIXME: This should be split up to ColorChromiumWin and ColorChromiumMac.
#if PLATFORM(DARWIN)
    // To avoid the Mac Chromium build having to rebasline 500+ layout tests and
    // continue to do this w/ new tests that get landed in WebKit, we want to
    // run the layout tests w/ the same color that stock WebKit uses.
    // 
    // TODO: For now we've hard coded the color that WebKit uses for layout
    // tests.  We need to revisit this and do either of the following:
    //  A. Fully honor the color from the UI, which means collecting the color
    //     (and change notifications) in the browser process, and messaging the
    //     color to the render process.
    //  B. Adding a "layout tests" flag, to control the orage vs. blue colors
    //     depending if we're running layout tests.
    // To see the WebKit implementation of using the UI color and/or a flag for
    // layout tests see WebKit/WebCore/platform/graphics/mac/ColorMac.mm.
    // (Reality is we just need an api to override the focus color and both
    // of the above are covered for what this file needs to provide, the
    // two options would be details that happen in other places.)

    // From WebKit:
    // static RGBA32 oldAquaFocusRingColorRGBA = 0xFF7DADD9;
    static Color oldAquaFocusRingColor(0x7D, 0xAD, 0xD9, 0xFF);
    return oldAquaFocusRingColor;
#else
    static Color focusRingColor(229, 151, 0, 255);
    return focusRingColor;
#endif
}

} // namespace WebCore
