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

#include <qstring.h>
#include <qfont.h>
#include <kwqdebug.h>

#import <Cocoa/Cocoa.h>

/*
    For this implementation Qt pixelSize is interpreted as Cocoa pointSize.
*/

static NSFont *defaultFont = 0;
static NSString *defaultFontFamilyName;
static float defaultFontSize;
static int defaultFontTrait;

static void
loadDefaultFont()
{
    if (defaultFont)
        return;
    defaultFont = [[NSFont userFontOfSize: (float)12.0] retain];
    defaultFontFamilyName = [[defaultFont familyName] retain];
    defaultFontSize = [defaultFont pointSize];
    defaultFontTrait = [[NSFontManager sharedFontManager] traitsOfFont:defaultFont] & (NSBoldFontMask | NSItalicFontMask);
}

QFont::QFont()
{
    _initializeWithFont(0);
}

void QFont::_initializeWithFont(const QFont *withFont)
{
    if (withFont) {
        font = [withFont->font retain];
        _family = [withFont->_family retain];
        _size = withFont->_size;
        _trait = withFont->_trait;
    } else {
        loadDefaultFont();
        font = [defaultFont retain];
        _family = [defaultFontFamilyName retain];
        _size = defaultFontSize;
        _trait = defaultFontTrait;
    }
}

QFont::QFont(const QFont &copyFrom)
{
    _initializeWithFont(&copyFrom);
}

QFont::~QFont()
{
    _free();
}

void QFont::_free()
{
    [_family release];
    [font autorelease];
    font = nil;
}

// member functions --------------------------------------------------------

#ifdef DEBUG_GETFONT
static int getFontCount = 0;
#endif

static NSMutableDictionary *fontCache = 0;

// IFObjectHolder holds objects as keys in dictionaries without
// copying.
@interface IFFontCacheKey : NSObject
{
    NSString *string;
    int trait;
    float size;
}

- initWithString: (NSString *)str trait: (int)t size: (float)s;

@end

@implementation IFFontCacheKey

- initWithString: (NSString *)str trait: (int)t size: (float)s;
{
    [super init];
    string = [str retain];
    trait = t;
    size = s;
    return self;
}

- (void)dealloc
{
    [string release];
    [super dealloc];
}

- (id)copyWithZone:(NSZone *)zone
{
    return [self retain];
}

- (unsigned)hash
{
    return [string hash];
}

- (NSString *)string
{
    return string;
}

- (int)trait { return trait; }
- (float)size { return size; }

- (BOOL)isEqual:(id)o
{
    IFFontCacheKey *anObject = o;
    if ([string isEqual: [anObject string]] && trait == [anObject trait] && size == [anObject size])
        return YES;
    return NO;
}

@end

static NSArray *_availableFamiles = 0;

NSFont *QFont::getFont()
{
    if (font == nil) {
        NSString *fontKey;
#ifdef DEBUG_GETFONT
        getFontCount++;
        fprintf (stdout, "getFountCount = %d, family = %s, traits = 0x%08x, size = %f\n", getFontCount, [_family cString], _trait, _size);
#endif
        if (fontCache == nil){
            fontCache = [[NSMutableDictionary alloc] init];
        }
        fontKey = [[IFFontCacheKey alloc] initWithString: _family trait: _trait size: _size];
        font = [fontCache objectForKey:fontKey];
        if (font == nil) {
            font = [[NSFontManager sharedFontManager] fontWithFamily:_family traits:_trait weight:5 size:_size];
            if (font == nil) {
                if (_availableFamiles == 0)
                    _availableFamiles = [[[NSFontManager sharedFontManager] availableFontFamilies] retain];
                    
                // FIXME:  For now do a simple case insensitive search for a matching font.
                // The font manager requires exact name matches.  The will at least address the problem
                // of matching arial to Arial, etc.
                int i, count = [_availableFamiles count];
                NSString *actualFamily;
                for (i = 0; i < count; i++){
                    actualFamily = [_availableFamiles objectAtIndex: i];
                    if ([_family caseInsensitiveCompare: actualFamily] == NSOrderedSame){
                        [_family release];
                        _family = [actualFamily retain];
                        font = [[NSFontManager sharedFontManager] fontWithFamily:_family traits:_trait weight:5 size:_size];
                        break;
                    }
                }
                if (font == nil){
                    KWQDEBUGLEVEL1(KWQ_LOG_FONTCACHE, "unable to find font for family %s\n", [_family cString]);
                    loadDefaultFont();
                    font = [[NSFontManager sharedFontManager] fontWithFamily:defaultFontFamilyName traits:_trait weight:5 size:_size];
                }
            }
            [fontCache setObject: font forKey: fontKey];
        }
        [fontKey release];
    }
    return font;
}
        

int QFont::pixelSize() const
{
    return (int)_size;
}


QString QFont::family() const
{
    return NSSTRING_TO_QSTRING(_family);
}


void QFont::setFamily(const QString &qfamilyName)
{
    [_family release];
    _family = [_FAST_QSTRING_TO_NSSTRING(qfamilyName) retain];
    [font autorelease];
    font = nil;
}


void QFont::setPixelSizeFloat(float sz)
{
    if (sz != _size){
        _size = sz;
        [font autorelease];
        font = nil;
    }
}


void QFont::setWeight(int weight)
{
    if (weight == Bold){
        if (!bold()) {
            [font autorelease];
            font = nil;
        }
        _trait |= NSBoldFontMask;
    }
    else if (weight == Normal){
        if (bold()){
            [font autorelease];
            font = nil;
        }
        _trait = _trait & (~NSBoldFontMask);
    }
}


int QFont::weight() const
{
    if (_trait == NSBoldFontMask)
        return Bold;
    return Normal;
}


bool QFont::setItalic(bool flag)
{
    if (flag){
        if (!italic()){
            [font autorelease];
            font = nil;
        }
        _trait |= NSItalicFontMask;
        return true;
    }
    else{
        if (italic()){
            [font autorelease];
            font = nil;
        }
        _trait = _trait & (~NSItalicFontMask);
        return false;
    }
}


bool QFont::italic() const
{
    return _trait & NSItalicFontMask ? TRUE : FALSE;
}


bool QFont::bold() const
{
    return _trait & NSBoldFontMask ? TRUE : FALSE;
}


// operators ---------------------------------------------------------------

QFont &QFont::operator=(const QFont &assignFrom)
{
    _free();
    _initializeWithFont(&assignFrom);
    return *this;
}


bool QFont::operator==(const QFont &compareFont) const
{
    // FIXME: This does not do the right thing when the font is nil
    return [compareFont.font isEqual: font];
}


bool QFont::operator!=(const QFont &compareFont) const
{
    return !(operator==( compareFont ));
}
