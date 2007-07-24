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
#include "WebKitGraphics.h"

#include "WebKit.h"
#include "WebKitDLL.h"

#pragma warning(push, 0)
#include <WebCore/CharacterNames.h>
#include <WebCore/Font.h>
#include <WebCore/FontDescription.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/PlatformString.h>
#include <WebCore/StringTruncator.h>
#include <WebCore/WebCoreTextRenderer.h>

#include <CoreGraphics/CoreGraphics.h>
#pragma warning(pop)

#include <WebKitSystemInterface/WebKitSystemInterface.h>

using namespace WebCore;

static Font makeFont(const WebFontDescription& description)
{
    String fontFamilyString(description.family, description.familyLength);

    FontDescription f;
    FontFamily family;
    family.setFamily(fontFamilyString);
    f.setFamily(family);
    f.setSpecifiedSize(description.size);
    f.setComputedSize(description.size);
    f.setItalic(description.italic);
    f.setBold(description.bold);
    f.setIsAbsoluteSize(true);

    Font font(f, 0, 0);
    font.update();

    return font;
}

void DrawTextAtPoint(CGContextRef cgContext, LPCTSTR text, int length, POINT point, const WebFontDescription& description, CGColorRef color, int underlinedIndex, bool drawAsPassword)
{
    GraphicsContext context(cgContext);

    const CGFloat* components = CGColorGetComponents(color);
    Color textColor((int)(components[0] * 255), (int)(components[1] * 255), (int)(components[2] * 255), (int)(components[3] * 255));

    String drawString(text, length);
    if (drawAsPassword)
        drawString = drawString.impl()->secure(WebCore::bullet);
    WebCoreDrawTextAtPoint(context, drawString, point, makeFont(description), textColor, underlinedIndex);
}

void WebDrawText(WebTextRenderInfo* info)
{
    if (!info || info->structSize != sizeof(WebTextRenderInfo) || !info->cgContext || !info->description)
        return;

    int oldFontSmoothingLevel = -1;
    if (info->overrideSmoothingLevel >= 0) {
        oldFontSmoothingLevel = wkGetFontSmoothingLevel();
        wkSetFontSmoothingLevel(info->overrideSmoothingLevel);
    }

    DrawTextAtPoint(info->cgContext, info->text, info->length, info->pt, *(info->description), info->color, info->underlinedIndex, info->drawAsPassword);

    if (info->overrideSmoothingLevel >= 0)
        wkSetFontSmoothingLevel(oldFontSmoothingLevel);
}

float TextFloatWidth(LPCTSTR text, int length, const WebFontDescription& description)
{
    return WebCoreTextFloatWidth(String(text, length), makeFont(description));
}

void FontMetrics(const WebFontDescription& description, int* ascent, int* descent, int* lineSpacing)
{
    if (!ascent && !descent && !lineSpacing)
        return;

    Font font(makeFont(description));

    if (ascent)
        *ascent = font.ascent();

    if (descent)
        *descent = font.descent();

    if (lineSpacing)
        *lineSpacing = font.lineSpacing();
}

void CenterTruncateStringToWidth(LPCTSTR text, int length, const WebFontDescription& description, float width, WCHAR* buffer)
{
    ASSERT(buffer);

    String result = StringTruncator::centerTruncate(String(text, length), width, makeFont(description), false);
    memcpy(buffer, result.characters(), result.length() * sizeof(UChar));
    buffer[result.length()] = '\0';
}

void RightTruncateStringToWidth(LPCTSTR text, int length, const WebFontDescription& description, float width, WCHAR* buffer)
{
    ASSERT(buffer);

    String result = StringTruncator::rightTruncate(String(text, length), width, makeFont(description), false);
    memcpy(buffer, result.characters(), result.length() * sizeof(UChar));
    buffer[result.length()] = '\0';
}
