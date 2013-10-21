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

#if USE(ACCELERATED_COMPOSITING)

#include "ShareableBitmap.h"
#include <WebCore/FloatRect.h>
#include <WebCore/Region.h>

namespace WebKit {

class PlatformCALayerRemote;

class RemoteLayerBackingStore {
public:
    RemoteLayerBackingStore();
    RemoteLayerBackingStore(PlatformCALayerRemote*, WebCore::IntSize, float scale);

    void setNeedsDisplay(const WebCore::IntRect);
    void setNeedsDisplay();

    bool display();

    ShareableBitmap* bitmap() const { return m_frontBuffer.get(); }
    WebCore::IntSize size() const { return m_size; }
    float scale() const { return m_scale; }

    PlatformCALayerRemote* layer() const { return m_layer; }

    void encode(CoreIPC::ArgumentEncoder&) const;
    static bool decode(CoreIPC::ArgumentDecoder&, RemoteLayerBackingStore&);

private:
    WebCore::IntRect mapToContentCoordinates(const WebCore::IntRect) const;

    PlatformCALayerRemote* m_layer;

    WebCore::IntSize m_size;
    float m_scale;

    WebCore::Region m_dirtyRegion;

    RefPtr<ShareableBitmap> m_frontBuffer;
    RefPtr<ShareableBitmap> m_backBuffer;
};

} // namespace WebKit

#endif // USE(ACCELERATED_COMPOSITING)

#endif // RemoteLayerBackingStore_h
