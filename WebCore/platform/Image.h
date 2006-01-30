/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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

#ifndef IMAGE_H_
#define IMAGE_H_

#include "KWQNamespace.h"
#include "KWQString.h"
#include "IntSize.h"
#include "IntRect.h"

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

namespace WebCore {

class QPainter;

class Image {
public:
    Image();
    Image(const QString& type);
    Image(const IntSize&);
    Image(const ByteArray&, const QString& type);
    Image(int, int);
#if __APPLE__
    Image(WebCoreImageRendererPtr);
#endif

    ~Image();
    
    static Image* loadResource(const char *name);
    static bool supportsType(const QString& type);

    bool isNull() const;

    IntSize size() const;
    IntRect rect() const;
    int width() const;
    int height() const;

    bool decode(const ByteArray &bytes, bool allDataReceived);

#if __APPLE__
    WebCoreImageRendererPtr imageRenderer() const { return m_imageRenderer; }
    CGImageRef imageRef() const;
#endif

    void stopAnimations() const;
    void resetAnimation() const;
    void setAnimationRect(const IntRect&) const;

    // Note: These constants exactly match the NSCompositeOperator constants of AppKit.
    enum CompositeOperator {
        CompositeClear,
        CompositeCopy,
        CompositeSourceOver,
        CompositeSourceIn,
        CompositeSourceOut,
        CompositeSourceAtop,
        CompositeDestinationOver,
        CompositeDestinationIn,
        CompositeDestinationOut,
        CompositeDestinationAtop,
        CompositeXOR,
        CompositePlusDarker,
        CompositeHighlight,
        CompositePlusLighter
    };

    static CompositeOperator compositeOperatorFromString(const QString& compositeOperator);

private:
    // We do not allow images to be assigned to or copied.
    Image(const Image&);
    Image &operator=(const Image&);

#if __APPLE__
    WebCoreImageRendererPtr m_imageRenderer;
#endif

    friend class QPainter;
};

}

#endif
