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
#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

static inline QImage createQImage(void* data, int width, int height)
{
    return QImage(reinterpret_cast<uchar*>(data), width, height, width * 4, QImage::Format_RGB32);
}

PassOwnPtr<GraphicsContext> ShareableBitmap::createGraphicsContext()
{
    // FIXME: Should this be OwnPtr<QImage>?
    QImage* image = new QImage(createQImage(data(), m_size.width(), m_size.height()));
    OwnPtr<GraphicsContext> context = adoptPtr(new GraphicsContext(new QPainter(image)));
    context->takeOwnershipOfPlatformContext();
    return context.release();
}

void ShareableBitmap::paint(GraphicsContext& context, const IntPoint& dstPoint, const IntRect& srcRect)
{
    QImage image = createQImage(data(), m_size.width(), m_size.height());
    QPainter* painter = context.platformContext();
    painter->drawImage(dstPoint, image, QRect(srcRect));
}

} // namespace WebKit
