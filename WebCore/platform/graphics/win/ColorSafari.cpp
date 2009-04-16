/*
 * Copyright (C) 2007 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "Color.h"

#include <CoreGraphics/CGColor.h>
#include <SafariTheme/SafariTheme.h>
#include <wtf/Assertions.h>
#include <wtf/RetainPtr.h>

using namespace SafariTheme;

namespace WebCore {

typedef CGColorRef (APIENTRY*stCopyThemeColorPtr)(unsigned, SafariTheme::ThemeControlState);
static const unsigned stFocusRingColorID = 4;

static const unsigned aquaFocusRingColor = 0xFF7DADD9;

static RGBA32 makeRGBAFromCGColor(CGColorRef c)
{
    const CGFloat* components = CGColorGetComponents(c);
    return makeRGBA(255 * components[0], 255 * components[1], 255 * components[2], 255 * components[3]);
}

Color focusRingColor()
{
    static Color focusRingColor;
    focusRingColor.isValid();

    if (!focusRingColor.isValid()) {
        if (HMODULE module = LoadLibrary(SAFARITHEMEDLL))
            if (stCopyThemeColorPtr stCopyThemeColor = (stCopyThemeColorPtr)GetProcAddress(module, "STCopyThemeColor")) {
                RetainPtr<CGColorRef> c(AdoptCF, stCopyThemeColor(stFocusRingColorID, SafariTheme::ActiveState));
                focusRingColor = makeRGBAFromCGColor(c.get());
            }
        if (!focusRingColor.isValid())
            focusRingColor = aquaFocusRingColor;
    }

    return focusRingColor;
}

} // namespace WebCore
