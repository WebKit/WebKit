/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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

#ifndef RemoteLayerBackingStore_h
#define RemoteLayerBackingStore_h

#include "ShareableBitmap.h"
#include <WebCore/FloatRect.h>
#include <WebCore/Region.h>

#if USE(IOSURFACE)
#include <IOSurface/IOSurface.h>
#endif

// FIXME: Make PlatformCALayerRemote.cpp Objective-C so we can include WebLayer.h here and share the typedef.
namespace WebCore {
typedef Vector<WebCore::FloatRect, 5> RepaintRectList;
}

namespace WebKit {

class PlatformCALayerRemote;

class RemoteLayerBackingStore {
public:
    RemoteLayerBackingStore();

    void ensureBackingStore(PlatformCALayerRemote*, WebCore::IntSize, float scale, bool acceleratesDrawing);

    void setNeedsDisplay(const WebCore::IntRect);
    void setNeedsDisplay();

    bool display();

    RetainPtr<CGImageRef> image() const;
#if USE(IOSURFACE)
    RetainPtr<IOSurfaceRef> surface() const { return m_frontSurface; }
#endif
    WebCore::IntSize size() const { return m_size; }
    float scale() const { return m_scale; }
    bool acceleratesDrawing() const { return m_acceleratesDrawing; }

    PlatformCALayerRemote* layer() const { return m_layer; }

    void encode(IPC::ArgumentEncoder&) const;
    static bool decode(IPC::ArgumentDecoder&, RemoteLayerBackingStore&);

    void enumerateRectsBeingDrawn(CGContextRef, void (^)(CGRect));

private:
    bool hasFrontBuffer()
    {
#if USE(IOSURFACE)
        if (m_acceleratesDrawing)
            return !!m_frontSurface;
#endif
        return !!m_frontBuffer;
    }

    void drawInContext(WebCore::GraphicsContext&, CGImageRef frontImage);

    PlatformCALayerRemote* m_layer;

    WebCore::IntSize m_size;
    float m_scale;

    WebCore::Region m_dirtyRegion;

    RefPtr<ShareableBitmap> m_frontBuffer;
#if USE(IOSURFACE)
    RetainPtr<IOSurfaceRef> m_frontSurface;
#endif

    bool m_acceleratesDrawing;

    WebCore::RepaintRectList m_paintingRects;
};

} // namespace WebKit

#endif // RemoteLayerBackingStore_h
