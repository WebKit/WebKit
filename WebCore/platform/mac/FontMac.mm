/**
 * This file is part of the html renderer for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2000 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2003, 2006 Apple Computer, Inc.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#import "config.h"
#import "Font.h"

#import "Logging.h"
#import "BlockExceptions.h"
#import "FoundationExtras.h"

#import "FontFallbackList.h"
#import "GraphicsContext.h"
#import "KWQKHTMLSettings.h"

#import "FontData.h"
#import "WebTextRendererFactory.h"

#import "IntRect.h"

using namespace std;

namespace WebCore {

FontFallbackList::FontFallbackList()
:m_pitch(UnknownPitch), m_font(nil)
{
    m_platformFont.font = nil;
}

FontFallbackList::~FontFallbackList()
{
    KWQRelease(m_platformFont.font);
}

const FontPlatformData& FontFallbackList::platformFont(const FontDescription& fontDescription) const
{
    if (!m_platformFont.font) {
        CREATE_FAMILY_ARRAY(fontDescription, families);
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        int traits = 0;
        if (fontDescription.italic())
            traits |= NSItalicFontMask;
        if (fontDescription.weight() >= WebCore::cBoldWeight)
            traits |= NSBoldFontMask;
        m_platformFont = [[WebTextRendererFactory sharedFactory] 
                                     fontWithFamilies:families traits:traits size:fontDescription.computedPixelSize()];
        KWQRetain(m_platformFont.font);
        m_platformFont.forPrinter = fontDescription.usePrinterFont();
        END_BLOCK_OBJC_EXCEPTIONS;
    }
    return m_platformFont;
}

FontData* FontFallbackList::primaryFont(const FontDescription& fontDescription) const
{
    if (!m_font)
        m_font = [[WebTextRendererFactory sharedFactory] rendererWithFont:platformFont(fontDescription)];
    return m_font;
}

void FontFallbackList::determinePitch(const FontDescription& fontDescription) const
{
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([[WebTextRendererFactory sharedFactory] isFontFixedPitch:platformFont(fontDescription)])
        m_pitch = FixedPitch;
    else
        m_pitch = VariablePitch;
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FontFallbackList::invalidate()
{
    m_font = 0;
    KWQRelease(m_platformFont.font);
    m_platformFont.font = nil;
    m_pitch = UnknownPitch;
}

const FontPlatformData& Font::platformFont() const
{
    return m_fontList->platformFont(fontDescription());
}

IntRect Font::selectionRectForText(const IntPoint& point, int h, int tabWidth, int xpos, 
    const UChar* str, int slen, int pos, int l, int toAdd,
    bool rtl, bool visuallyOrdered, int from, int to) const
{
    assert(m_fontList);
    int len = min(slen - pos, l);

    CREATE_FAMILY_ARRAY(fontDescription(), families);

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, str + pos, len, from, to);
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.rtl = rtl;
    style.directionalOverride = visuallyOrdered;
    style.letterSpacing = letterSpacing();
    style.wordSpacing = wordSpacing();
    style.smallCaps = fontDescription().smallCaps();
    style.families = families;    
    style.padding = toAdd;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = point;
    geometry.selectionY = point.y();
    geometry.selectionHeight = h;
    geometry.useFontMetricsForSelectionYAndHeight = false;
    return enclosingIntRect(m_fontList->primaryFont(fontDescription())->selectionRectForRun(&run, &style, &geometry));
}
                     
void Font::drawText(GraphicsContext* context, const IntPoint& point, int tabWidth, int xpos, const UChar* str, int len, int from, int to,
                    int toAdd, TextDirection d, bool visuallyOrdered) const
{
    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(*this, families);

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;
        
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, str, len, from, to);    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(context->pen().color());
    style.backgroundColor = nil;
    style.rtl = d == RTL;
    style.directionalOverride = visuallyOrdered;
    style.letterSpacing = letterSpacing();
    style.wordSpacing = wordSpacing();
    style.smallCaps = isSmallCaps();
    style.families = families;
    style.padding = toAdd;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = point;
    m_fontList->primaryFont(fontDescription())->drawRun(&run, &style, &geometry);
}

void Font::drawHighlightForText(GraphicsContext* context, const IntPoint& point, int h, int tabWidth, int xpos, const UChar* str,
                                int len, int from, int to, int toAdd,
                                TextDirection d, bool visuallyOrdered, const Color& backgroundColor) const
{
    // Avoid allocations, use stack array to pass font families.  Normally these
    // css fallback lists are small <= 3.
    CREATE_FAMILY_ARRAY(*this, families);

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;
        
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, str, len, from, to);    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.textColor = nsColor(context->pen().color());
    style.backgroundColor = backgroundColor.isValid() ? nsColor(backgroundColor) : nil;
    style.rtl = d == RTL;
    style.directionalOverride = visuallyOrdered;
    style.letterSpacing = letterSpacing();
    style.wordSpacing = wordSpacing();
    style.smallCaps = isSmallCaps();
    style.families = families;    
    style.padding = toAdd;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    WebCoreTextGeometry geometry;
    WebCoreInitializeEmptyTextGeometry(&geometry);
    geometry.point = point;
    geometry.selectionY = point.y();
    geometry.selectionHeight = h;
    geometry.useFontMetricsForSelectionYAndHeight = false;
    m_fontList->primaryFont(fontDescription())->drawHighlightForRun(&run, &style, &geometry);
}

void Font::drawLineForText(GraphicsContext* context, const IntPoint& point, int yOffset, int width) const
{
    m_fontList->primaryFont(fontDescription())->drawLineForCharacters(point, yOffset, width, context->pen().color(), context->pen().width());
}

void Font::drawLineForMisspelling(GraphicsContext* context, const IntPoint& point, int width) const
{
    m_fontList->primaryFont(fontDescription())->drawLineForMisspelling(point, width);
}

int Font::misspellingLineThickness(GraphicsContext* context) const
{
    return m_fontList->primaryFont(fontDescription())->misspellingLineThickness();
}

float Font::floatWidth(const UChar* uchars, int slen, int pos, int len, int tabWidth, int xpos, bool runRounding) const
{
    assert(m_fontList);
    CREATE_FAMILY_ARRAY(fontDescription(), families);

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, uchars, slen, pos, pos + len);
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    style.letterSpacing = letterSpacing();
    style.wordSpacing = wordSpacing();
    style.smallCaps = fontDescription().smallCaps();
    style.families = families;
    style.applyRunRounding = runRounding;

    return m_fontList->primaryFont(fontDescription())->floatWidthForRun(&run, &style);
}

int Font::checkSelectionPoint(const UChar* s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, TextDirection d, bool visuallyOrdered, bool includePartialGlyphs) const
{
    assert(m_fontList);
    CREATE_FAMILY_ARRAY(fontDescription(), families);
    
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, s + pos, min(slen - pos, len), 0, len);
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.letterSpacing = letterSpacing();
    style.wordSpacing = wordSpacing();
    style.smallCaps = fontDescription().smallCaps();
    style.families = families;
    style.padding = toAdd;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    style.rtl =  d == RTL;
    style.directionalOverride = visuallyOrdered;

    return m_fontList->primaryFont(fontDescription())->pointToOffset(&run, &style, x, includePartialGlyphs);
}

}
