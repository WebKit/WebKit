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
#include <math.h>

#include <kwqdebug.h>
#include <qfontmetrics.h>

#define ROUND_TO_INT(f) ((int)rint((f)))

struct QFontMetricsPrivate {
friend class QFontMetrics;
public:
    QFontMetricsPrivate() : 
        font(0), 
        textContainer(0), 
        layoutManager(0),
        attributes(0),
        boundingRectCache(0),
        lastLength(0)
    {
    }
    
    ~QFontMetricsPrivate()
    {
        if (font){
            [font release];
            font = 0;
        }
        if (textContainer){
            [textContainer release];
            textContainer = 0;
        }
        if (layoutManager){
            [layoutManager release];
            layoutManager = 0;
        }
        if (attributes){
            [attributes release];
            attributes = 0;
        }
        if (boundingRectCache){
            [boundingRectCache release];
            boundingRectCache = 0;
        }
    }
    
private:
    NSFont *font;
    NSTextContainer *textContainer;
    NSLayoutManager *layoutManager;
    NSDictionary *attributes;
    NSMutableDictionary *boundingRectCache;
    int lastLength;
};


QFontMetrics::QFontMetrics()
{
    _initialize();
}


QFontMetrics::QFontMetrics(const QFont &withFont)
{
    _initializeWithData (0);
    data->font = [withFont.font retain];
}


QFontMetrics::QFontMetrics(const QFontMetrics &copyFrom)
{
    _initializeWithData(copyFrom.data);
}

void QFontMetrics::_initialize()
{
    _initializeWithData (0);
}

void QFontMetrics::_initializeWithData (QFontMetricsPrivate *withData)
{
    data = new QFontMetricsPrivate();

    if (withData == 0){
        data->font = [QFont::defaultNSFont() retain];
        data->textContainer = 0;
        data->layoutManager = 0;
        data->attributes = 0;
        data->boundingRectCache = 0;
    }
    else {
        data->font = [withData->font retain];
        data->textContainer = [withData->textContainer retain];
        data->layoutManager = [withData->layoutManager retain];
        data->attributes = [withData->attributes retain];
        data->boundingRectCache = [withData->boundingRectCache retain];
    }
}

void QFontMetrics::_free(){
    delete data;
}


QFontMetrics::~QFontMetrics()
{
    _free();
}


int QFontMetrics::ascent() const
{
    // Qt seems to use [font defaultLineHeightForFont] + [font descender] instead
    // of what seems more natural [font ascender].
    // Remember that descender is negative. 
    return ROUND_TO_INT([data->font defaultLineHeightForFont] + [data->font descender]);
}


int QFontMetrics::height() const
{
    // According to Qt documentation: 
    // "This is always equal to ascent()+descent()+1 (the 1 is for the base line)."
    // However, the [font defaultLineHeightForFont] seems more appropriate.
    return ROUND_TO_INT([data->font defaultLineHeightForFont]);
}

const float LargeNumberForText = 1.0e7;

NSRect QFontMetrics::_rectOfString(NSString *string) const
 {
    NSValue *cachedValue;
    NSTextStorage *textStorage;

    if (data->boundingRectCache == nil){
        data->boundingRectCache = [[NSMutableDictionary alloc] init];
    }

    cachedValue = [data->boundingRectCache objectForKey: string];
    if (cachedValue != nil){
        return [cachedValue rectValue];
    }
    
    if (data->textContainer == nil){
        data->textContainer = [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
        data->layoutManager = [[NSLayoutManager alloc] init];
        [data->layoutManager addTextContainer: data->textContainer];
        data->attributes = [[NSDictionary dictionaryWithObjectsAndKeys:data->font, NSFontAttributeName, nil] retain];
    }

    textStorage = [[NSTextStorage alloc] initWithString:string attributes: data->attributes];
    [textStorage addLayoutManager: data->layoutManager];
    
    unsigned numberOfGlyphs = [data->layoutManager numberOfGlyphs];
    NSRect glyphRect = [data->layoutManager boundingRectForGlyphRange: NSMakeRange (0, numberOfGlyphs) inTextContainer: data->textContainer];

    [textStorage removeLayoutManager: data->layoutManager];
    [textStorage release];
    
    [data->boundingRectCache setObject: [NSValue valueWithRect: glyphRect] forKey: string];
        
    return glyphRect;
}




int QFontMetrics::width(QChar qc) const
{
    ushort c = qc.unicode();
    NSString *string = [NSString stringWithCharacters: (const unichar *)&c length: 1];
    int stringWidth = ROUND_TO_INT(_rectOfString(string).size.width);
    return stringWidth;
}



int QFontMetrics::width(char c) const
{
    NSString *string = [NSString stringWithCString: &c length: 1];
    int stringWidth = ROUND_TO_INT(_rectOfString(string).size.width);
    return stringWidth;
}


int QFontMetrics::width(const QString &qstring, int len) const
{
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = QSTRING_TO_NSSTRING (qstring);
    int stringWidth = ROUND_TO_INT(_rectOfString(string).size.width);
    return stringWidth;
}


int QFontMetrics::descent() const
{
    return -ROUND_TO_INT([data->font descender]);
}


QRect QFontMetrics::boundingRect(const QString &qstring, int len) const
{
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = QSTRING_TO_NSSTRING (qstring);
    NSRect rect = _rectOfString(string);

    return QRect(ROUND_TO_INT(rect.origin.x),
            ROUND_TO_INT(rect.origin.y),
            ROUND_TO_INT(rect.size.width),
            ROUND_TO_INT(rect.size.height));
}


QRect QFontMetrics::boundingRect(QChar qc) const
{
    ushort c = qc.unicode();
    NSString *string = [NSString stringWithCharacters: (const unichar *)&c length: 1];
    NSRect rect = _rectOfString(string);

    return QRect(ROUND_TO_INT(rect.origin.x),
            ROUND_TO_INT(rect.origin.y),
            ROUND_TO_INT(rect.size.width),
            ROUND_TO_INT(rect.size.height));
}


QSize QFontMetrics::size(int, const QString &qstring, int len, int tabstops, 
    int *tabarray, char **intern ) const
{
    if (tabstops != 0){
        NSLog (@"ERROR:  QFontMetrics::size() tabs not supported.\n");
    }
    
    NSLog (@"string = %@\n", QSTRING_TO_NSSTRING(qstring));
    int w, h;
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = QSTRING_TO_NSSTRING (qstring);
    w = (int)[data->font widthOfString: string];
    h = height();

    return QSize (w,h);
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


QFontMetrics &QFontMetrics::operator=(const QFontMetrics &assignFrom)
{
    _free();
    _initializeWithData(assignFrom.data);
    return *this;    
}


