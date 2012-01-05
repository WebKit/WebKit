/*
 * Copyright (C) 2011 Google Inc. All rights reserved.
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
#include "ScrollbarThemeChromiumAndroid.h"

#include "PlatformContextSkia.h"
#include "PlatformMouseEvent.h"
#include "PlatformSupport.h"
#include "Scrollbar.h"
#include "TransformationMatrix.h"

namespace WebCore {

// On Android, the threaded compositor is in charge of drawing the scrollbar,
// so set the internal scrollbar thickness and button length to be zero.
static const int scrollbarThicknessValue = 0;
static const int buttonLength = 0;

ScrollbarTheme* ScrollbarTheme::nativeTheme()
{
    DEFINE_STATIC_LOCAL(ScrollbarThemeChromiumAndroid, theme, ());
    return &theme;
}

int ScrollbarThemeChromiumAndroid::scrollbarThickness(ScrollbarControlSize controlSize)
{
    if (PlatformSupport::layoutTestMode()) {
        // Match Chromium-Linux for DumpRenderTree, so the layout test results
        // can be shared. The width of scrollbar down arrow should equal the
        // width of the vertical scrollbar.
        IntSize scrollbarSize = PlatformSupport::getThemePartSize(PlatformSupport::PartScrollbarDownArrow);
        return scrollbarSize.width();
    }
    return scrollbarThicknessValue;
}

void ScrollbarThemeChromiumAndroid::paintScrollbarBackground(GraphicsContext* context, Scrollbar* scrollbar)
{
    // Paint black background in DumpRenderTree, otherwise the pixels in the scrollbar area depend
    // on their previous state, which makes the dumped result undetermined.
    if (PlatformSupport::layoutTestMode())
        context->fillRect(scrollbar->frameRect(), Color::black, ColorSpaceDeviceRGB);
}

bool ScrollbarThemeChromiumAndroid::shouldCenterOnThumb(Scrollbar*, const PlatformMouseEvent& evt)
{
    return true;
}

IntSize ScrollbarThemeChromiumAndroid::buttonSize(Scrollbar* scrollbar)
{
    if (scrollbar->orientation() == VerticalScrollbar)
        return IntSize(scrollbarThicknessValue, buttonLength);

    return IntSize(buttonLength, scrollbarThicknessValue);
}

int ScrollbarThemeChromiumAndroid::minimumThumbLength(Scrollbar* scrollbar)
{
    return 2 * scrollbarThickness(scrollbar->controlSize());
}

} // namespace WebCore
