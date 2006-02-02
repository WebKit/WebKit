/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#import "config.h"
#import "KWQCursor.h"

#import "KWQFoundationExtras.h"
#import "WebCoreImageRenderer.h"

namespace WebCore {

static NSCursor *createCustomCursor(const Image& image)
{
    // FIXME: The cursor won't animate properly.  Not sure if that's a big deal.
    NSImage *img = image.getNSImage();
    if (!img)
        return nil;
    return [[NSCursor alloc] initWithImage:img hotSpot:NSZeroPoint];
}

QCursor::QCursor()
    : cursor(nil)
{
}

QCursor::QCursor(const Image& image)
    : cursor(createCustomCursor(image))
{
}

QCursor::QCursor(const QCursor& other)
    : cursor(KWQRetain(other.cursor))
{
}

QCursor::~QCursor()
{
    KWQRelease(cursor);
}

QCursor QCursor::makeWithNSCursor(NSCursor * c)
{
    QCursor q;
    q.cursor = KWQRetain(c);
    return q;
}
      
QCursor& QCursor::operator=(const QCursor& other)
{
    KWQRetain(other.cursor);
    KWQRelease(cursor);
    cursor = other.cursor;
    return *this;
}

NSCursor *QCursor::handle() const
{
    return cursor;
}

}
