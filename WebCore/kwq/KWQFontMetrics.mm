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
const float LargeNumberForText = 1.0e7;

@interface WSMetricsInfo : NSObject
{
    NSFont *font;
    NSTextContainer *textContainer;
    NSLayoutManager *layoutManager;
    NSDictionary *attributes;
    NSMutableDictionary *boundingRectCache;
}

+ (WSMetricsInfo *)getMetricsForFont: (NSFont *)aFont;
+ (void)setMetric: (WSMetricsInfo *)info forFont: (NSFont *)aFont;
- initWithFont: (NSFont *)aFont;
- (NSRect)rectForString:(NSString *)string;

@end

static NSMutableDictionary *metricsCache = nil;

@implementation WSMetricsInfo
+ (WSMetricsInfo *)getMetricsForFont: (NSFont *)aFont
{
    WSMetricsInfo *info = (WSMetricsInfo *)[metricsCache objectForKey: aFont];
    if (info == nil){
        info = [[WSMetricsInfo alloc] initWithFont: aFont];
        [WSMetricsInfo setMetric: info forFont: aFont];
        [info release];
    }
    return info;
}
+ (void)setMetric: (WSMetricsInfo *)info forFont: (NSFont *)aFont
{
    if (metricsCache == nil)
        metricsCache = [[NSMutableDictionary alloc] init];
    [metricsCache setObject: info forKey: aFont];
}

- initWithFont: (NSFont *)aFont
{
    [super init];
    font = [aFont retain];
    textContainer = [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
    layoutManager = [[NSLayoutManager alloc] init];
    [layoutManager addTextContainer: textContainer];
    attributes = [[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil] retain];
    return self;
}

- (NSRect)rectForString:(NSString *)string
 {
    NSValue *cachedValue;
    NSTextStorage *textStorage;

    if (boundingRectCache == nil){
        boundingRectCache = [[NSMutableDictionary alloc] init];
    }

    cachedValue = [boundingRectCache objectForKey: string];
    if (cachedValue != nil){
        return [cachedValue rectValue];
    }
    
    if (textContainer == nil){
        textContainer = [[NSTextContainer alloc] initWithContainerSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
        layoutManager = [[NSLayoutManager alloc] init];
        [layoutManager addTextContainer: textContainer];
        attributes = [[NSDictionary dictionaryWithObjectsAndKeys:font, NSFontAttributeName, nil] retain];
    }

    textStorage = [[NSTextStorage alloc] initWithString:string attributes: attributes];
    [textStorage addLayoutManager: layoutManager];
    
    unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
    NSRect glyphRect = [layoutManager boundingRectForGlyphRange: NSMakeRange (0, numberOfGlyphs) inTextContainer: textContainer];

    [textStorage removeLayoutManager: layoutManager];
    [textStorage release];
    
    [boundingRectCache setObject: [NSValue valueWithRect: glyphRect] forKey: string];
        
    return glyphRect;
}
 
- (void)dealloc
{
    [textContainer release];
    [layoutManager release];
    [attributes release];
}

@end


struct QFontMetricsPrivate {
friend class QFontMetrics;
public:
    QFontMetricsPrivate(NSFont *aFont) 
    {
        font = [aFont retain];
        info = [[WSMetricsInfo getMetricsForFont: aFont] retain];
    }
    
    ~QFontMetricsPrivate()
    {
        if (info){
            [info release];
            info = 0;
        }
        if (font){
            [font release];
            font = 0;
        }
    }
    
private:
    WSMetricsInfo *info;
    NSFont *font;
};


QFontMetrics::QFontMetrics()
{
    _initialize();
}


QFontMetrics::QFontMetrics(const QFont &withFont)
{
    _initializeWithFont (withFont.font);
}


QFontMetrics::QFontMetrics(const QFontMetrics &copyFrom)
{
    _initializeWithFont(copyFrom.data->font);
}

void QFontMetrics::_initialize()
{
    _initializeWithFont (0);
}

void QFontMetrics::_initializeWithFont (NSFont *font)
{
    data = new QFontMetricsPrivate(font);
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


int QFontMetrics::width(QChar qc) const
{
    ushort c = qc.unicode();
    NSString *string = [NSString stringWithCharacters: (const unichar *)&c length: 1];
    int stringWidth = ROUND_TO_INT([data->info rectForString: string].size.width);
    return stringWidth;
}



int QFontMetrics::width(char c) const
{
    NSString *string = [NSString stringWithCString: &c length: 1];
    int stringWidth = ROUND_TO_INT([data->info rectForString: string].size.width);
    return stringWidth;
}


int QFontMetrics::width(const QString &qstring, int len) const
{
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = QSTRING_TO_NSSTRING (qstring);
    int stringWidth = ROUND_TO_INT([data->info rectForString: string].size.width);
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
    NSRect rect = [data->info rectForString: string];

    return QRect(ROUND_TO_INT(rect.origin.x),
            ROUND_TO_INT(rect.origin.y),
            ROUND_TO_INT(rect.size.width),
            ROUND_TO_INT(rect.size.height));
}


QRect QFontMetrics::boundingRect(QChar qc) const
{
    ushort c = qc.unicode();
    NSString *string = [NSString stringWithCharacters: (const unichar *)&c length: 1];
    NSRect rect = [data->info rectForString: string];

    return QRect(ROUND_TO_INT(rect.origin.x),
            ROUND_TO_INT(rect.origin.y),
            ROUND_TO_INT(rect.size.width),
            ROUND_TO_INT(rect.size.height));
}


QSize QFontMetrics::size(int, const QString &qstring, int len, int tabstops, 
    int *tabarray, char **intern ) const
{
    if (tabstops != 0){
        KWQDEBUGLEVEL(KWQ_LOG_ERROR, "ERROR:  QFontMetrics::size() tabs not supported.\n");
    }
    
    KWQDEBUG1("string = %s\n", DEBUG_OBJECT(QSTRING_TO_NSSTRING(qstring)));
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = QSTRING_TO_NSSTRING (qstring);
    NSRect rect = [data->info rectForString: string];

    return QSize (ROUND_TO_INT(rect.size.width),ROUND_TO_INT(rect.size.height));
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
    _initializeWithFont(assignFrom.data->font);
    return *this;    
}


