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

#import <Cocoa/Cocoa.h>

/*
    For this implementation Qt pixelSize is interpreted as Cocoa pointSize.
*/

QFont::QFont()
{
    _initialize();
}


void QFont::_initialize()
{
    _initializeWithFont (0);
}

static NSFont *_defaultNSFont = 0;

NSFont *QFont::defaultNSFont()
{
    if (_defaultNSFont == 0)
    	_defaultNSFont = [[NSFont userFontOfSize: (float)12.0] retain];
    return _defaultNSFont;
}

void QFont::_initializeWithFont (const QFont *withFont)
{
    if (withFont){
        font = [withFont->font retain];
        _family = [withFont->_family retain];
        _size = withFont->_size;
        _trait = withFont->_trait;
    }
    else {
        font = [defaultNSFont() retain];
        _family = [[font familyName] retain];
        _size = [font pointSize];
        _trait = [[NSFontManager sharedFontManager] traitsOfFont: font] & (NSBoldFontMask | NSItalicFontMask);
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

void QFont::_free(){
    [_family release];
    [font autorelease];
    font = 0;
}


// member functions --------------------------------------------------------

NSFont *QFont::getFont()
{
    if (font == nil)
        font = [[[NSFontManager sharedFontManager] fontWithFamily:_family traits:_trait weight:5 size:_size] retain];
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
    [font release];
    font = nil;
}


void QFont::setPixelSizeFloat(float sz)
{
    if (sz != _size){
        _size = sz;
        [font release];
        font = nil;
    }
}


void QFont::setWeight(int weight)
{
    if (weight == Bold){
        if (!bold()){
            [font release];
            font = nil;
        }
        _trait |= NSBoldFontMask;
    }
    else if (weight == Normal){
        if (bold()){
            [font release];
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
            [font release];
            font = nil;
        }
        _trait |= NSItalicFontMask;
        return true;
    }
    else{
        if (italic()){
            [font release];
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
    return [compareFont.font isEqual: font];
}


bool QFont::operator!=(const QFont &compareFont) const
{
    return !(operator==( compareFont ));
}
