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

#include <qpixmap.h>

/*
 * FIXME: This is a bad hack which really should go away.
 * Here we're using private API to get around the problem 
 * of image bytes being double freed.
 */
@interface NSData (NSDataWebCoreExtensions)
- (id)initWithBytes:(void *)bytes length:(unsigned)length copy:(BOOL)copy freeWhenDone:(BOOL)freeBytes bytesAreVM:(BOOL)vm;
@end


QPixmap::QPixmap()
{
    nsimage = nil;
}


QPixmap::QPixmap(const QSize&sz)
{
    nsimage = [[NSImage alloc] initWithSize: NSMakeSize ((float)sz.width(), (float)sz.height())];
}


QPixmap::QPixmap(const QByteArray&bytes)
{
    /*
     * FIXME: This is a bad hack which really should go away.
     * Here we're using private API to get around the problem 
     * of image bytes being double freed.
     */
    //NSData *nsdata = [[[NSData alloc] initWithBytesNoCopy: bytes.data() length: bytes.size()] autorelease];
    NSData *nsdata = [[[NSData alloc] initWithBytes: bytes.data() length: bytes.size() copy:NO freeWhenDone:NO bytesAreVM:NO] autorelease];
    nsimage = [[NSImage alloc] initWithData: nsdata];
    [nsimage setFlipped: YES];
}


QPixmap::QPixmap(int w, int h)
{
    nsimage = [[NSImage alloc] initWithSize: NSMakeSize(w, h)];
}


QPixmap::QPixmap(const QPixmap &copyFrom)
{
    if (copyFrom.nsimage != nil)
        nsimage = [copyFrom.nsimage retain];
    else
        nsimage = nil;
    xmatrix = copyFrom.xmatrix;
}


QPixmap::~QPixmap()
{
    if (nsimage != nil){
        [nsimage release];
        nsimage = nil;
    }
}


void QPixmap::setMask(const QBitmap &)
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


const QBitmap *QPixmap::mask() const
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


bool QPixmap::isNull() const
{
    if (nsimage == nil)
        return TRUE;
    return false;
}


QSize QPixmap::size() const
{
    NSSize sz = [nsimage size];
    return QSize (sz.width, sz.height);
}


QRect QPixmap::rect() const
{
    NSSize sz = [nsimage size];
    return QRect (0,0,sz.width, sz.height);
}


int QPixmap::width() const
{
    return (int)[nsimage size].width;
}


int QPixmap::height() const
{
    return (int)[nsimage size].height;
}


void QPixmap::resize(const QSize &sz)
{
    resize (sz.width(), sz.height());
}


void QPixmap::resize(int w, int h) {
    [nsimage setSize: NSMakeSize ((float)(w), (float)(h))];
}



QPixmap QPixmap::xForm(const QWMatrix &xmatrix) const
{
    QPixmap xPix = *this;
    xPix.xmatrix = xmatrix;
    return xPix;
}


QImage QPixmap::convertToImage() const
{
    NSLog (@"ERROR %s:%s:%d (NOT IMPLEMENTED)\n", __FILE__, __FUNCTION__, __LINE__);
}


QPixmap &QPixmap::operator=(const QPixmap &assignFrom)
{
    if (assignFrom.nsimage == nsimage)
        return *this;
    [nsimage release];
    nsimage = [assignFrom.nsimage retain];
    xmatrix = assignFrom.xmatrix;
    return *this;
}

