/*
 * Copyright (C) 2006 Friedemann Kleint <fkleint@trolltech.com>
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#ifndef ImageDecoderQt_h
#define ImageDecoderQt_h

#include "ImageDecoder.h"
#include <QtGui/QImage>
#include <QtGui/QPixmap>
#include <QtCore/QList>
#include <QtCore/QHash>

namespace WebCore {


class ImageDecoderQt : public ImageDecoder
{
public:
    static ImageDecoderQt* create(const SharedBuffer& data);
    ~ImageDecoderQt();

    typedef Vector<char> IncomingData;

    virtual void setData(const IncomingData& data, bool allDataReceived);
    virtual bool isSizeAvailable();
    virtual int frameCount() const;
    virtual int repetitionCount() const;
    virtual RGBA32Buffer* frameBufferAtIndex(size_t index);

    QPixmap* imageAtIndex(size_t index) const;
    virtual bool supportsAlpha() const;
    int duration(size_t index) const;
    virtual String filenameExtension() const;

    void clearFrame(size_t index);

private:
    ImageDecoderQt(const QString &imageFormat);
    ImageDecoderQt(const ImageDecoderQt&);
    ImageDecoderQt &operator=(const ImageDecoderQt&);

    class ReadContext;
    void reset();
    bool hasFirstImageHeader() const;

    enum ImageState {
        // Started image reading
        ImagePartial,
            // Header (size / alpha) are known
            ImageHeaderValid,
            // Image is complete
            ImageComplete };

    struct ImageData {
        ImageData(const QImage& image, ImageState imageState = ImagePartial, int duration=0);
        QImage m_image;
        ImageState m_imageState;
        int m_duration;
    };

    bool m_hasAlphaChannel;
    typedef QList<ImageData> ImageList;
    mutable ImageList m_imageList;
    mutable QHash<int, QPixmap> m_pixmapCache;
    int m_loopCount;
    QString m_imageFormat;
};



}

#endif

