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
    data = (struct KWQFontData *)calloc (1, sizeof (struct KWQFontData));
    if (withFont == 0 || withFont->data == 0)
        // Hmm...  What size should we use as a default font?
        data->font = [defaultNSFont() retain];
    else
        data->font = [withFont->data->font retain];
}


QFont::QFont(const QFont &copyFrom)
{
    struct KWQFontData *oldData = data;
    _initializeWithFont(&copyFrom);
    _freeWithData (oldData);
}


QFont::~QFont()
{
    _free();
}

void QFont::_free(){
    _freeWithData (data);
}


void QFont::_freeWithData(struct KWQFontData *freeData){
    if (freeData != 0){
        if (freeData->font != nil)
            [freeData->font release];
        free (freeData);
    }
}


// member functions --------------------------------------------------------

int QFont::pixelSize() const
{
    return (int)[data->font pointSize];
}


QString QFont::family() const
{
    return NSSTRING_TO_QSTRING([data->font familyName]);
}


void QFont::setFamily(const QString &qfamilyName)
{
    NSString *familyName;
    NSFont *oldFont = data->font;
    
    familyName = QSTRING_TO_NSSTRING(qfamilyName);
    data->font = [[[NSFontManager sharedFontManager] convertFont: data->font toFamily: familyName] retain];

    [oldFont release];
}


void QFont::setPixelSizeFloat(float sz)
{
    NSFont *oldFont = data->font;

    data->font = [[[NSFontManager sharedFontManager] convertFont: data->font toSize: sz] retain];

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
    if ([[NSFontManager sharedFontManager] traitsOfFont: data->font] & NSBoldFontMask)
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
    if ([[NSFontManager sharedFontManager] traitsOfFont: data->font] & NSItalicFontMask)
        return TRUE;
    return FALSE;
}


bool QFont::bold() const
{
    if ([[NSFontManager sharedFontManager] traitsOfFont: data->font] & NSItalicFontMask)
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
    return [compareFont.data->font isEqual: data->font];
}


bool QFont::operator!=(const QFont &compareFont) const
{
    return !(operator==( compareFont ));
}


void QFont::_setTrait (NSFontTraitMask mask)
{
    NSFont *oldFont = data->font;

    data->font = [[[NSFontManager sharedFontManager] convertFont: data->font toHaveTrait: mask] retain];

    [oldFont release];
}
