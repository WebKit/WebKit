/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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
#include "WebCodecsVideoFrame.h"

#if ENABLE(WEB_CODECS)

#include "DOMRectReadOnly.h"
#include "ExceptionOr.h"

namespace WebCore {

WebCodecsVideoFrame::~WebCodecsVideoFrame()
{
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(CanvasImageSource&&, Init&&)
{
    return adoptRef(*new WebCodecsVideoFrame);
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::create(BufferSource&&, BufferInit&&)
{
    return adoptRef(*new WebCodecsVideoFrame);
}

Ref<WebCodecsVideoFrame> WebCodecsVideoFrame::create(Ref<VideoFrame>&&, BufferInit&&)
{
    return adoptRef(*new WebCodecsVideoFrame);
}

ExceptionOr<size_t> WebCodecsVideoFrame::allocationSize(CopyToOptions&&)
{
    // FIXME: Implement this.
    return 0;
}

void WebCodecsVideoFrame::copyTo(BufferSource&&, CopyToOptions&&, CopyToPromise&&)
{
    // FIXME: Implement this.
}

ExceptionOr<Ref<WebCodecsVideoFrame>> WebCodecsVideoFrame::clone()
{
    if (isDetached())
        return Exception { InvalidStateError,  "VideoFrame is detached"_s };

    auto clone = adoptRef(*new WebCodecsVideoFrame);
    clone->m_internalFrame = m_internalFrame;
    clone->m_isDetached = m_isDetached;
    clone->m_format = m_format;
    clone->m_codedWidth = m_codedWidth;
    clone->m_codedHeight = m_codedHeight;
    clone->m_codedRect = m_codedRect;
    clone->m_displayWidth = m_displayWidth;
    clone->m_displayHeight = m_displayHeight;
    clone->m_visibleWidth = m_visibleWidth;
    clone->m_visibleHeight = m_visibleHeight;
    clone->m_visibleLeft = m_visibleLeft;
    clone->m_visibleTop = m_visibleTop;
    clone->m_visibleRect = m_visibleRect;
    clone->m_duration = m_duration;
    clone->m_timestamp = m_timestamp;
    clone->m_colorSpace = m_colorSpace;
    return clone;
}

// https://w3c.github.io/webcodecs/#close-videoframe
void WebCodecsVideoFrame::close()
{
    m_internalFrame = nullptr;
    m_isDetached = true;
    m_format = { };
    m_codedWidth = 0;
    m_codedHeight = 0;
    m_codedRect = nullptr;
    m_displayWidth = 0;
    m_displayHeight = 0;
    m_visibleWidth = 0;
    m_visibleHeight = 0;
    m_visibleLeft = 0;
    m_visibleTop = 0;
    m_visibleRect = nullptr;
    m_duration = { };
    m_timestamp = 0;
    m_colorSpace = nullptr;
}

DOMRectReadOnly* WebCodecsVideoFrame::codedRect() const
{
    if (m_isDetached)
        return nullptr;
    if (!m_codedRect)
        m_codedRect = DOMRectReadOnly::create(0, 0, m_codedWidth, m_codedHeight);

    return m_codedRect.get();
}

DOMRectReadOnly* WebCodecsVideoFrame::visibleRect() const
{
    if (m_isDetached)
        return nullptr;
    if (!m_visibleRect)
        m_visibleRect = DOMRectReadOnly::create(m_visibleLeft, m_visibleTop, m_visibleWidth, m_visibleHeight);

    return m_visibleRect.get();
}

} // namespace WebCore

#endif // ENABLE(WEB_CODECS)
