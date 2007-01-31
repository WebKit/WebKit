/*
 * Copyright (C) 2006 Nikolas Zimmermann <zimmermann@kde.org>
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
#include "ImageBuffer.h"

#include "GraphicsContext.h"

#include <QPainter>
#include <QPixmap>

namespace WebCore {

std::auto_ptr<ImageBuffer> ImageBuffer::create(const IntSize& size, bool grayScale)
{
    QPixmap px(size);
    return std::auto_ptr<ImageBuffer>(new ImageBuffer(px));
}

ImageBuffer::ImageBuffer(const QPixmap& px)
    : m_pixmap(px),
      m_painter(0)
{
    m_painter = new QPainter(&m_pixmap);
    m_context.set(new GraphicsContext(m_painter));
}

ImageBuffer::~ImageBuffer()
{
    delete m_painter;
}

GraphicsContext* ImageBuffer::context() const
{
    if (!m_painter->isActive())
        m_painter->begin(&m_pixmap);

    return m_context.get();
}

QPixmap* ImageBuffer::pixmap() const
{
    if (!m_painter)
        return &m_pixmap;
    if (m_painter->isActive())
        m_painter->end();
    return &m_pixmap;
}

}
