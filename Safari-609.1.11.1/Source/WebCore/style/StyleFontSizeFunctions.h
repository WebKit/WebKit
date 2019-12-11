    /*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once


namespace WebCore {

class Document;
class RenderStyle;

namespace Style {

float computedFontSizeFromSpecifiedSize(float specifiedSize, bool isAbsoluteSize, bool useSVGZoomRules, const RenderStyle*, const Document&);
float computedFontSizeFromSpecifiedSizeForSVGInlineText(float specifiedSize, bool isAbsoluteSize, float zoomFactor, const Document&);

// Given a CSS keyword id in the range (CSSValueXxSmall to CSSValueWebkitXxxLarge), this function will return
// the correct font size scaled relative to the user's default (medium).
float fontSizeForKeyword(unsigned keywordID, bool shouldUseFixedDefaultSize, const Document&);

// Given a font size in pixel, this function will return legacy font size between 1 and 7.
int legacyFontSizeForPixelSize(int pixelFontSize, bool shouldUseFixedDefaultSize, const Document&);

}
}
