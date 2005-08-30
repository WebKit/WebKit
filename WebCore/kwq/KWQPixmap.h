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

#ifndef QPIXMAP_H_
#define QPIXMAP_H_

#include "KWQPaintDevice.h"
#include "KWQColor.h"
#include "KWQString.h"
#include "KWQNamespace.h"
#include "KWQImage.h"
#include "KWQSize.h"
#include "KWQRect.h"

#include <ApplicationServices/ApplicationServices.h>

#ifdef __OBJC__
@protocol WebCoreImageRenderer;
typedef id <WebCoreImageRenderer> WebCoreImageRendererPtr;
@class NSString;
#else
class WebCoreImageRenderer;
typedef WebCoreImageRenderer *WebCoreImageRendererPtr;
class NSString;
#endif

class QWMatrix;
class QPainter;
class QPixmap;

namespace khtml
{
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
    QPixmap(const QByteArray&, NSString *MIMEType);
    QPixmap(int, int);
    QPixmap(WebCoreImageRendererPtr);
    QPixmap(const QPixmap &);
    ~QPixmap();
    
    bool isNull() const;

    QSize size() const;
    QRect rect() const;
    int width() const;
    int height() const;
    void resize(const QSize &);
    void resize(int, int);

    QPixmap xForm(const QWMatrix &) const;
    QImage convertToImage() const { return QImage(); }
    
    bool mask() const;

    QPixmap &operator=(const QPixmap &);

    bool receivedData(const QByteArray &bytes, bool isComplete, khtml::CachedImageCallback *decoderCallback);
    void stopAnimations();

    WebCoreImageRendererPtr image() { return imageRenderer; };

    void increaseUseCount() const;
    void decreaseUseCount() const;
    
    void flushRasterCache();
    
    CGImageRef imageRef();
    
    static bool shouldUseThreadedDecoding();

    void resetAnimation();
    void setAnimationRect(const QRect&) const;

private:

    WebCoreImageRendererPtr imageRenderer;
        
    mutable bool needCopyOnWrite;
    
    NSString *MIMEType;
    
    friend class QPainter;

};

#endif
