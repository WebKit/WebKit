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

NSFont *QFont::defaultNSFont()
{
    return [NSFont userFontOfSize: (float)12.0];
}

void QFont::_initializeWithFont (const QFont *withFont)
{
    if (withFont == 0)
        // Hmm...  What size should we use as a default font?
        font = [defaultNSFont() retain];
    else
        font = [withFont->font retain];
}


QFont::QFont(const QFont &copyFrom)
{
    _initializeWithFont(&copyFrom);
    // FIXME: why were we freeing here?
    //_free();
}


QFont::~QFont()
{
    _free();
}

void QFont::_free(){
    [font autorelease];
    font = 0;
}


// member functions --------------------------------------------------------

int QFont::pixelSize() const
{
    return (int)[font pointSize];
}


QString QFont::family() const
{
    return NSSTRING_TO_QSTRING([font familyName]);
}


void QFont::setFamily(const QString &qfamilyName)
{
    NSString *familyName;
    NSFont *oldFont = font;
    
    familyName = QSTRING_TO_NSSTRING(qfamilyName);
    font = [[[NSFontManager sharedFontManager] convertFont: oldFont toFamily: familyName] retain];

    [oldFont release];
}


void QFont::setPixelSizeFloat(float sz)
{
    NSFont *oldFont = font;

    font = [[[NSFontManager sharedFontManager] convertFont: oldFont toSize: sz] retain];

    [oldFont release];
}


void QFont::setWeight(int weight)
{
    if (weight == Bold)
        _setTrait (NSBoldFontMask);
    else if (weight == Normal)
        _setTrait (NSUnboldFontMask);
}


int QFont::weight() const
{
    if ([[NSFontManager sharedFontManager] traitsOfFont: font] & NSBoldFontMask)
        return Bold;
    return Normal;
}


bool QFont::setItalic(bool flag)
{
    if (flag)
        _setTrait (NSItalicFontMask);
    else
        _setTrait (NSUnitalicFontMask);
}


bool QFont::italic() const
{
    if ([[NSFontManager sharedFontManager] traitsOfFont: font] & NSItalicFontMask)
        return TRUE;
    return FALSE;
}


bool QFont::bold() const
{
    if ([[NSFontManager sharedFontManager] traitsOfFont: font] & NSItalicFontMask)
        return TRUE;
    return FALSE;
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


void QFont::_setTrait (NSFontTraitMask mask)
{
    NSFont *oldFont = font;

    font = [[[NSFontManager sharedFontManager] convertFont: oldFont toHaveTrait: mask] retain];

    [oldFont release];
}
