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

#include "config.h"
#import "KWQPixmap.h"
#import "IntSize.h"
#import "IntRect.h"

#import "CachedImageCallback.h"
#import "KWQFoundationExtras.h"
#import "WebCoreImageRenderer.h"
#import "WebCoreImageRendererFactory.h"

using namespace khtml;

QPixmap *KWQLoadPixmap(const char *name)
{
    QPixmap *p = new QPixmap([[WebCoreImageRendererFactory sharedFactory] imageRendererWithName:[NSString stringWithCString:name]]);
    return p;
}

bool canRenderImageType(const QString& type)
{
    return [[[WebCoreImageRendererFactory sharedFactory] supportedMIMETypes] containsObject:type.getNSString()];
}

QPixmap::QPixmap()
{
    m_imageRenderer = nil;
    m_MIMEType = nil;
    m_needCopyOnWrite = false;
}

QPixmap::QPixmap(WebCoreImageRendererPtr r)
{
    m_imageRenderer = KWQRetain(r);
    m_MIMEType = nil;
    m_needCopyOnWrite = false;
}

QPixmap::QPixmap(void *MIME)
{
    m_imageRenderer = nil;
    m_MIMEType = KWQRetainNSRelease([(NSString *)MIME copy]);
    m_needCopyOnWrite = false;
}

QPixmap::QPixmap(const IntSize& sz)
{
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithSize:sz]);
    m_MIMEType = nil;
    m_needCopyOnWrite = false;
}

QPixmap::QPixmap(const ByteArray& bytes)
{
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithBytes:bytes.data() length:bytes.size()]);
    m_MIMEType = nil;
    m_needCopyOnWrite = false;
}

QPixmap::QPixmap(const ByteArray& bytes, NSString *MIME)
{
    m_MIMEType = KWQRetainNSRelease([MIME copy]);
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithBytes:bytes.data() length:bytes.size() MIMEType:m_MIMEType]);
    m_needCopyOnWrite = false;
}

QPixmap::QPixmap(int w, int h)
{
    m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithSize:NSMakeSize(w, h)]);
    m_MIMEType = nil;
    m_needCopyOnWrite = false;
}

QPixmap::QPixmap(const QPixmap& copyFrom) : QPaintDevice(copyFrom)
{
    m_imageRenderer = KWQRetainNSRelease([copyFrom.m_imageRenderer copyWithZone:NULL]);;
    m_needCopyOnWrite = false;
    m_MIMEType = KWQRetainNSRelease([copyFrom.m_MIMEType copy]);
}

QPixmap::~QPixmap()
{
    KWQRelease(m_MIMEType);
    KWQRelease(m_imageRenderer);
}

CGImageRef QPixmap::imageRef() const
{
    return [m_imageRenderer imageRef];
}

void QPixmap::resetAnimation()
{
    if (m_imageRenderer)
        [m_imageRenderer resetAnimation];
}

void QPixmap::setAnimationRect(const IntRect& rect) const
{
    [m_imageRenderer setAnimationRect:NSMakeRect(rect.x(), rect.y(), rect.width(), rect.height())];
}

@interface WebImageCallback : NSObject
{
    CachedImageCallback *callback;
    CGImageSourceStatus status;
}
- (void)notify;
- (void)setImageSourceStatus:(CGImageSourceStatus)status;
- (CGImageSourceStatus)status;
@end

@implementation WebImageCallback
- initWithCallback:(CachedImageCallback *)c
{
    self = [super init];
    callback = c;
    c->ref();
    return self;
}

- (void)_commonTermination
{
    callback->deref();
}

- (void)dealloc
{
    [self _commonTermination];
    [super dealloc];
}

- (void)finalize
{
    [self _commonTermination];
    [super finalize];
}

- (void)notify
{
    if (status < kCGImageStatusReadingHeader)
        callback->notifyDecodingError();
    else if (status == kCGImageStatusIncomplete)
        callback->notifyUpdate();
    else if (status == kCGImageStatusComplete)
        callback->notifyFinished();
}

- (void)setImageSourceStatus:(CGImageSourceStatus)s
{
    status = s;
}

- (CGImageSourceStatus)status
{
    return status;
}

@end

bool QPixmap::shouldUseThreadedDecoding()
{
    return [WebCoreImageRendererFactory shouldUseThreadedDecoding] ? true : false;
}

bool QPixmap::receivedData(const ByteArray& bytes, bool isComplete, CachedImageCallback *decoderCallback)
{
    if (!m_imageRenderer)
        m_imageRenderer = KWQRetain([[WebCoreImageRendererFactory sharedFactory] imageRendererWithMIMEType:m_MIMEType]);
    
    WebImageCallback *callbackWrapper = nil;
    if (decoderCallback)
        callbackWrapper = [[WebImageCallback alloc] initWithCallback:decoderCallback];

    bool result = [m_imageRenderer incrementalLoadWithBytes:bytes.data() length:bytes.size() complete:isComplete callback:callbackWrapper];

    [callbackWrapper release];
    
    return result;
}

bool QPixmap::mask() const
{
    return false;
}

bool QPixmap::isNull() const
{
    return !m_imageRenderer || [m_imageRenderer isNull];
}

IntSize QPixmap::size() const
{
    if (!m_imageRenderer)
        return IntSize(0, 0);
    return IntSize([m_imageRenderer size]);
}

IntRect QPixmap::rect() const
{
    return IntRect(IntPoint(0, 0), size());
}

int QPixmap::width() const
{
    return size().width();
}

int QPixmap::height() const
{
    return size().height();
}

void QPixmap::resize(const IntSize& sz)
{
    resize(sz.width(), sz.height());
}

void QPixmap::resize(int w, int h)
{
    if (m_needCopyOnWrite) {
        id <WebCoreImageRenderer> newImageRenderer = KWQRetainNSRelease([m_imageRenderer copyWithZone:NULL]);
        KWQRelease(m_imageRenderer);
        m_imageRenderer = newImageRenderer;
        m_needCopyOnWrite = false;
    }
    [m_imageRenderer resize:NSMakeSize(w, h)];
}

QPixmap& QPixmap::operator=(const QPixmap& assignFrom)
{
    id <WebCoreImageRenderer> oldImageRenderer = m_imageRenderer;
    m_imageRenderer = KWQRetainNSRelease([assignFrom.m_imageRenderer retainOrCopyIfNeeded]);
    KWQRelease(oldImageRenderer);
    NSString *newMIMEType = KWQRetainNSRelease([assignFrom.m_MIMEType copy]);
    KWQRelease(m_MIMEType);
    m_MIMEType = newMIMEType;
    assignFrom.m_needCopyOnWrite = true;
    m_needCopyOnWrite = true;
    return *this;
}

void QPixmap::increaseUseCount() const
{
    [m_imageRenderer increaseUseCount];
}

void QPixmap::decreaseUseCount() const
{
    [m_imageRenderer decreaseUseCount];
}

void QPixmap::stopAnimations()
{
    [m_imageRenderer stopAnimation];
}

void QPixmap::flushRasterCache()
{
    [m_imageRenderer flushRasterCache];
}
