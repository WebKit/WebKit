/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ShareableBitmap.h"

#include <QImage>
#include <QPainter>
#include <WebCore/BitmapImage.h>
#include <WebCore/GraphicsContext.h>
#include <WebCore/NotImplemented.h>

using namespace WebCore;

namespace WebKit {

QImage ShareableBitmap::createQImage()
{
    return QImage(reinterpret_cast<uchar*>(data()), m_size.width(), m_size.height(), m_size.width() * 4,
                  m_flags & SupportsAlpha ? QImage::Format_ARGB32_Premultiplied : QImage::Format_RGB32);
}

PassRefPtr<Image> ShareableBitmap::createImage()
{
    QPixmap* pixmap = new QPixmap(QPixmap::fromImage(createQImage()));
    return BitmapImage::create(pixmap);
}

PassOwnPtr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    // FIXME: Should this be OwnPtr<QImage>?
    QImage* image = new QImage(createQImage());
    OwnPtr<GraphicsContext> context = adoptPtr(new GraphicsContext(new QPainter(image)));
    context->takeOwnershipOfPlatformContext();
    return context.release();
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    QImage image = createQImage();
    QPainter* painter = context.platformContext();
    painter->drawImage(dstPoint, image, QRect(srcRect));
}

void ShareableBitmap::paint(GraphicsContext& /*context*/, float /*scaleFactor*/, const IntPoint& /*dstPoint*/, const IntRect& /*srcRect*/)
{
    // See <https://bugs.webkit.org/show_bug.cgi?id=64663>.
    notImplemented();
}

void ShareableBitmap::swizzleRGB()
{
    uint32_t* data = reinterpret_cast<uint32_t*>(this->data());
    int width = size().width();
    int height = size().height();
    for (int y = 0; y < height; ++y) {
        uint32_t* p = data + y * width;
        for (int x = 0; x < width; ++x)
            p[x] = ((p[x] << 16) & 0xff0000) | ((p[x] >> 16) & 0xff) | (p[x] & 0xff00ff00);
    }
}

}
// namespace WebKit
