/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#import <qfontmetrics.h>

#import <Cocoa/Cocoa.h>

#import <qfont.h>
#import <WebCoreTextRendererFactory.h>
#import <WebCoreTextRenderer.h>
#import <kwqdebug.h>

struct QFontMetricsPrivate
{
    QFontMetricsPrivate(const QFont &font)
    {
        refCount = 0;
        renderer = [[[WebCoreTextRendererFactory sharedFactory] rendererWithFamily:font.getNSFamily() traits:font.getNSTraits() size:font.getNSSize()] retain];
    }
    ~QFontMetricsPrivate()
    {
        [renderer release];
    }
    id <WebCoreTextRenderer> getRenderer()
    {
        return renderer;
    }
    
    int refCount;
    
private:
    id <WebCoreTextRenderer> renderer;
    
    QFontMetricsPrivate(const QFontMetricsPrivate&);
    QFontMetricsPrivate& operator=(const QFontMetricsPrivate&);
};

QFontMetrics::QFontMetrics(const QFont &withFont)
: data(new QFontMetricsPrivate(withFont))
{
}

QFontMetrics::QFontMetrics(const QFontMetrics &withFont)
: data(withFont.data)
{
}

QFontMetrics::~QFontMetrics()
{
}

QFontMetrics &QFontMetrics::operator=(const QFontMetrics &withFont)
{
    data = withFont.data;
    return *this;
}

int QFontMetrics::baselineOffset() const
{
    return ascent();
}

int QFontMetrics::ascent() const
{
    return [data->getRenderer() ascent];
}

int QFontMetrics::descent() const
{
    return [data->getRenderer() descent];
}

int QFontMetrics::height() const
{
    // According to Qt documentation: 
    // "This is always equal to ascent()+descent()+1 (the 1 is for the base line)."
    return ascent() + descent() + 1;
}

int QFontMetrics::lineSpacing() const
{
    return [data->getRenderer() lineSpacing];
}

float QFontMetrics::xHeight() const
{
    return [data->getRenderer() xHeight];
}

int QFontMetrics::width(QChar qc) const
{
    UniChar c = qc.unicode();
    return [data->getRenderer() widthForCharacters:&c length:1];
}

int QFontMetrics::charWidth(const QString &s, int pos) const
{
    return width(s[pos]);
}

int QFontMetrics::width(char c) const
{
    UniChar ch = (uchar) c;
    return [data->getRenderer() widthForCharacters:&ch length:1];
}

int QFontMetrics::width(const QString &qstring, int len) const
{
    NSString *string;

    string = qstring.getNSString();
    if (len != -1)
        string = [string substringToIndex:len];
    return [data->getRenderer() widthForString:string];
}

int QFontMetrics::width(const QChar *uchars, int len) const
{
    return [data->getRenderer() widthForCharacters:(const UniChar *)uchars length:len];
}

float QFontMetrics::floatWidth(const QChar *uchars, int slen, int pos, int len) const
{
    return [data->getRenderer() floatWidthForCharacters:(const UniChar *)uchars stringLength:slen fromCharacterPosition:pos numberOfCharacters:len];
}

float QFontMetrics::floatCharacterWidth(const QChar *uchars, int slen, int pos) const
{
    return [data->getRenderer() floatWidthForCharacters:(const UniChar *)uchars stringLength:slen characterPosition:pos];
}

QRect QFontMetrics::boundingRect(const QString &qstring, int len) const
{
    return QRect(0, 0, width(qstring, len), height());
}

QRect QFontMetrics::boundingRect(int x, int y, int width, int height, int flags, const QString &str) const
{
    // FIXME: need to support word wrapping?
    return QRect(x, y, width, height).intersect(boundingRect(str));
}

QSize QFontMetrics::size(int, const QString &qstring) const
{
    return QSize(width(qstring), height());
}

int QFontMetrics::rightBearing(QChar) const
{
    _logNotYetImplemented();
    return 0;
}

int QFontMetrics::leftBearing(QChar) const
{
    _logNotYetImplemented();
    return 0;
}
