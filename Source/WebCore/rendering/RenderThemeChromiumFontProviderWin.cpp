/*
 * Copyright (C) 2012 Google Inc.  All rights reserved.
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
#include "RenderThemeChromiumFontProvider.h"

#include "CSSValueKeywords.h"
#include "FontDescription.h"
#include "HWndDC.h"
#include "SystemInfo.h"

#include <windows.h>
#include <wtf/text/WTFString.h>

#define SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(structName, member) \
    offsetof(structName, member) + \
    (sizeof static_cast<structName*>(0)->member)
#define NONCLIENTMETRICS_SIZE_PRE_VISTA \
    SIZEOF_STRUCT_WITH_SPECIFIED_LAST_MEMBER(NONCLIENTMETRICS, lfMessageFont)

namespace WebCore {

static FontDescription& smallSystemFont()
{
    DEFINE_STATIC_LOCAL(FontDescription, font, ());
    return font;
}

static FontDescription& menuFont()
{
    DEFINE_STATIC_LOCAL(FontDescription, font, ());
    return font;
}

static FontDescription& labelFont()
{
    DEFINE_STATIC_LOCAL(FontDescription, font, ());
    return font;
}

// Converts |points| to pixels. One point is 1/72 of an inch.
static float pointsToPixels(float points)
{
    static float pixelsPerInch = 0.0f;
    if (!pixelsPerInch) {
        HWndDC hdc(0); // What about printing? Is this the right DC?
        if (hdc) // Can this ever actually be 0?
            pixelsPerInch = GetDeviceCaps(hdc, LOGPIXELSY);
        else
            pixelsPerInch = 96.0f;
    }

    static const float pointsPerInch = 72.0f;
    return points / pointsPerInch * pixelsPerInch;
}

static void getNonClientMetrics(NONCLIENTMETRICS* metrics)
{
    static UINT size = (windowsVersion() >= WindowsVista) ?
        sizeof(NONCLIENTMETRICS) : NONCLIENTMETRICS_SIZE_PRE_VISTA;
    metrics->cbSize = size;
    bool success = !!SystemParametersInfo(SPI_GETNONCLIENTMETRICS, size, metrics, 0);
    ASSERT(success);
}

// Return the height of system font |font| in pixels. We use this size by
// default for some non-form-control elements.
static float systemFontSize(const LOGFONT& font)
{
    float size = -font.lfHeight;
    if (size < 0) {
        HFONT hFont = CreateFontIndirect(&font);
        if (hFont) {
            HWndDC hdc(0); // What about printing? Is this the right DC?
            if (hdc) {
                HGDIOBJ hObject = SelectObject(hdc, hFont);
                TEXTMETRIC tm;
                GetTextMetrics(hdc, &tm);
                SelectObject(hdc, hObject);
                size = tm.tmAscent;
            }
            DeleteObject(hFont);
        }
    }

    // The "codepage 936" bit here is from Gecko; apparently this helps make
    // fonts more legible in Simplified Chinese where the default font size is
    // too small.
    //
    // FIXME: http://b/1119883 Since this is only used for "small caption",
    // "menu", and "status bar" objects, I'm not sure how much this even
    // matters. Plus the Gecko patch went in back in 2002, and maybe this
    // isn't even relevant anymore. We should investigate whether this should
    // be removed, or perhaps broadened to be "any CJK locale".
    //
    return ((size < 12.0f) && (GetACP() == 936)) ? 12.0f : size;
}

// static
void RenderThemeChromiumFontProvider::systemFont(int propId, FontDescription& fontDescription)
{
    // This logic owes much to RenderThemeSafari.cpp.
    FontDescription* cachedDesc = 0;
    AtomicString faceName;
    float fontSize = 0;
    switch (propId) {
    case CSSValueSmallCaption:
        cachedDesc = &smallSystemFont();
        if (!smallSystemFont().isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = AtomicString(metrics.lfSmCaptionFont.lfFaceName, wcslen(metrics.lfSmCaptionFont.lfFaceName));
            fontSize = systemFontSize(metrics.lfSmCaptionFont);
        }
        break;
    case CSSValueMenu:
        cachedDesc = &menuFont();
        if (!menuFont().isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = AtomicString(metrics.lfMenuFont.lfFaceName, wcslen(metrics.lfMenuFont.lfFaceName));
            fontSize = systemFontSize(metrics.lfMenuFont);
        }
        break;
    case CSSValueStatusBar:
        cachedDesc = &labelFont();
        if (!labelFont().isAbsoluteSize()) {
            NONCLIENTMETRICS metrics;
            getNonClientMetrics(&metrics);
            faceName = metrics.lfStatusFont.lfFaceName;
            fontSize = systemFontSize(metrics.lfStatusFont);
        }
        break;
    case CSSValueWebkitMiniControl:
    case CSSValueWebkitSmallControl:
    case CSSValueWebkitControl:
        faceName = defaultGUIFont();
        // Why 2 points smaller? Because that's what Gecko does.
        fontSize = s_defaultFontSize - pointsToPixels(2);
        break;
    default:
        faceName = defaultGUIFont();
        fontSize = s_defaultFontSize;
        break;
    }

    if (!cachedDesc)
        cachedDesc = &fontDescription;

    if (fontSize) {
        cachedDesc->firstFamily().setFamily(faceName);
        cachedDesc->setIsAbsoluteSize(true);
        cachedDesc->setGenericFamily(FontDescription::NoFamily);
        cachedDesc->setSpecifiedSize(fontSize);
        cachedDesc->setWeight(FontWeightNormal);
        cachedDesc->setItalic(false);
    }
    fontDescription = *cachedDesc;
}

// static
void RenderThemeChromiumFontProvider::setDefaultFontSize(int fontSize)
{
    s_defaultFontSize = static_cast<float>(fontSize);

    // Reset cached fonts.
    smallSystemFont() = menuFont() = labelFont() = FontDescription();
}

} // namespace WebCore
