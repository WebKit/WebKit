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

#include <qfontmetrics.h>


QFontMetrics::QFontMetrics()
{
    _initialize();
}


QFontMetrics::QFontMetrics(const QFont &withFont)
{
    _initializeWithFont (withFont.data->font);
}


QFontMetrics::QFontMetrics(const QFontMetrics &copyFrom)
{
    struct KWQFontMetricsData *oldData = data;
    _initializeWithFont(copyFrom.data->font);
    _freeWithData (oldData);
}

void QFontMetrics::_initialize()
{
    _initializeWithFont (0);
}

void QFontMetrics::_initializeWithFont (NSFont *withFont)
{
    data = (struct KWQFontMetricsData *)calloc (1, sizeof (struct KWQFontMetricsData));
    if (withFont == 0)
        data->font = QFont::defaultNSFont();
    else
        data->font = [withFont retain];
}

void QFontMetrics::_free(){
    _freeWithData (data);
}


void QFontMetrics::_freeWithData(struct KWQFontMetricsData *freeData){
    if (freeData != 0){
        [freeData->font release];
        free (freeData);
    }
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
    return (int)([data->font defaultLineHeightForFont] + [data->font descender]);
}


int QFontMetrics::height() const
{
    // According to Qt documentation: 
    // "This is always equal to ascent()+descent()+1 (the 1 is for the base line)."
    // However, the [font defaultLineHeightForFont] seems more appropriate.
    return (int)[data->font defaultLineHeightForFont];
}


int QFontMetrics::width(QChar qc) const
{
    ushort c = qc.unicode();
    NSString *string = [NSString stringWithCharacters: (const unichar *)&c length: 1];
    return (int)[data->font widthOfString: string];
}


int QFontMetrics::width(char c) const
{
    NSString *string = [NSString stringWithCString: &c length: 1];
    return (int)[data->font widthOfString: string];
}


int QFontMetrics::width(const QString &qstring, int len) const
{
    NSString *string;

    if (len != -1)
        string = QSTRING_TO_NSSTRING_LENGTH (qstring, len);
    else
        string = QSTRING_TO_NSSTRING (qstring);
    return (int)[data->font widthOfString: string];
}


int QFontMetrics::descent() const
{
    return -(int)[data->font descender];
}


QRect QFontMetrics::boundingRect(const QString &, int len=-1) const
{
    NSLog (@"WARNING (NOT IMPLEMENTED) QRect QFontMetrics::boundingRect(const QString &, int len=-1)\n");
}


QRect QFontMetrics::boundingRect(QChar) const
{
    NSLog (@"WARNING (NOT IMPLEMENTED) QFontMetrics::boundingRect(QChar)\n");
}


QSize QFontMetrics::size(int, const QString &, int len=-1, int tabstops=0, 
    int *tabarray=0, char **intern=0 ) const
{
    NSLog (@"WARNING (NOT IMPLEMENTED) QSize QFontMetrics::size(int, const QString &, int len=-1, int tabstops=0, int *tabarray=0, char **intern=0 )\n");
}


int QFontMetrics::rightBearing(QChar) const
{
    NSLog (@"WARNING (NOT IMPLEMENTED) QFontMetrics::rightBearing(QChar)\n");
    return 0;
}


int QFontMetrics::leftBearing(QChar) const
{
    NSLog (@"WARNING (NOT IMPLEMENTED) leftBearing(QChar)\n");
    return 0;
}


QFontMetrics &QFontMetrics::operator=(const QFontMetrics &assignFrom)
{
    _free();
    _initializeWithFont(assignFrom.data->font);
    return *this;    
}


