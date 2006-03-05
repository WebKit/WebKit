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
#import "KWQExceptions.h"
#import "FoundationExtras.h"

#import "FontRenderer.h"
#import "GraphicsContext.h"
#import "khtml_settings.h"
#import <algorithm>

#import "WebCoreTextRenderer.h"
#import "IntRect.h"

namespace WebCore {

FontRenderer::FontRenderer()
:m_pitch(UnknownPitch), m_renderer(nil)
{
    m_webCoreFont.font = nil;
}

const WebCoreFont& FontRenderer::getWebCoreFont(const FontDescription& fontDescription) const
{
    if (!m_webCoreFont.font) {
        CREATE_FAMILY_ARRAY(fontDescription, families);
        KWQ_BLOCK_EXCEPTIONS;
        int traits = 0;
        if (fontDescription.italic())
            traits |= NSItalicFontMask;
        if (fontDescription.weight() >= WebCore::cBoldWeight)
            traits |= NSBoldFontMask;
        m_webCoreFont = [[WebCoreTextRendererFactory sharedFactory] 
                                     fontWithFamilies:families traits:traits size:fontDescription.computedPixelSize()];
        KWQRetain(m_webCoreFont.font);
        m_webCoreFont.forPrinter = fontDescription.usePrinterFont();
        KWQ_UNBLOCK_EXCEPTIONS;
    }
    return m_webCoreFont;
}

id <WebCoreTextRenderer> FontRenderer::getRenderer(const FontDescription& fontDescription)
{
    if (!m_renderer)
        m_renderer = KWQRetain([[WebCoreTextRendererFactory sharedFactory] rendererWithFont:getWebCoreFont(fontDescription)]);
    return m_renderer;
}

void FontRenderer::determinePitch(const FontDescription& fontDescription) const {
    KWQ_BLOCK_EXCEPTIONS;
    if ([[WebCoreTextRendererFactory sharedFactory] isFontFixedPitch:getWebCoreFont(fontDescription)])
        m_pitch = FixedPitch;
    else
        m_pitch = VariablePitch;
    KWQ_UNBLOCK_EXCEPTIONS;
}

void FontRenderer::update(const FontDescription& fontDescription)
{
    KWQRelease(m_renderer);
    m_renderer = nil;
    KWQRelease(m_webCoreFont.font);
    m_webCoreFont.font = nil;
    m_pitch = UnknownPitch;
}

const WebCoreFont& Font::getWebCoreFont() const
{
    return m_renderer->getWebCoreFont(fontDescription());
}

int Font::ascent() const
{
    return [m_renderer->getRenderer(fontDescription()) ascent];
}

int Font::descent() const
{
    assert(m_renderer);
    return [m_renderer->getRenderer(fontDescription()) descent];
}

int Font::lineSpacing() const
{
    assert(m_renderer);
    return [m_renderer->getRenderer(fontDescription()) lineSpacing];
}

float Font::xHeight() const
{
    assert(m_renderer);
    return [m_renderer->getRenderer(fontDescription()) xHeight];
}

bool Font::isFixedPitch() const
{
    assert(m_renderer);
    return m_renderer->isFixedPitch(fontDescription());
}

IntRect Font::selectionRectForText(int x, int y, int h, int tabWidth, int xpos, 
    const QChar* str, int slen, int pos, int l, int toAdd,
    bool rtl, bool visuallyOrdered, int from, int to) const
{
    assert(m_renderer);
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
    geometry.point = NSMakePoint(x, y);
    geometry.selectionY = y;
    geometry.selectionHeight = h;
    geometry.useFontMetricsForSelectionYAndHeight = false;
    return enclosingIntRect([m_renderer->getRenderer(fontDescription()) selectionRectForRun:&run style:&style geometry:&geometry]);
}

void Font::drawHighlightForText(GraphicsContext* context, int x, int y, int h, int tabWidth, int xpos, 
    const QChar* str, int slen, int pos, int len,
    int toAdd, TextDirection d, bool visuallyOrdered, int from, int to, Color bg) const
{
    // FIXME: We should be drawing this, and people should be using the GraphicsContext API instead of calling us directly.
    context->drawHighlightForText(x, y, h, tabWidth, xpos, str + pos, std::min(slen - pos, len), from, to, toAdd, bg, d, visuallyOrdered,
                                  m_letterSpacing, m_wordSpacing, fontDescription().smallCaps());
}
                     
void Font::drawText(GraphicsContext* context, int x, int y, int tabWidth, int xpos, const QChar* str, int slen, int pos, int len,
    int toAdd, TextDirection d, bool visuallyOrdered, int from, int to, Color bg ) const
{
    // FIXME: We should be drawing this, and people should be using the GraphicsContext API instead of calling us directly.
    context->drawText(x, y, tabWidth, xpos, str + pos, std::min(slen - pos, len), from, to, toAdd, bg, d, visuallyOrdered,
                      m_letterSpacing, m_wordSpacing, fontDescription().smallCaps());
}

float Font::floatWidth(const QChar* uchars, int slen, int pos, int len, int tabWidth, int xpos) const
{
    assert(m_renderer);
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

    return [m_renderer->getRenderer(fontDescription()) floatWidthForRun:&run style:&style];

}

int Font::checkSelectionPoint(const QChar* s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int x, TextDirection d, bool visuallyOrdered, bool includePartialGlyphs) const
{
    assert(m_renderer);
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

    return [m_renderer->getRenderer(fontDescription()) pointToOffset:&run style:&style position:x includePartialGlyphs:includePartialGlyphs];
}

}
