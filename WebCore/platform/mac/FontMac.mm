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

#import "FontDataSet.h"
#import "GraphicsContext.h"
#import "KWQKHTMLSettings.h"

#import "WebTextRenderer.h"
#import "WebTextRendererFactory.h"

#import "IntRect.h"

namespace WebCore {

FontDataSet::FontDataSet()
:m_pitch(UnknownPitch), m_renderer(nil)
{
    m_webCoreFont.font = nil;
}

FontDataSet::~FontDataSet()
{
    KWQRelease(m_renderer);
    KWQRelease(m_webCoreFont.font);
}

const WebCoreFont& FontDataSet::getWebCoreFont(const FontDescription& fontDescription) const
{
    if (!m_webCoreFont.font) {
        CREATE_FAMILY_ARRAY(fontDescription, families);
        BEGIN_BLOCK_OBJC_EXCEPTIONS;
        int traits = 0;
        if (fontDescription.italic())
            traits |= NSItalicFontMask;
        if (fontDescription.weight() >= WebCore::cBoldWeight)
            traits |= NSBoldFontMask;
        m_webCoreFont = [[WebTextRendererFactory sharedFactory] 
                                     fontWithFamilies:families traits:traits size:fontDescription.computedPixelSize()];
        KWQRetain(m_webCoreFont.font);
        m_webCoreFont.forPrinter = fontDescription.usePrinterFont();
        END_BLOCK_OBJC_EXCEPTIONS;
    }
    return m_webCoreFont;
}

WebTextRenderer* FontDataSet::getRenderer(const FontDescription& fontDescription)
{
    if (!m_renderer)
        m_renderer = KWQRetain([[WebTextRendererFactory sharedFactory] rendererWithFont:getWebCoreFont(fontDescription)]);
    return m_renderer;
}

void FontDataSet::determinePitch(const FontDescription& fontDescription) const {
    BEGIN_BLOCK_OBJC_EXCEPTIONS;
    if ([[WebTextRendererFactory sharedFactory] isFontFixedPitch:getWebCoreFont(fontDescription)])
        m_pitch = FixedPitch;
    else
        m_pitch = VariablePitch;
    END_BLOCK_OBJC_EXCEPTIONS;
}

void FontDataSet::invalidate()
{
    KWQRelease(m_renderer);
    m_renderer = nil;
    KWQRelease(m_webCoreFont.font);
    m_webCoreFont.font = nil;
    m_pitch = UnknownPitch;
}

const WebCoreFont& Font::getWebCoreFont() const
{
    return m_dataSet->getWebCoreFont(fontDescription());
}

int Font::ascent() const
{
    return [m_dataSet->getRenderer(fontDescription()) ascent];
}

int Font::descent() const
{
    assert(m_dataSet);
    return [m_dataSet->getRenderer(fontDescription()) descent];
}

int Font::lineSpacing() const
{
    assert(m_dataSet);
    return [m_dataSet->getRenderer(fontDescription()) lineSpacing];
}

float Font::xHeight() const
{
    assert(m_dataSet);
    return [m_dataSet->getRenderer(fontDescription()) xHeight];
}

bool Font::isFixedPitch() const
{
    assert(m_dataSet);
    return m_dataSet->isFixedPitch(fontDescription());
}

IntRect Font::selectionRectForText(const IntPoint& point, int h, int tabWidth, int xpos, 
    const QChar* str, int slen, int pos, int l, int toAdd,
    bool rtl, bool visuallyOrdered, int from, int to) const
{
    assert(m_dataSet);
    int len = std::min(slen - pos, l);

    CREATE_FAMILY_ARRAY(fontDescription(), families);

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)(str+pos), len, from, to);    
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
    return enclosingIntRect([m_dataSet->getRenderer(fontDescription()) selectionRectForRun:&run style:&style geometry:&geometry]);
}
                     
void Font::drawText(GraphicsContext* context, const IntPoint& point, int tabWidth, int xpos, const QChar* str, int len, int from, int to,
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
    WebCoreInitializeTextRun(&run, (const UniChar *)str, len, from, to);    
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
    [m_dataSet->getRenderer(fontDescription()) drawRun:&run style:&style geometry:&geometry];
}

void Font::drawHighlightForText(GraphicsContext* context, const IntPoint& point, int h, int tabWidth, int xpos, const QChar* str,
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
    WebCoreInitializeTextRun(&run, (const UniChar *)str, len, from, to);    
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
    [m_dataSet->getRenderer(fontDescription()) drawHighlightForRun:&run style:&style geometry:&geometry];
}

void Font::drawLineForText(GraphicsContext* context, const IntPoint& point, int yOffset, int width) const
{
    [m_dataSet->getRenderer(fontDescription())
        drawLineForCharacters:point
                      yOffset:(float)yOffset
                        width:width
                        color:nsColor(context->pen().color())
                    thickness:context->pen().width()];
}

void Font::drawLineForMisspelling(GraphicsContext* context, const IntPoint& point, int width) const
{
    [m_dataSet->getRenderer(fontDescription()) drawLineForMisspelling:point withWidth:width];
}

int Font::misspellingLineThickness(GraphicsContext* context) const
{
    return [m_dataSet->getRenderer(fontDescription()) misspellingLineThickness];
}

float Font::floatWidth(const QChar* uchars, int slen, int pos, int len, int tabWidth, int xpos, bool runRounding) const
{
    assert(m_dataSet);
    CREATE_FAMILY_ARRAY(fontDescription(), families);

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)uchars, slen, pos, pos+len);
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    style.letterSpacing = letterSpacing();
    style.wordSpacing = wordSpacing();
    style.smallCaps = fontDescription().smallCaps();
    style.families = families;
    style.applyRunRounding = runRounding;

    return [m_dataSet->getRenderer(fontDescription()) floatWidthForRun:&run style:&style];

}

int Font::checkSelectionPoint(const QChar* s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, TextDirection d, bool visuallyOrdered, bool includePartialGlyphs) const
{
    assert(m_dataSet);
    CREATE_FAMILY_ARRAY(fontDescription(), families);
    
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)(s+pos), std::min(slen - pos, len), 0, len);
    
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

    return [m_dataSet->getRenderer(fontDescription()) pointToOffset:&run style:&style position:x includePartialGlyphs:includePartialGlyphs];
}

}
