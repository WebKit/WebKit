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

#import "config.h"
#import "RemoteLayerBackingStore.h"

#if USE(ACCELERATED_COMPOSITING)

#import "PlatformCALayerRemote.h"
#import "ArgumentCoders.h"
#import "ShareableBitmap.h"
#import "WebCoreArgumentCoders.h"

using namespace WebCore;
using namespace WebKit;

RemoteLayerBackingStore::RemoteLayerBackingStore(PlatformCALayerRemote* layer, IntSize size, bool isOpaque)
    : m_layer(layer)
    , m_size(size)
    , m_isOpaque(isOpaque)
    , m_needsFullRepaint(true)
{
    ASSERT(layer);
}

RemoteLayerBackingStore::RemoteLayerBackingStore()
    : m_layer(nullptr)
{
}

void RemoteLayerBackingStore::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    ShareableBitmap::Handle handle;
    m_bitmap->createHandle(handle);

    encoder << handle;
    encoder << m_size;
}

bool RemoteLayerBackingStore::decode(CoreIPC::ArgumentDecoder& decoder, RemoteLayerBackingStore& result)
{
    ShareableBitmap::Handle handle;
    if (!decoder.decode(handle))
        return false;
    result.m_bitmap = ShareableBitmap::create(handle);

    if (!decoder.decode(result.m_size))
        return false;

    return true;
}

void RemoteLayerBackingStore::setNeedsDisplay(IntRect rect)
{
    // FIXME: Only repaint dirty regions.
    setNeedsDisplay();
}

void RemoteLayerBackingStore::setNeedsDisplay()
{
    m_needsFullRepaint = true;
}

bool RemoteLayerBackingStore::display()
{
    if (!m_layer)
        return false;

    if (!m_layer->owner()->platformCALayerDrawsContent()) {
        // If we previously were drawsContent=YES, and now are not, we need
        // to note that our backing store has changed (by being cleared).
        if (m_bitmap) {
            m_bitmap = nullptr;
            return true;
        }
        return false;
    }

    if (m_bitmap && !m_needsFullRepaint)
        return false;

    if (!m_bitmap) {
        m_bitmap = ShareableBitmap::createShareable(m_size, m_isOpaque ? ShareableBitmap::NoFlags : ShareableBitmap::SupportsAlpha);
        m_needsFullRepaint = true;
    }

    std::unique_ptr<GraphicsContext> context = m_bitmap->createGraphicsContext();
    m_layer->owner()->platformCALayerPaintContents(*context.get(), IntRect(IntPoint(), m_size));

    m_needsFullRepaint = false;

    return true;
}

#endif // USE(ACCELERATED_COMPOSITING)
