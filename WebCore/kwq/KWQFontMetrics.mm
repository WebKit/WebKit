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

#import <Cocoa/Cocoa.h>
#import <KWQMetrics.h>
#import <KWQTextStorage.h>
#import <KWQTextContainer.h>

#define FLOOR_TO_INT(x) (int)(floor(x))
#define ROUND_TO_INT(x) (int)(((x) > (floor(x) + .5)) ? ceil(x) : floor(x))
//#define ROUND_TO_INT(f) ((int)(rint(f)))
const float LargeNumberForText = 1.0e7;


@implementation KWQLayoutFragment
- initWithString: (NSString *)str attributes: (NSDictionary *)attrs
{
    [super init];

    textStorage = [[KWQTextStorage alloc] initWithString:str attributes: attrs];
    //textContainer = [[KWQTextContainer alloc] initWithContainerSize:NSMakeSize(LargeNumberForText, LargeNumberForText)];
    layoutManager = [[NSLayoutManager alloc] init];

    [layoutManager addTextContainer: [KWQTextContainer sharedInstance]];
    [textStorage addLayoutManager: layoutManager];    

    //[textContainer setLineFragmentPadding:0.0f];

    cachedRect = NO;

#ifdef _DEBUG_LAYOUT_FRAGMENT
    _accessCount = 0;
#endif

    return self;
}

- (NSRect)boundingRect
{
    if (!cachedRect){
        unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
        boundingRect = [layoutManager boundingRectForGlyphRange: NSMakeRange (0, numberOfGlyphs) inTextContainer: [KWQTextContainer sharedInstance]];
        cachedRect = YES;
    }
#ifdef _DEBUG_LAYOUT_FRAGMENT
    _accessCount++;
#endif
    return boundingRect;
}

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (int)_accessCount { return _accessCount; }

- (NSComparisonResult)compare: (id)val
{
    if ([val _accessCount] > _accessCount)
        return NSOrderedDescending;
    else if ([val _accessCount] < _accessCount)
        return NSOrderedAscending;
    return NSOrderedSame;
}

#endif

- (void)dealloc
{
    [textStorage release];
    //[textContainer release];
    [layoutManager release];
    [super dealloc];
}
@end


static NSMutableDictionary *metricsCache = nil;

/*
    
*/
@implementation KWQLayoutInfo

+ (void)drawString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)font color: (NSColor *)color
{
    KWQLayoutInfo *metricsCache = [KWQLayoutInfo getMetricsForFont: font];
    NSLayoutManager *layoutManager = [metricsCache layoutManagerForString: string];
    if (layoutManager != nil){
        unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
        [metricsCache setColor: color];
        [metricsCache setFont: font];
        [KWQTextStorage setString: string attributes: [metricsCache attributes]];
        [layoutManager drawGlyphsForGlyphRange:NSMakeRange (0, numberOfGlyphs) atPoint:p];
    }
}

+ (void)drawUnderlineForString: (NSString *)string atPoint: (NSPoint)p withFont: (NSFont *)font color: (NSColor *)color
{
    KWQLayoutInfo *metricsCache = [KWQLayoutInfo getMetricsForFont: font];
    NSLayoutManager *layoutManager = [metricsCache layoutManagerForString: string];
    if (layoutManager != nil){
        unsigned numberOfGlyphs = [layoutManager numberOfGlyphs];
        [metricsCache setColor: color];
        [metricsCache setFont: font];
        [KWQTextStorage setString: string attributes: [metricsCache attributes]];
        NSRect lineRect = [layoutManager lineFragmentRectForGlyphAtIndex: 0 effectiveRange: 0];
        [layoutManager underlineGlyphRange:NSMakeRange (0, numberOfGlyphs) underlineType:NSSingleUnderlineStyle lineFragmentRect:lineRect lineFragmentGlyphRange:NSMakeRange (0, numberOfGlyphs) containerOrigin:p];
    }
}

#ifdef _DEBUG_LAYOUT_FRAGMENT
+ (void)_dumpLayoutCache: (NSDictionary *)fragCache
{
    int i, count;
    NSArray *stringKeys;
    NSString *string;
    KWQLayoutFragment *fragment;

    if (fragCache == nil){
        fprintf (stdout, "Fragment cache empty\n");
        return;
    }
    fprintf (stdout, "  Hits   String\n");
    
    stringKeys = [fragCache keysSortedByValueUsingSelector:@selector(compare:)];
    count = [stringKeys count];
    for (i = 0; i < count; i++){
        string = [stringKeys objectAtIndex: i];
        fragment = [fragCache objectForKey: [stringKeys objectAtIndex: i]];
        fprintf (stdout, "  %06d \"%s\"\n", [fragment _accessCount], [string cString]);
    }
}

+ (void)_dumpAllLayoutCaches
{
    int i, count;
    NSFont *font;
    NSArray *fontKeys;
    KWQLayoutInfo *layoutInfo;
    int totalObjects = 0;
    
    fontKeys = [metricsCache allKeys];
    count = [fontKeys count];
    for (i = 0; i < count; i++){
        font = [fontKeys objectAtIndex: i];
        layoutInfo = [metricsCache objectForKey: [fontKeys objectAtIndex: i]];
        fprintf (stdout, "Cache information for font %s %f (%d objects)\n", [[font displayName] cString],[font pointSize], [[layoutInfo _fragmentCache] count]);
        [KWQLayoutInfo _dumpLayoutCache: [layoutInfo _fragmentCache]];
        totalObjects += [[layoutInfo _fragmentCache] count];
    }
    fprintf (stdout, "Total cached objects %d\n", totalObjects);
}

#endif


+ (KWQLayoutInfo *)getMetricsForFont: (NSFont *)aFont
{
    KWQLayoutInfo *info = (KWQLayoutInfo *)[metricsCache objectForKey: aFont];
    if (info == nil){
        info = [[KWQLayoutInfo alloc] initWithFont: aFont];
        [KWQLayoutInfo setMetric: info forFont: aFont];
        [info release];
    }
    return info;
}


+ (void)setMetric: (KWQLayoutInfo *)info forFont: (NSFont *)aFont
{
    if (metricsCache == nil)
        metricsCache = [[NSMutableDictionary alloc] init];
    [metricsCache setObject: info forKey: aFont];
}

#ifdef _DEBUG_LAYOUT_FRAGMENT
- (NSDictionary *)_fragmentCache { return fragmentCache; }
#endif

- initWithFont: (NSFont *)aFont
{
    [super init];
    attributes = [[NSMutableDictionary dictionaryWithObjectsAndKeys:aFont, NSFontAttributeName, nil] retain];
    return self;
}


- (NSLayoutManager *)layoutManagerForString: (NSString *)string
{
    KWQLayoutFragment *cachedValue;

    if (fragmentCache == nil){
        fragmentCache = [[NSMutableDictionary alloc] init];
    }

    cachedValue = [fragmentCache objectForKey: string];
    if (cachedValue == nil){
        return nil;
    }

    return cachedValue->layoutManager;
}

- (NSRect)rectForString:(NSString *)string
 {
    KWQLayoutFragment *cachedFragment, *fragment;

    if (fragmentCache == nil){
        fragmentCache = [[NSMutableDictionary alloc] init];
    }

    cachedFragment = [fragmentCache objectForKey: string];
    if (cachedFragment != nil){
#ifdef _DEBUG_LAYOUT_FRAGMENT
        cachedFragment->_accessCount++;
#endif
        return cachedFragment->boundingRect;
    }

    fragment = [[KWQLayoutFragment alloc] initWithString: string attributes: attributes];
    [fragmentCache setObject: fragment forKey: string];        

    return [fragment boundingRect];
}
 
- (void)setColor: (NSColor *)color
{
    [attributes setObject: color forKey: NSForegroundColorAttributeName];
}

- (void)setFont: (NSFont *)aFont
{
    [attributes setObject: aFont forKey: NSFontAttributeName];
}

- (NSDictionary *)attributes
{
    return attributes;
}

- (void)dealloc
{
    [attributes release];
    [super dealloc];
}

@end


struct QFontMetricsPrivate {
friend class QFontMetrics;
public:
    QFontMetricsPrivate(NSFont *aFont) 
    {
        font = [aFont retain];
        info = [[KWQLayoutInfo getMetricsForFont: aFont] retain];
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
    KWQLayoutInfo *info;
    NSFont *font;
};


QFontMetrics::QFontMetrics()
{
    _initialize();
}


QFontMetrics::QFontMetrics(const QFont &withFont)
{
    _initializeWithFont (((QFont)withFont).getFont());
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


int QFontMetrics::baselineOffset()
{
    return ascent();
}

int QFontMetrics::ascent() const
{
    return ROUND_TO_INT([data->font ascender]);
}



int QFontMetrics::descent() const
{
    return ROUND_TO_INT(-[data->font descender]);
}


int QFontMetrics::height() const
{
    // According to Qt documentation: 
    // "This is always equal to ascent()+descent()+1 (the 1 is for the base line)."
    return ascent() + descent() + 1;
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
        string = _FAST_QSTRING_TO_NSSTRING (qstring);
    int stringWidth = ROUND_TO_INT([data->info rectForString: string].size.width);
    return stringWidth;
}


QRect QFontMetrics::boundingRect(const QString &qstring, int len) const
{
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = _FAST_QSTRING_TO_NSSTRING (qstring);
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
        string = _FAST_QSTRING_TO_NSSTRING (qstring);
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


