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

#include <kwqdebug.h>
#include <qpixmap.h>
#include <qbitmap.h>

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


@interface NSData (NSDataOpenStepExtensions)

- (id)initWithBytes:(void *)bytes length:(unsigned)length copy:(BOOL)copy freeWhenDone:(BOOL)freeBytes bytesAreVM:(BOOL)vm;

- (BOOL)_bytesAreVM;

@end

QPixmap::QPixmap(const QByteArray&bytes)
{
    /*
     * FIXME: This is a bad hack which really should go away.
     * Here we're using private API to get around the problem 
     * of image bytes being double freed.
     */
    NSData *nsdata = [[NSData alloc] initWithBytes: bytes.data() length: bytes.size() copy:NO freeWhenDone:NO bytesAreVM:NO];
    nsimage = [[NSImage alloc] initWithData: nsdata];
    // FIXME: Workaround for Radar 2890624 (Double free of image data in QPixmap)
    // This else if code block should be removed when we pick up changes from AppKit
    // image code that are on HEAD now, but are not in Jaguar <= 6B63
    // When we pick up the change from AppKit, uncomment the next line and release the data here
    //[nsdata release];
    if (nsimage == nil){
        KWQDEBUG("unable to create image\n");
        // FIXME: delete next line when Radar 2890624
        [nsdata release];
    } 
    else if ([[nsimage representations] count] == 0) {
        KWQDEBUG("unable to create image [can't decode bytes]\n");
        // leak the ns image pointer
        // note that the data is freed erroneously by AppKit
        // we're not leaking that, just the nsimage pointer
        nsimage = nil;
    }
    else {
        KWQDEBUG("image created\n");
        // FIXME: delete next line when Radar 2890624
        [nsdata release];
        [nsimage setFlipped: YES];
        [nsimage setScalesWhenResized: YES];
    }
}


QPixmap::QPixmap(int w, int h)
{
    nsimage = [[NSImage alloc] initWithSize: NSMakeSize(w, h)];
}


QPixmap::QPixmap(const QPixmap &copyFrom)
{
    if (copyFrom.nsimage != nil){
        // Do a deep copy of the image.  This is required because the image
        // may be transformed, i.e. scaled.
        nsimage = [copyFrom.nsimage copyWithZone: [copyFrom.nsimage zone]];    
    }
    else
        nsimage = nil;
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
    _logNotYetImplemented();
}

static QBitmap *theMask = NULL;
const QBitmap *QPixmap::mask() const
{
    _logNotYetImplemented();

    if (theMask == NULL) {
	theMask = new QBitmap();
    }

    return theMask;
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
    // This function is only called when an image needs to be scaled.  
    // We can depend on render_image.cpp to call resize AFTER
    // creating a copy of the image to be scaled.   So, this
    // implementation simply returns a copy of the image.  Note,
    // this implementation depends on the implementation of
    // RenderImage::printObject.   
    QPixmap xPix = *this;
    [xPix.nsimage setScalesWhenResized: YES];
    return xPix;
}


QImage QPixmap::convertToImage() const
{
    _logNotYetImplemented();
    return QImage();
}


QPixmap &QPixmap::operator=(const QPixmap &assignFrom)
{
    if (assignFrom.nsimage == nsimage)
        return *this;
    [nsimage release];
    nsimage = [assignFrom.nsimage retain];
    return *this;
}

