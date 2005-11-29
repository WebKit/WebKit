/*
 * Copyright (C) 2005 Apple Computer, Inc.  All rights reserved.
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


#import "kcanvas/RenderPath.h"

class KCanvasItemQuartz : public RenderPath {
public:
    KCanvasItemQuartz(khtml::RenderStyle *style, KSVG::SVGStyledElementImpl *node);
    virtual ~KCanvasItemQuartz() { }
    
    virtual QRect bboxForPath(bool includeStroke) const;
    virtual bool hitsPath(const QPoint &p, bool fill /* false means stroke */) const;
    
    virtual QRect getAbsoluteRepaintRect() { return absoluteTransform().mapRect(relativeBBox(true)); }
    
    virtual bool requiresLayer() { return false; }
    virtual void layout() { setNeedsLayout(false); }
    virtual void paint(PaintInfo &paintInfo, int parentX, int parentY);
    virtual bool nodeAtPoint(NodeInfo& info, int _x, int _y, int _tx, int _ty,
                            HitTestAction hitTestAction);
private:
    void drawMarkersIfNeeded(const QRect &rect, CGPathRef path) const;
};
