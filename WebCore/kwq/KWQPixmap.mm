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
#include <kwqdebug.h>
#include <qbitmap.h>

QPixmap::QPixmap()
{
    nsimage = nil;
    needCopyOnWrite = false;
}

QPixmap::QPixmap(const QSize &sz)
{
    nsimage = [[NSImage alloc] initWithSize: NSMakeSize((float)sz.width(), (float)sz.height())];
    needCopyOnWrite = false;
}

QPixmap::QPixmap(const QByteArray &bytes)
{
    NSData *data = [[NSData alloc] initWithBytes: bytes.data() length: bytes.size()];
    nsimage = [[NSImage alloc] initWithData: data];
    [data release];
    if (nsimage == nil){
        KWQDEBUG("unable to create image\n");
    }
    else {
        KWQDEBUG("image created");
        [nsimage setFlipped: YES];
    }
    needCopyOnWrite = false;
}

QPixmap::QPixmap(int w, int h)
{
    nsimage = [[NSImage alloc] initWithSize: NSMakeSize(w, h)];
    needCopyOnWrite = false;
}

QPixmap::QPixmap(const QPixmap &copyFrom)
    : QPaintDevice()
{
    nsimage = [copyFrom.nsimage retain];
    needCopyOnWrite = true;
}

QPixmap::~QPixmap()
{
    [nsimage release];
}

void QPixmap::setMask(const QBitmap &)
{
    _logNotYetImplemented();
}

const QBitmap *QPixmap::mask() const
{
    return 0;
}

bool QPixmap::isNull() const
{
    return nsimage == nil;
}

QSize QPixmap::size() const
{
    NSSize sz = [nsimage size];
    return QSize((int)sz.width, (int)sz.height);
}

QRect QPixmap::rect() const
{
    NSSize sz = [nsimage size];
    return QRect(0, 0, (int)sz.width, (int)sz.height);
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
    resize(sz.width(), sz.height());
}

void QPixmap::resize(int w, int h)
{
    if (needCopyOnWrite) {
        nsimage = [nsimage copy];
        needCopyOnWrite = false;
    }
    [nsimage setScalesWhenResized: YES];
    [nsimage setSize: NSMakeSize((float)w, (float)h)];
}

QPixmap QPixmap::xForm(const QWMatrix &xmatrix) const
{
    // This function is only called when an image needs to be scaled.  
    // We can depend on render_image.cpp to call resize AFTER
    // creating a copy of the image to be scaled.   So, this
    // implementation simply returns a copy of the image.  Note,
    // this implementation depends on the implementation of
    // RenderImage::printObject.   
    return *this;
}

QImage QPixmap::convertToImage() const
{
    _logNotYetImplemented();
    return QImage();
}

QPixmap &QPixmap::operator=(const QPixmap &assignFrom)
{
    [assignFrom.nsimage retain];
    [nsimage release];
    nsimage = assignFrom.nsimage;
    needCopyOnWrite = true;
    return *this;
}
