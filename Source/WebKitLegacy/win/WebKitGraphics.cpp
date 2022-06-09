/*
 * Copyright (C) 2007 Apple Inc.  All rights reserved.
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

#include "WebKitGraphics.h"

#include "WebKit.h"
#include "WebKitDLL.h"

#include "WebPreferences.h"

#include <WebCore/FontCascade.h>
#include <WebCore/FontDescription.h>
#include <WebCore/FontSelector.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/StringTruncator.h>
#include <WebCore/WebCoreTextRenderer.h>
#include <wtf/text/StringView.h>
#include <wtf/text/WTFString.h>
#include <wtf/unicode/CharacterNames.h>

#include <CoreGraphics/CoreGraphics.h>

using namespace WebCore;

static FontCascade makeFont(const WebFontDescription& description)
{
    AtomString::init();

    String fontFamilyString(description.family, description.familyLength);

    FontCascadeDescription f;
    f.setOneFamily(fontFamilyString);
    f.setSpecifiedSize(description.size);
    f.setComputedSize(description.size);
    f.setIsItalic(description.italic);
    f.setWeight(description.bold ? boldWeightValue() : normalWeightValue());
    f.setIsAbsoluteSize(true);

    FontSmoothingType smoothingType;
    if (SUCCEEDED(WebPreferences::sharedStandardPreferences()->fontSmoothing(&smoothingType)))
        f.setRenderingMode(smoothingType == FontSmoothingTypeWindows ? FontRenderingMode::Alternate : FontRenderingMode::Normal);

    FontCascade font(f, 0, 0);
    font.update(0);

    return font;
}

// Text shadow is added post 3.1.1.  In order for nightlies to not break Safari 3.1.1, we should still allow
// the old WebTextRenderInfo that has a smaller structSize than the current one with the new text shadow data members.
struct WebTextRenderInfoWithoutShadow
{
    DWORD structSize;
    CGContextRef cgContext;
    LPCTSTR text;
    int length;
    POINT pt;
    const WebFontDescription* description;
    CGColorRef color;
    int underlinedIndex;
    bool drawAsPassword;
    int overrideSmoothingLevel; // pass in -1 if caller does not want to override smoothing level
};

float TextFloatWidth(LPCTSTR text, int length, const WebFontDescription& description)
{
    return WebCoreTextFloatWidth(String(text, length), makeFont(description));
}

void FontMetrics(const WebFontDescription& description, int* ascent, int* descent, int* lineSpacing)
{
    if (!ascent && !descent && !lineSpacing)
        return;

    FontCascade font(makeFont(description));
    const WebCore::FontMetrics& fontMetrics(font.fontMetrics());

    if (ascent)
        *ascent = fontMetrics.ascent();

    if (descent)
        *descent = fontMetrics.descent();

    if (lineSpacing)
        *lineSpacing = fontMetrics.lineSpacing();
}

unsigned CenterTruncateStringToWidth(LPCTSTR text, int length, const WebFontDescription& description, float width, WCHAR* buffer)
{
    ASSERT(buffer);

    String result = StringTruncator::centerTruncate(String(text, length), width, makeFont(description));
    StringView(result).getCharactersWithUpconvert(buffer);
    buffer[result.length()] = '\0';
    return result.length();
}

unsigned RightTruncateStringToWidth(LPCTSTR text, int length, const WebFontDescription& description, float width, WCHAR* buffer)
{
    ASSERT(buffer);

    String result = StringTruncator::rightTruncate(String(text, length), width, makeFont(description));
    StringView(result).getCharactersWithUpconvert(buffer);
    buffer[result.length()] = '\0';
    return result.length();
}

void WebKitSetShouldUseFontSmoothing(bool smooth)
{
    WebCoreSetShouldUseFontSmoothing(smooth);
}

bool WebKitShouldUseFontSmoothing()
{
    return WebCoreShouldUseFontSmoothing();
}
