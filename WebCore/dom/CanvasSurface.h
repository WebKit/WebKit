/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#ifndef CanvasSurface_h
#define CanvasSurface_h

#include "AffineTransform.h"
#include "IntSize.h"

#include <wtf/OwnPtr.h>
#include <wtf/Noncopyable.h>

namespace WebCore {

class AffineTransform;
class FloatPoint;
class FloatRect;
class FloatSize;
class GraphicsContext;
class ImageBuffer;
class IntPoint;
class String;

class CSSStyleSelector;
class RenderBox;
class RenderStyle;
class SecurityOrigin;

typedef int ExceptionCode;

class CanvasSurface : public Noncopyable {
public:
    CanvasSurface(float pageScaleFactor);
    virtual ~CanvasSurface();

    int width() const { return m_size.width(); }
    int height() const { return m_size.height(); }

    String toDataURL(const String& mimeType, ExceptionCode&);

    const IntSize& size() const { return m_size; }

    virtual void willDraw(const FloatRect&);

    GraphicsContext* drawingContext() const;

    ImageBuffer* buffer() const;

    IntRect convertLogicalToDevice(const FloatRect&) const;
    IntSize convertLogicalToDevice(const FloatSize&) const;
    IntPoint convertLogicalToDevice(const FloatPoint&) const;

    void setOriginTainted() { m_originClean = false; }
    bool originClean() const { return m_originClean; }

    AffineTransform baseTransform() const;

    const SecurityOrigin& securityOrigin() const;
    RenderBox* renderBox() const;
    RenderStyle* computedStyle();
    CSSStyleSelector* styleSelector();

protected:
    void setSurfaceSize(const IntSize&);
    bool hasCreatedImageBuffer() const { return m_hasCreatedImageBuffer; }

    static const int DefaultWidth;
    static const int DefaultHeight;

private:
    void createImageBuffer() const;

    static const float MaxCanvasArea;

    IntSize m_size;

    float m_pageScaleFactor;
    bool m_originClean;

    // m_createdImageBuffer means we tried to malloc the buffer.  We didn't necessarily get it.
    mutable bool m_hasCreatedImageBuffer;
    mutable OwnPtr<ImageBuffer> m_imageBuffer;
};

} // namespace WebCore

#endif
