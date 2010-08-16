/*
 * Copyright (C) 2010 Sencha, Inc.
 *
 * All rights reserved.
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

#ifndef ContextShadow_h
#define ContextShadow_h

#include <QPainter>

namespace WebCore {

// This is to track and keep the shadow state. We use this rather than
// using GraphicsContextState to allow possible optimizations (right now
// only to determine the shadow type, but in future it might covers things
// like cached scratch image, persistent shader, etc).

// This class should be copyable since GraphicsContextQt keeps a stack of
// the shadow state for savePlatformState and restorePlatformState.

class ContextShadow {
public:
    enum {
        NoShadow,
        OpaqueSolidShadow,
        AlphaSolidShadow,
        BlurShadow
    } type;

    QColor color;
    int blurRadius;
    QPointF offset;

    ContextShadow();
    ContextShadow(const QColor& c, float r, qreal dx, qreal dy);

    void clear();

    // Draws the shadow for colored rectangle (can't be filled with pattern
    // or gradient) according to the shadow parameters.
    // Note: 'rect' specifies the rectangle which casts the shadow,
    // NOT the bounding box of the shadow.
    void drawShadowRect(QPainter* p, const QRectF& rect);

};

} // namespace WebCore

#endif // ContextShadow_h
