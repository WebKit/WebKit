/*
 * Copyright (C) 2004, 2005 Apple Computer, Inc.  All rights reserved.
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

#ifndef QPIXMAP_H_
#define QPIXMAP_H_

#include "KWQNamespace.h"
#include "KWQPaintDevice.h"
#include "KWQString.h"

#if __APPLE__
#include <ApplicationServices/ApplicationServices.h>

#ifdef __OBJC__
@protocol WebCoreImageRenderer;
typedef id <WebCoreImageRenderer> WebCoreImageRendererPtr;
@class NSString;
#else
class WebCoreImageRenderer;
typedef WebCoreImageRenderer *WebCoreImageRendererPtr;
class NSString;
#endif // __OBJC__

#endif // __APPLE__

class QWMatrix;
class QPainter;
class QPixmap;
class QRect;
class QSize;

namespace khtml {
    class CachedImageCallback;
}

bool canRenderImageType(const QString &type);
QPixmap *KWQLoadPixmap(const char *name);

class QPixmap : public QPaintDevice, public Qt {
public:
    QPixmap();
    QPixmap(void *MIMEType);
    QPixmap(const QSize&);
    QPixmap(const QByteArray&);
#if __APPLE__
    QPixmap(const QByteArray&, NSString *MIMEType);
#endif
    QPixmap(int, int);
#if __APPLE__
    QPixmap(WebCoreImageRendererPtr);
#endif
    QPixmap(const QPixmap &);
    ~QPixmap();
    
    bool isNull() const;

    QSize size() const;
    QRect rect() const;
    int width() const;
    int height() const;
    void resize(const QSize &);
    void resize(int, int);

    bool mask() const;

    QPixmap &operator=(const QPixmap &);

    bool receivedData(const QByteArray &bytes, bool isComplete, khtml::CachedImageCallback *decoderCallback);
    void stopAnimations();

#if __APPLE__
    WebCoreImageRendererPtr imageRenderer() const { return m_imageRenderer; }
    CGImageRef imageRef() const;
#endif

    void increaseUseCount() const;
    void decreaseUseCount() const;
    
    void flushRasterCache();
   
    static bool shouldUseThreadedDecoding();

    void resetAnimation();
    void setAnimationRect(const QRect&) const;

private:
#if __APPLE__
    WebCoreImageRendererPtr m_imageRenderer;
    NSString *m_MIMEType;
#endif
    mutable bool m_needCopyOnWrite;
    
    friend class QPainter;

};

#endif
