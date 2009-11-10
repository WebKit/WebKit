/*
 * Copyright (C) 2009 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WebColor.h"

#include "Color.h"
#include "CSSValueKeywords.h"
#include "RenderTheme.h"
#include "UnusedParam.h"
#include "WebColorName.h"

using namespace::WebCore;

namespace WebKit {

static int toCSSValueKeyword(WebColorName in_value)
{
    switch (in_value) {
    case WebColorActiveBorder:
        return CSSValueActiveborder;
    case WebColorActiveCaption:
        return CSSValueActivecaption;
    case WebColorAppworkspace:
        return CSSValueAppworkspace;
    case WebColorBackground:
        return CSSValueBackground;
    case WebColorButtonFace:
        return CSSValueButtonface;
    case WebColorButtonHighlight:
        return CSSValueButtonhighlight;
    case WebColorButtonShadow:
        return CSSValueButtonshadow;
    case WebColorButtonText:
        return CSSValueButtontext;
    case WebColorCaptionText:
        return CSSValueCaptiontext;
    case WebColorGrayText:
        return CSSValueGraytext;
    case WebColorHighlight:
        return CSSValueHighlight;
    case WebColorHighlightText:
        return CSSValueHighlighttext;
    case WebColorInactiveBorder:
        return CSSValueInactiveborder;
    case WebColorInactiveCaption:
        return CSSValueInactivecaption;
    case WebColorInactiveCaptionText:
        return CSSValueInactivecaptiontext;
    case WebColorInfoBackground:
        return CSSValueInfobackground;
    case WebColorInfoText:
        return CSSValueInfotext;
    case WebColorMenu:
        return CSSValueMenu;
    case WebColorMenuText:
        return CSSValueMenutext;
    case WebColorScrollbar:
        return CSSValueScrollbar;
    case WebColorText:
        return CSSValueText;
    case WebColorThreedDarkShadow:
        return CSSValueThreeddarkshadow;
    case WebColorThreedShadow:
        return CSSValueThreedshadow;
    case WebColorThreedFace:
        return CSSValueThreedface;
    case WebColorThreedHighlight:
        return CSSValueThreedhighlight;
    case WebColorThreedLightShadow:
        return CSSValueThreedlightshadow;
    case WebColorWebkitFocusRingColor:
        return CSSValueWebkitFocusRingColor;
    case WebColorWindow:
        return CSSValueWindow;
    case WebColorWindowFrame:
        return CSSValueWindowframe;
    case WebColorWindowText:
        return CSSValueWindowtext;
    default:
        return CSSValueInvalid;
    }
}

void setNamedColors(const WebColorName* colorNames, const WebColor* colors, size_t length)
{
    for (size_t i = 0; i < length; ++i) {
        WebColorName colorName = colorNames[i];
        WebColor color = colors[i];

        // Convert color to internal value identifier.
        int internalColorName = toCSSValueKeyword(colorName);
        if (internalColorName == CSSValueWebkitFocusRingColor) {
            RenderTheme::setCustomFocusRingColor(color);
            continue;
        }
    }

    // TODO(jeremy): Tell RenderTheme to update colors.
}

} // WebKit
