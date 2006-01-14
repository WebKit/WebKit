/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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
#import "KWQFontMetrics.h"

#import <Cocoa/Cocoa.h>

#import "KWQFont.h"
#import "KWQLogging.h"
#import "KWQFoundationExtras.h"

#import "misc/shared.h"

#import "WebCoreTextRenderer.h"
#import "WebCoreTextRendererFactory.h"

// We know that none of the ObjC calls here will raise exceptions
// because they are all calls to WebCoreTextRenderer, which has a
// contract of not raising.

struct QFontMetricsPrivate : public khtml::Shared<QFontMetricsPrivate>
{
    QFontMetricsPrivate(const QFont &font)
        : _font(font), _renderer(nil)
    {
    }
    ~QFontMetricsPrivate()
    {
        KWQRelease(_renderer);
    }
    id <WebCoreTextRenderer> getRenderer()
    {
        if (!_renderer)
            _renderer = KWQRetain([[WebCoreTextRendererFactory sharedFactory] rendererWithFont:_font.getWebCoreFont()]);
        return _renderer;
    }
    
    const QFont &font() const { return _font; }
    void setFont(const QFont &font)
    {
        if (_font == font) {
            return;
        }
        _font = font;
        KWQRelease(_renderer);
        _renderer = nil;
    }

private:
    QFont _font;
    id <WebCoreTextRenderer> _renderer;
    
    QFontMetricsPrivate(const QFontMetricsPrivate&);
    QFontMetricsPrivate& operator=(const QFontMetricsPrivate&);
};

QFontMetrics::QFontMetrics()
{
}

QFontMetrics::QFontMetrics(const QFont &font)
    : data(new QFontMetricsPrivate(font))
{
}

QFontMetrics::QFontMetrics(const QFontMetrics &other)
    : data(other.data)
{
}

QFontMetrics::~QFontMetrics()
{
}

QFontMetrics &QFontMetrics::operator=(const QFontMetrics &other)
{
    data = other.data;
    return *this;
}

void QFontMetrics::setFont(const QFont &font)
{
    if (!data) {
        data = new QFontMetricsPrivate(font);
    } else {
        data->setFont(font);
    }
}

int QFontMetrics::ascent() const
{
    if (!data) {
        ERROR("called ascent on an empty QFontMetrics");
        return 0;
    }
    
    return [data->getRenderer() ascent];
}

int QFontMetrics::descent() const
{
    if (!data) {
        ERROR("called descent on an empty QFontMetrics");
        return 0;
    }
    
    return [data->getRenderer() descent];
}

int QFontMetrics::height() const
{
    // According to Qt documentation: 
    // "This is always equal to ascent()+descent()+1 (the 1 is for the base line)."
    // We DO NOT match the Qt behavior here.  This is intentional.
    return ascent() + descent();
}

int QFontMetrics::lineSpacing() const
{
    if (!data) {
        ERROR("called lineSpacing on an empty QFontMetrics");
        return 0;
    }
    return [data->getRenderer() lineSpacing];
}

float QFontMetrics::xHeight() const
{
    if (!data) {
        ERROR("called xHeight on an empty QFontMetrics");
        return 0;
    }
    return [data->getRenderer() xHeight];
}

int QFontMetrics::width(const QString &qstring, int tabWidth, int xpos, int len) const
{
    if (!data) {
        ERROR("called width on an empty QFontMetrics");
        return 0;
    }
    
    CREATE_FAMILY_ARRAY(data->font(), families);

    int length = len == -1 ? qstring.length() : len;

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)qstring.unicode(), length, 0, length);
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.families = families;
    style.tabWidth = tabWidth;
    style.xpos = xpos;

    return lroundf([data->getRenderer() floatWidthForRun:&run style:&style]);
}

int QFontMetrics::width(const QChar *uchars, int len, int tabWidth, int xpos) const
{
    if (!data) {
        ERROR("called width on an empty QFontMetrics");
        return 0;
    }
    
    CREATE_FAMILY_ARRAY(data->font(), families);

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)uchars, len, 0, len);
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.families = families;
    style.tabWidth = tabWidth;
    style.xpos = xpos;

    return lroundf([data->getRenderer() floatWidthForRun:&run style:&style]);
}

float QFontMetrics::floatWidth(const QChar *uchars, int slen, int pos, int len, int tabWidth, int xpos, int letterSpacing, int wordSpacing, bool smallCaps) const
{
    if (!data) {
        ERROR("called floatWidth on an empty QFontMetrics");
        return 0;
    }

    CREATE_FAMILY_ARRAY(data->font(), families);

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)uchars, slen, pos, pos+len);
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    style.letterSpacing = letterSpacing;
    style.wordSpacing = wordSpacing;
    style.smallCaps = smallCaps;
    style.families = families;

    return [data->getRenderer() floatWidthForRun:&run style:&style];
}

IntRect QFontMetrics::selectionRectForText(int x, int y, int h, int tabWidth, int xpos,
    const QChar *str, int len, int from, int to, int toAdd,
    bool rtl, bool visuallyOrdered, int letterSpacing, int wordSpacing, bool smallCaps) const
{
    CREATE_FAMILY_ARRAY(data->font(), families);

    if (from < 0)
        from = 0;
    if (to < 0)
        to = len;
        
    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)str, len, from, to);    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.rtl = rtl;
    style.directionalOverride = visuallyOrdered;
    style.letterSpacing = letterSpacing;
    style.wordSpacing = wordSpacing;
    style.smallCaps = smallCaps;
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
    return IntRect([data->getRenderer() selectionRectForRun:&run style:&style geometry:&geometry]);
}

int QFontMetrics::checkSelectionPoint(QChar *s, int slen, int pos, int len, int toAdd, int tabWidth, int xpos, int letterSpacing, int wordSpacing, bool smallCaps, int x, bool reversed, bool dirOverride, bool includePartialGlyphs) const
{
    if (!data) {
        ERROR("called checkSelectionPoint on an empty QFontMetrics");
        return 0;
    }
    
    CREATE_FAMILY_ARRAY(data->font(), families);

    WebCoreTextRun run;
    WebCoreInitializeTextRun(&run, (const UniChar *)s, slen, pos, pos+len);
    
    WebCoreTextStyle style;
    WebCoreInitializeEmptyTextStyle(&style);
    style.letterSpacing = letterSpacing;
    style.wordSpacing = wordSpacing;
    style.smallCaps = smallCaps;
    style.families = families;
    style.padding = toAdd;
    style.tabWidth = tabWidth;
    style.xpos = xpos;
    style.rtl = reversed;
    style.directionalOverride = dirOverride;

    return [data->getRenderer() pointToOffset:&run style:&style position:x includePartialGlyphs:includePartialGlyphs];
}

IntRect QFontMetrics::boundingRect(QChar c) const
{
    return IntRect(0, 0, width(c, 0, 0), height());
}

IntRect QFontMetrics::boundingRect(const QString &qstring, int tabWidth, int xpos, int len) const
{
    return IntRect(0, 0, width(qstring, tabWidth, xpos, len), height());
}

IntRect QFontMetrics::boundingRect(int x, int y, int width, int height, int flags, const QString &str, int tabWidth, int xpos) const
{
    // FIXME: need to support word wrapping?
    return IntRect(x, y, width, height).intersect(boundingRect(str, tabWidth, xpos));
}

IntSize QFontMetrics::size(int, const QString &qstring, int tabWidth, int xpos) const
{
    return IntSize(width(qstring, tabWidth, xpos), height());
}
