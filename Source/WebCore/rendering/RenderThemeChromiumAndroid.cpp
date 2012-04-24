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
#include "RenderThemeChromiumAndroid.h"

#include "CSSValueKeywords.h"
#include "Color.h"
#include "PaintInfo.h"
#include "PlatformSupport.h"
#include "RenderMediaControlsChromium.h"
#include "RenderObject.h"
#include "RenderProgress.h"
#include "RenderSlider.h"
#include "ScrollbarTheme.h"
#include "UserAgentStyleSheets.h"

namespace WebCore {

PassRefPtr<RenderTheme> RenderThemeChromiumAndroid::create()
{
    return adoptRef(new RenderThemeChromiumAndroid());
}

PassRefPtr<RenderTheme> RenderTheme::themeForPage(Page* page)
{
    static RenderTheme* renderTheme = RenderThemeChromiumAndroid::create().leakRef();
    return renderTheme;
}

RenderThemeChromiumAndroid::~RenderThemeChromiumAndroid()
{
}

Color RenderThemeChromiumAndroid::systemColor(int cssValueId) const
{
    if (PlatformSupport::layoutTestMode() && cssValueId == CSSValueButtonface) {
        // Match Chromium Linux' button color in layout tests.
        static const Color linuxButtonGrayColor(0xffdddddd);
        return linuxButtonGrayColor;
    }
    return RenderTheme::systemColor(cssValueId);
}

String RenderThemeChromiumAndroid::extraMediaControlsStyleSheet()
{
    return String(mediaControlsChromiumAndroidUserAgentStyleSheet, sizeof(mediaControlsChromiumAndroidUserAgentStyleSheet));
}

String RenderThemeChromiumAndroid::extraDefaultStyleSheet()
{
    return RenderThemeChromiumLinux::extraDefaultStyleSheet() +
        String(themeChromiumAndroidUserAgentStyleSheet, sizeof(themeChromiumAndroidUserAgentStyleSheet));
}

void RenderThemeChromiumAndroid::adjustInnerSpinButtonStyle(StyleResolver*, RenderStyle* style, Element*) const
{
    if (PlatformSupport::layoutTestMode()) {
        // Match Chromium Linux spin button style in layout tests.
        // FIXME: Consider removing the conditional if a future Android theme matches this.
        IntSize size = PlatformSupport::getThemePartSize(PlatformSupport::PartInnerSpinButton);

        style->setWidth(Length(size.width(), Fixed));
        style->setMinWidth(Length(size.width(), Fixed));
    }
}

bool RenderThemeChromiumAndroid::paintMediaFullscreenButton(RenderObject* object, const PaintInfo& paintInfo, const IntRect& rect)
{
#if ENABLE(VIDEO)
    return RenderMediaControlsChromium::paintMediaControlsPart(MediaEnterFullscreenButton, object, paintInfo, rect);
#else
    UNUSED_PARAM(object);
    UNUSED_PARAM(paintInfo);
    UNUSED_PARAM(rect);
    return false;
#endif
}

} // namespace WebCore
